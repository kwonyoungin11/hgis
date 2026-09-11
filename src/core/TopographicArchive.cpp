#include "TopographicArchive.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>
#include <QUuid>
#include <cpl_vsi.h>
#include <memory>

namespace {
using namespace TopographicArchive;
constexpr qsizetype ChunkSize = 256 * 1024;
struct Work {
  Limits limits;
  const std::function<bool()>& cancel;
  Result result;
  quint64 total = 0;
  int entries = 0;
  QSet<QString> files;
  QSet<QString> directories;
  QJsonArray manifest;
  bool stopped() {
    if (cancel && cancel()) result.canceled = true;
    return result.canceled || !result.error.isEmpty();
  }
  bool fail(const QString& message) { result.error = message; return false; }
};
bool safeRelative(const QString& name) {
  if (name.isEmpty() || name.size() > 2048 || name.contains(QChar::ReplacementCharacter)
      || name.contains(QLatin1Char('\\')) || QDir::isAbsolutePath(name)) return false;
  const auto parts = name.split(QLatin1Char('/'));
  if (parts.size() > 64) return false;
  static const QRegularExpression device(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)"),
                                          QRegularExpression::CaseInsensitiveOption);
  for (const auto& part : parts) {
    if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")
        || part.endsWith(QLatin1Char('.')) || part.endsWith(QLatin1Char(' '))
        || device.match(part).hasMatch()) return false;
    for (const auto ch : part)
      if (ch.unicode() < 32 || QStringLiteral(":*?\"<>|").contains(ch)) return false;
  }
  return true;
}
bool noLinks(const QString& path) {
  auto cursor = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  while (!cursor.isEmpty()) {
    const QFileInfo info(cursor);
    if (info.isSymbolicLink() || info.isJunction()) return false;
    const auto parent = info.absolutePath();
    if (parent == cursor) break;
    cursor = parent;
  }
  return true;
}
QByteArray fileHash(const QString& path, Work& work) {
  if (work.stopped()) return {};
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) { work.fail(QStringLiteral("자료 파일을 읽지 못했습니다: %1").arg(file.errorString())); return {}; }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  QByteArray buffer(ChunkSize, Qt::Uninitialized);
  while (!file.atEnd()) {
    if (work.stopped()) return {};
    const auto read = file.read(buffer.data(), buffer.size());
    if (read <= 0) { work.fail(QStringLiteral("자료 파일을 끝까지 읽지 못했습니다.")); return {}; }
    hash.addData(QByteArrayView(buffer.constData(), read));
  }
  return hash.result().toHex();
}
bool registerPath(const QString& relative, bool directory, Work& work) {
  const auto key = relative.toCaseFolded();
  auto parent = relative.section(QLatin1Char('/'), 0, -2);
  if (!relative.contains(QLatin1Char('/'))) parent.clear();
  while (!parent.isEmpty()) {
    const auto parentKey = parent.toCaseFolded();
    if (work.files.contains(parentKey)) return work.fail(QStringLiteral("압축 자료의 파일 경로가 서로 충돌합니다."));
    work.directories.insert(parentKey);
    if (!parent.contains(QLatin1Char('/'))) break;
    parent = parent.section(QLatin1Char('/'), 0, -2);
  }
  if (work.files.contains(key) || (!directory && work.directories.contains(key)))
    return work.fail(QStringLiteral("압축 자료에 중복된 파일 경로가 있습니다."));
  if (directory) work.directories.insert(key); else work.files.insert(key);
  return true;
}
bool extract(const QString& archive, const QString& payload, const QString& scratch,
             const QString& prefix, int depth, Work& work) {
  if (work.stopped()) return false;
  if (depth > work.limits.maxArchiveDepth) return work.fail(QStringLiteral("압축 자료의 중첩 깊이가 허용 범위를 넘었습니다."));
  const auto vsiRoot = QStringLiteral("/vsizip/{%1}").arg(QDir::fromNativeSeparators(archive));
  std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> dir(VSIOpenDir(vsiRoot.toUtf8().constData(), -1, nullptr), VSICloseDir);
  if (!dir) return work.fail(QStringLiteral("ZIP 자료를 열지 못했습니다. 다운로드 완료 여부를 확인하세요."));
  bool found = false;
  while (const auto* entry = VSIGetNextDirEntry(dir.get())) {
    if (work.stopped()) return false;
    found = true;
    if (++work.entries > work.limits.maxEntries) return work.fail(QStringLiteral("압축 자료의 파일 수가 허용 범위를 넘었습니다."));
    QString name = QString::fromUtf8(entry->pszName);
    const bool isDirectory = entry->bModeKnown && VSI_ISDIR(entry->nMode);
    if (isDirectory && name.endsWith(QLatin1Char('/'))) name.chop(1);
    if (!safeRelative(name)) return work.fail(QStringLiteral("압축 자료에 안전하지 않은 파일 경로가 있습니다."));
    const auto relative = prefix + name;
    if (!safeRelative(relative)) return work.fail(QStringLiteral("압축 자료의 경로가 너무 길거나 안전하지 않습니다."));
    if (!registerPath(relative, isDirectory, work)) return false;
    if (isDirectory) continue;
    // Never restore archive permissions or links. All entry contents are copied
    // into newly created regular files; link targets are never followed.
    if (entry->bModeKnown && !VSI_ISREG(entry->nMode))
      return work.fail(QStringLiteral("압축 자료에 지원하지 않는 링크 또는 특수 파일이 있습니다."));
    if (!entry->bSizeKnown || entry->nSize > work.limits.maxFileBytes
        || entry->nSize > work.limits.maxTotalBytes - work.total)
      return work.fail(QStringLiteral("압축 해제 크기가 허용 범위를 넘었습니다."));
    const quint64 expected = entry->nSize;
    const auto vsiPath = (vsiRoot + QLatin1Char('/') + name).toUtf8();
    std::unique_ptr<VSILFILE, decltype(&VSIFCloseL)> input(VSIFOpenL(vsiPath.constData(), "rb"), VSIFCloseL);
    if (!input) return work.fail(QStringLiteral("ZIP 안의 파일을 읽지 못했습니다."));
    const bool nested = name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive);
    const auto target = nested ? QDir(scratch).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".zip"))
                               : QDir(payload).filePath(relative);
    if (!noLinks(target) || !QDir().mkpath(QFileInfo(target).absolutePath()))
      return work.fail(QStringLiteral("자료를 보관할 안전한 폴더를 만들지 못했습니다."));
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly)) return work.fail(QStringLiteral("압축 자료를 저장하지 못했습니다: %1").arg(output.errorString()));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(ChunkSize, Qt::Uninitialized);
    quint64 copied = 0;
    for (;;) {
      if (work.stopped()) return false;
      const size_t count = VSIFReadL(buffer.data(), 1, static_cast<size_t>(buffer.size()), input.get());
      if (count == 0) break;
      if (count > expected - copied || count > work.limits.maxTotalBytes - work.total)
        return work.fail(QStringLiteral("압축 자료의 실제 크기가 허용 범위를 넘었습니다."));
      copied += count; work.total += count;
      if (output.write(buffer.constData(), static_cast<qint64>(count)) != static_cast<qint64>(count))
        return work.fail(QStringLiteral("압축 자료 저장 중 오류가 발생했습니다: %1").arg(output.errorString()));
      hash.addData(QByteArrayView(buffer.constData(), static_cast<qsizetype>(count)));
    }
    if (VSIFErrorL(input.get()) || copied != expected)
      return work.fail(QStringLiteral("압축 자료가 손상되었거나 다운로드가 끝나지 않았습니다."));
    if (VSIFCloseL(input.release()) != 0) return work.fail(QStringLiteral("ZIP 파일 읽기를 완료하지 못했습니다."));
    if (!output.commit()) return work.fail(QStringLiteral("압축 자료 저장을 완료하지 못했습니다: %1").arg(output.errorString()));
    if (nested) {
      if (!extract(target, payload, scratch, relative + QStringLiteral(".contents/"), depth + 1, work)) return false;
    } else {
      work.manifest.append(QJsonObject{{QStringLiteral("path"), relative},
                                      {QStringLiteral("size"), QString::number(copied)},
                                      {QStringLiteral("sha256"), QString::fromLatin1(hash.result().toHex())}});
    }
  }
  return found || work.fail(QStringLiteral("ZIP 자료에 읽을 수 있는 파일이 없습니다."));
}
bool cached(const QString& directory, const QByteArray& sourceHash, Work& work) {
  const auto manifestPath = QDir(directory).filePath(QStringLiteral("manifest.json"));
  if (!noLinks(manifestPath)) return false;
  QFile file(manifestPath);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) return false;
  const auto object = QJsonDocument::fromJson(file.readAll()).object();
  if (object.value(QStringLiteral("version")).toInt() != 1
      || object.value(QStringLiteral("sourceSha256")).toString().toLatin1() != sourceHash) return false;
  const auto files = object.value(QStringLiteral("files")).toArray();
  if (files.isEmpty() || files.size() > work.limits.maxEntries) return false;
  const auto payload = QDir(directory).filePath(QStringLiteral("payload"));
  QSet<QString> expected;
  QStringList paths;
  quint64 bytes = 0;
  for (const auto& value : files) {
    if (work.stopped()) return false;
    const auto item = value.toObject();
    const auto relative = item.value(QStringLiteral("path")).toString();
    bool sizeOk = false;
    const auto size = item.value(QStringLiteral("size")).toString().toULongLong(&sizeOk);
    if (!safeRelative(relative) || !sizeOk || size > work.limits.maxFileBytes || size > work.limits.maxTotalBytes - bytes
        || expected.contains(relative.toCaseFolded())) return false;
    bytes += size;
    const auto path = QDir(payload).filePath(relative);
    const QFileInfo info(path);
    if (!noLinks(path) || !info.isFile() || static_cast<quint64>(info.size()) != size) return false;
    const auto digest = fileHash(path, work);
    if (work.stopped() || digest != item.value(QStringLiteral("sha256")).toString().toLatin1()) return false;
    expected.insert(relative.toCaseFolded()); paths.append(path);
  }
  QDirIterator entries(payload, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
  int count = 0;
  while (entries.hasNext()) {
    if (work.stopped() || ++count > work.limits.maxEntries + 64 * files.size()) return false;
    const auto path = entries.next();
    const auto info = entries.fileInfo();
    if (info.isSymbolicLink() || info.isJunction()
        || (info.isFile() && !expected.contains(QDir(payload).relativeFilePath(path).toCaseFolded()))) return false;
  }
  work.result.directory = payload;
  paths.sort(); work.result.files = paths; work.result.reused = true;
  return true;
}
}

