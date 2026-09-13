#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <cpl_conv.h>
#include "core/TopographicArchive.h"

namespace {
QByteArray contents(const QString& path) {
  QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
  return file.readAll();
}
bool zip(const QString& path, const QList<QPair<QString, QByteArray>>& entries) {
  void* archive = CPLCreateZip(path.toUtf8().constData(), nullptr);
  if (!archive) return false;
  bool ok = true;
  for (const auto& entry : entries) {
    if (CPLCreateFileInZip(archive, entry.first.toUtf8().constData(), nullptr) != CE_None) {
      ok = false; break;
    }
    ok = CPLWriteFileInZip(archive, entry.second.constData(), static_cast<int>(entry.second.size())) == CE_None;
    ok = CPLCloseFileInZip(archive) == CE_None && ok;
    if (!ok) break;
  }
  return CPLCloseZip(archive) == CE_None && ok;
}
}
class TopographicArchiveTest : public QObject {
  Q_OBJECT
private slots:
  void nestedArchiveIsReadOnlyAndReusable() {
    QTemporaryDir temp; QVERIFY(temp.isValid());
    const auto inner = temp.filePath(QStringLiteral("sheet.zip"));
    QVERIFY(zip(inner, {{QStringLiteral("도엽/F0010000.dxf"), QByteArray("contours")}}));
    const auto outer = temp.filePath(QStringLiteral("download.zip"));
    QVERIFY(zip(outer, {{QStringLiteral("nested/sheet.zip"), contents(inner)},
                        {QStringLiteral("license.txt"), QByteArray("license")}}));
    const auto original = contents(outer);
    const auto library = temp.filePath(QStringLiteral("library"));
    const auto first = TopographicArchive::prepare(outer, library);
    QVERIFY2(first.error.isEmpty(), qPrintable(first.error)); QVERIFY(!first.canceled);
    QVERIFY(!first.reused); QCOMPARE(first.files.size(), 2);
    QVERIFY(QFileInfo::exists(first.directory));
    bool found = false;
    for (const auto& file : first.files) if (file.endsWith(QStringLiteral(".dxf"))) {
      QCOMPARE(contents(file), QByteArray("contours")); found = true;
    }
    QVERIFY(found); QCOMPARE(contents(outer), original);
    const auto again = TopographicArchive::prepare(outer, library);
    QVERIFY2(again.error.isEmpty(), qPrintable(again.error)); QVERIFY(again.reused);
    QCOMPARE(again.directory, first.directory); QCOMPARE(again.files, first.files);
    QFile corrupted(first.files.first()); QVERIFY(corrupted.open(QIODevice::WriteOnly));
    QVERIFY(corrupted.write("corrupted") > 0); corrupted.close();
    const auto repaired = TopographicArchive::prepare(outer, library);
    QVERIFY2(repaired.error.isEmpty(), qPrintable(repaired.error)); QVERIFY(!repaired.reused);
    QCOMPARE(contents(outer), original);
    const auto verified = TopographicArchive::prepare(outer, library);
    QVERIFY(verified.reused);
  }
  void rejectsUnsafePaths_data() {
    QTest::addColumn<QString>("name");
    for (const auto& name : {"../escape.dxf", "safe/../../escape.dxf", "/escape.dxf",
                            "C:/escape.dxf", "safe\\..\\escape.dxf", "NUL.dxf",
                            "safe/CON/file.dxf", "safe/file.dxf:stream", "safe./file.dxf"})
      QTest::newRow(name) << QString::fromLatin1(name);
  }
  void rejectsUnsafePaths() {
    QFETCH(QString, name);
    QTemporaryDir temp; const auto source = temp.filePath(QStringLiteral("unsafe.zip"));
    QVERIFY(zip(source, {{name, QByteArray("data")}}));
    const auto result = TopographicArchive::prepare(source, temp.filePath(QStringLiteral("library")));
    QVERIFY(!result.error.isEmpty()); QVERIFY(result.directory.isEmpty()); QVERIFY(result.files.isEmpty());
    QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("escape.dxf"))));
  }
  void rejectsResourceLimitsAndCancellation() {
    QTemporaryDir temp;
    const auto source = temp.filePath(QStringLiteral("large.zip"));
    QVERIFY(zip(source, {{QStringLiteral("a.dxf"), QByteArray(32768, 'a')},
                         {QStringLiteral("b.dxf"), QByteArray(32768, 'b')}}));
    const auto library = temp.filePath(QStringLiteral("library"));
    TopographicArchive::Limits limits; limits.maxFileBytes = 100;
    QVERIFY(!TopographicArchive::prepare(source, library, limits).error.isEmpty());
    limits = {}; limits.maxTotalBytes = 40000;
    QVERIFY(!TopographicArchive::prepare(source, library, limits).error.isEmpty());
    limits = {}; limits.maxEntries = 1;
    QVERIFY(!TopographicArchive::prepare(source, library, limits).error.isEmpty());
    int checks = 0;
    const auto canceled = TopographicArchive::prepare(source, library, {}, [&] { return ++checks > 3; });
    QVERIFY(canceled.canceled); QVERIFY(canceled.directory.isEmpty()); QVERIFY(canceled.files.isEmpty());
    const auto retry = TopographicArchive::prepare(source, library);
    QVERIFY2(retry.error.isEmpty(), qPrintable(retry.error));
    const auto lateCancel = TopographicArchive::prepare(source, library, {}, [] { return true; });
    QVERIFY(lateCancel.canceled); QVERIFY(QFileInfo::exists(retry.directory));
  }
  void rawVectorReturnsOriginalAndUnsupportedFails() {
    QTemporaryDir temp; const auto source = temp.filePath(QStringLiteral("map.dxf"));
    QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("test"); file.close();
    const auto result = TopographicArchive::prepare(source, temp.filePath(QStringLiteral("library")));
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(result.files, QStringList{source});
    QCOMPARE(contents(source), QByteArray("test"));
    QVERIFY(!TopographicArchive::prepare(temp.filePath(QStringLiteral("absent.zip")), temp.path()).error.isEmpty());
  }
  void rejectsNestedDepthAndCaseCollisions() {
    QTemporaryDir temp;
    const auto inner = temp.filePath(QStringLiteral("inner.zip"));
    const auto outer = temp.filePath(QStringLiteral("outer.zip"));
    QVERIFY(zip(inner, {{QStringLiteral("map.dxf"), QByteArray("map")}}));
    QVERIFY(zip(outer, {{QStringLiteral("inner.zip"), contents(inner)}}));
    TopographicArchive::Limits limits; limits.maxArchiveDepth = 1;
    QVERIFY(!TopographicArchive::prepare(outer, temp.filePath(QStringLiteral("library")), limits).error.isEmpty());
    const auto collision = temp.filePath(QStringLiteral("collision.zip"));
    QVERIFY(zip(collision, {{QStringLiteral("Map.dxf"), QByteArray("a")},
                           {QStringLiteral("Nap.dxf"), QByteArray("b")}}));
    auto duplicateBytes = contents(collision); duplicateBytes.replace("Nap.dxf", "map.dxf");
    QFile duplicate(collision); QVERIFY(duplicate.open(QIODevice::WriteOnly));
    QCOMPARE(duplicate.write(duplicateBytes), duplicateBytes.size()); duplicate.close();
    QVERIFY(!TopographicArchive::prepare(collision, temp.filePath(QStringLiteral("library"))).error.isEmpty());
  }
  void archiveLinkNeverCreatesFilesystemLink() {
    QTemporaryDir temp;
    const auto source = temp.filePath(QStringLiteral("link.zip"));
    QVERIFY(zip(source, {{QStringLiteral("link.dxf"), QByteArray("../../outside.dxf")}}));
    auto bytes = contents(source);
    const auto central = bytes.indexOf(QByteArray::fromHex("504b0102")); QVERIFY(central >= 0);
    // Fixture only: mark the entry as a Unix symbolic link in ZIP metadata.
    bytes[central + 5] = 3;
    qToLittleEndian<quint32>(0120777U << 16, bytes.data() + central + 38);
    QFile archive(source); QVERIFY(archive.open(QIODevice::WriteOnly));
    QCOMPARE(archive.write(bytes), bytes.size()); archive.close();
    const auto result = TopographicArchive::prepare(source, temp.filePath(QStringLiteral("library")));
    if (result.error.isEmpty()) {
      QCOMPARE(result.files.size(), 1);
      QVERIFY(!QFileInfo(result.files.first()).isSymbolicLink());
      QVERIFY(!QFileInfo(result.files.first()).isJunction());
      QCOMPARE(contents(result.files.first()), QByteArray("../../outside.dxf"));
    } else {
      QVERIFY(result.directory.isEmpty());
    }
    QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("outside.dxf"))));
  }
};
QTEST_GUILESS_MAIN(TopographicArchiveTest)
#include "test_topographic_archive.moc"
