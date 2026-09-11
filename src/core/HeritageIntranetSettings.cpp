#include "HeritageIntranetSettings.h"
#include "KaPortableRuntime.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>

namespace {

QString personalPath() {
  const QString directory = KaPortableRuntime::userConfigDir();
  return directory.isEmpty() ? QString()
                             : QDir(directory).filePath(QStringLiteral("heritage-account.ini"));
}

HeritageIntranetSettings::Credentials readIni(const QString& path) {
  if (!QFileInfo(path).isFile()) return {};
  QSettings settings(path, QSettings::IniFormat);
  settings.setFallbacksEnabled(false);
  settings.sync();
  HeritageIntranetSettings::Credentials result{
      settings.value(QStringLiteral("heritage/username")).toString(),
      settings.value(QStringLiteral("heritage/password")).toString()};
  return settings.status() == QSettings::NoError ? result
                                                 : HeritageIntranetSettings::Credentials{};
}

}  // namespace

HeritageIntranetSettings::Credentials HeritageIntranetSettings::credentials() {
  const QDir app(QCoreApplication::applicationDirPath());
  return readFromFiles(personalPath(), {
      app.filePath(QStringLiteral("config/heritage-local.ini")),
      app.filePath(QStringLiteral("../../config/heritage-local.ini"))});
}

HeritageIntranetSettings::Credentials HeritageIntranetSettings::readFromFiles(
    const QString& personalFile, const QStringList& fallbackFiles) {
  // 존재 여부가 우선순위를 정한다. 빈 값을 저장하면 이 PC의 기본 로그인을 끄는 것이지
  // 번들 계정으로 되돌아가는 것이 아니다. (TopographicSettings 와 같은 규칙)
  if (!personalFile.isEmpty() && QFileInfo::exists(personalFile)) return readIni(personalFile);
  for (const auto& fallback : fallbackFiles)
    if (QFileInfo::exists(fallback)) return readIni(fallback);
  return {};
}

bool HeritageIntranetSettings::hasCredentials() {
  const Credentials c = credentials();
  return !c.username.trimmed().isEmpty() && !c.password.isEmpty();
}

QString HeritageIntranetSettings::describeForLog() {
  const Credentials c = credentials();
  if (c.username.trimmed().isEmpty()) return QStringLiteral("계정 없음");
  // 비밀번호는 길이조차 남기지 않는다.
  return QStringLiteral("계정 %1 · 비밀번호 저장됨").arg(c.username.trimmed());
}

bool HeritageIntranetSettings::saveCredentials(const Credentials& credentials, QString* error) {
  return saveToFile(personalPath(), credentials, error);
}

bool HeritageIntranetSettings::saveToFile(const QString& personalFile,
                                          const Credentials& credentials, QString* error) {
  if (error) error->clear();
  auto fail = [error](const QString& message) {
    if (error) *error = message;
    return false;
  };
  if (personalFile.isEmpty())
    return fail(QStringLiteral("이 PC의 사용자 설정 경로를 찾지 못했습니다."));
  const auto directory = QFileInfo(personalFile).absolutePath();
  if (!QDir().mkpath(directory))
    return fail(QStringLiteral("계정 설정 폴더를 만들지 못했습니다. 폴더 권한을 확인하세요."));
  QTemporaryDir staging(QDir(directory).filePath(QStringLiteral(".heritage-account-XXXXXX")));
  if (!staging.isValid())
    return fail(QStringLiteral("계정 설정을 준비하지 못했습니다. 폴더 권한을 확인하세요."));
  const auto serialized = staging.filePath(QStringLiteral("account.ini"));
  {
    // INI 이스케이프와 UTF-8 처리는 Qt에 맡긴다. 비밀번호에 공백·세미콜론·따옴표·역슬래시가
    // 들어가거나 '@' 로 시작해도 그대로 살아남아야 한다.
    QSettings settings(serialized, QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setValue(QStringLiteral("heritage/username"), credentials.username);
    settings.setValue(QStringLiteral("heritage/password"), credentials.password);
    settings.sync();
    if (settings.status() != QSettings::NoError)
      return fail(QStringLiteral("계정 설정을 기록하지 못했습니다. 저장 공간과 폴더 권한을 확인하세요."));
  }
  QFile input(serialized);
  if (!input.open(QIODevice::ReadOnly))
    return fail(QStringLiteral("준비한 계정 설정을 읽지 못했습니다."));
  const auto data = input.readAll();
  if (input.error() != QFileDevice::NoError)
    return fail(QStringLiteral("준비한 계정 설정을 끝까지 읽지 못했습니다."));
  input.close();
  QSaveFile output(personalFile);
  output.setDirectWriteFallback(false);
  if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
    return fail(QStringLiteral("계정 설정을 저장하지 못했습니다. 이전 설정은 유지됩니다: %1")
                    .arg(output.errorString()));
  return true;
}