TopographicArchive::Result TopographicArchive::prepare(
    const QString& source, const QString& libraryRoot, const Limits& limits,
    const std::function<bool()>& isCanceled) {
  Work work{limits, isCanceled};
  if (work.stopped()) return work.result;
  const QFileInfo sourceInfo(source);
  if (!sourceInfo.isFile() || !noLinks(sourceInfo.absoluteFilePath())) {
    work.fail(QStringLiteral("완료된 원본 자료 파일을 찾지 못했거나 링크 파일입니다.")); return work.result;
  }
  const auto extension = sourceInfo.suffix().toLower();
  if (extension != QStringLiteral("zip")) {
    if (!QStringList{QStringLiteral("dxf"), QStringLiteral("shp"), QStringLiteral("gpkg"), QStringLiteral("ngi")}.contains(extension))
      work.fail(QStringLiteral("수치지형도 ZIP·SHP·DXF·GPKG·NGI 자료를 선택하세요."));
    else { work.result.directory = sourceInfo.absolutePath(); work.result.files = {sourceInfo.absoluteFilePath()}; }
    return work.result;
  }
  if (!limits.maxTotalBytes || !limits.maxFileBytes || limits.maxEntries <= 0 || limits.maxArchiveDepth <= 0
      || libraryRoot.isEmpty() || !noLinks(libraryRoot) || !QDir().mkpath(libraryRoot)) {
    work.fail(QStringLiteral("자료 보관 경로 또는 압축 해제 제한이 올바르지 않습니다.")); return work.result;
  }
  const auto digest = fileHash(sourceInfo.absoluteFilePath(), work);
  if (work.stopped()) return work.result;
  const auto root = QFileInfo(libraryRoot).canonicalFilePath();
  const auto destination = QDir(root).filePath(QString::fromLatin1(digest));
  QLockFile lock(destination + QStringLiteral(".lock"));
  if (!lock.tryLock(0)) { work.fail(QStringLiteral("같은 자료를 준비 중입니다. 잠시 후 다시 시도하세요.")); return work.result; }
  if (cached(destination, digest, work)) return work.result;
  if (work.stopped()) return work.result;
  if (!noLinks(destination)) { work.fail(QStringLiteral("자료 보관 폴더에 링크가 있어 사용할 수 없습니다.")); return work.result; }
  QTemporaryDir staging(QDir(root).filePath(QStringLiteral(".preparing-XXXXXX")));
  if (!staging.isValid()) { work.fail(QStringLiteral("자료 준비 폴더를 만들지 못했습니다.")); return work.result; }
  const auto payload = staging.filePath(QStringLiteral("payload"));
  const auto scratch = staging.filePath(QStringLiteral("archives"));
  if (!QDir().mkpath(payload) || !QDir().mkpath(scratch)) {
    work.fail(QStringLiteral("자료 준비 폴더를 만들지 못했습니다.")); return work.result;
  }
  if (!extract(sourceInfo.absoluteFilePath(), payload, scratch, {}, 1, work)) return work.result;
  if (fileHash(sourceInfo.absoluteFilePath(), work) != digest && !work.stopped())
    work.fail(QStringLiteral("준비 중 원본 자료가 변경되었습니다. 다시 시도하세요."));
  if (work.stopped()) return work.result;
  if (work.manifest.isEmpty()) { work.fail(QStringLiteral("ZIP 자료에 적재할 파일이 없습니다.")); return work.result; }
  // Intermediate ZIPs are outside payload and are never discovered by the catalog.
  // Staging is exclusively owned here; remove it only after verifying this exact child.
  if (!noLinks(scratch) || !QDir(scratch).removeRecursively()) {
    work.fail(QStringLiteral("중간 압축 자료를 정리하지 못했습니다.")); return work.result;
  }
  QSaveFile manifest(staging.filePath(QStringLiteral("manifest.json")));
  const auto json = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                             {QStringLiteral("sourceSha256"), QString::fromLatin1(digest)},
                                             {QStringLiteral("files"), work.manifest}}).toJson(QJsonDocument::Compact);
  if (!manifest.open(QIODevice::WriteOnly) || manifest.write(json) != json.size() || !manifest.commit()) {
    work.fail(QStringLiteral("자료 완료 정보를 저장하지 못했습니다.")); return work.result;
  }
  if (work.stopped()) return work.result;
  const auto backup = destination + QStringLiteral(".previous-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
  const bool hadPrevious = QFileInfo::exists(destination);
  if (hadPrevious && !QDir().rename(destination, backup)) {
    work.fail(QStringLiteral("이전 자료가 사용 중이어서 갱신하지 못했습니다.")); return work.result;
  }
  if (!QDir().rename(staging.path(), destination)) {
    const bool restored = !hadPrevious || QDir().rename(backup, destination);
    work.fail(restored ? QStringLiteral("자료 준비를 완료하지 못했습니다. 이전 자료는 유지됩니다.")
                       : QStringLiteral("자료 준비를 완료하지 못했습니다. 이전 자료 보관 경로: %1").arg(backup));
    return work.result;
  }
  staging.setAutoRemove(false);
  // Keep previous invalid content for recovery: never delete files that may still
  // be referenced by an open provider. It is outside the returned payload directory.
  work.result.directory = QDir(destination).filePath(QStringLiteral("payload"));
  for (const auto& entry : work.manifest)
    work.result.files.append(QDir(work.result.directory).filePath(entry.toObject().value(QStringLiteral("path")).toString()));
  work.result.files.sort();
  return work.result;
}
