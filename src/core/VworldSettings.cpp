#include "VworldSettings.h"
#include <QSettings>
#include <QByteArray>
#include <QStandardPaths>
#include <QDir>

static QString orgName() { return QStringLiteral("ka-hgis"); }
static QString appName() { return QStringLiteral("ka-hgis"); }
// Single SSOT path. Do not also use "vworld/apiKey" on write paths:
// Windows QSettings (Native + Ini) is case-insensitive, so VWorld/ApiKey == vworld/apiKey.
static QString ssotKey() { return QStringLiteral("VWorld/ApiKey"); }
static QString legacyKey() { return QStringLiteral("vworld/apiKey"); }

static QSettings makeSettings() {
  const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (!base.isEmpty()) {
    QDir().mkpath(base);
    const QString ini = QDir(base).filePath(QStringLiteral("ka-hgis-vworld.ini"));
    return QSettings(ini, QSettings::IniFormat);
  }
  return QSettings(QSettings::IniFormat, QSettings::UserScope, orgName(), appName());
}

static void writeKey(QSettings& settings, const QString& key) {
  // Clear then set: never remove(legacy) after set(ssot) — case-fold would wipe the value.
  settings.remove(ssotKey());
  if (!key.isEmpty())
    settings.setValue(ssotKey(), key);
  settings.sync();
}

QString VworldSettings::loadApiKey() {
  QSettings settings = makeSettings();
  QString key = settings.value(ssotKey()).toString().trimmed();
  if (!key.isEmpty())
    return key;

  // Migrate older NativeFormat org/app installs
  {
    QSettings native(orgName(), appName());
    key = native.value(ssotKey()).toString().trimmed();
    if (key.isEmpty())
      key = native.value(legacyKey()).toString().trimmed();
    if (!key.isEmpty()) {
      writeKey(settings, key);
      writeKey(native, key);
      return key;
    }
  }

  const QByteArray env = qgetenv("VWORLD_API_KEY");
  if (!env.isEmpty()) {
    key = QString::fromUtf8(env).trimmed();
    if (!key.isEmpty()) {
      writeKey(settings, key);
      return key;
    }
  }

  key = settings.value(legacyKey()).toString().trimmed();
  if (key.isEmpty()) {
    QSettings bare;
    key = bare.value(legacyKey()).toString().trimmed();
  }
  if (!key.isEmpty()) {
    writeKey(settings, key);
    return key;
  }
  return {};
}

void VworldSettings::saveApiKey(const QString& key) {
  const QString k = key.trimmed();
  QSettings settings = makeSettings();
  writeKey(settings, k);

  QSettings native(orgName(), appName());
  writeKey(native, k);
}
