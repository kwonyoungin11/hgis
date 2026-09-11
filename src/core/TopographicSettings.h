#pragma once
#include <QString>
#include <QStringList>

class TopographicSettings {
public:
  struct Credentials { QString username; QString password; };
  static Credentials credentials();
  static bool saveCredentials(const Credentials& credentials, QString* error = nullptr);

private:
  friend class TopographicSettingsTest;
  static Credentials readFromFiles(const QString& personalFile, const QStringList& fallbackFiles);
  static bool saveToFile(const QString& personalFile, const Credentials& credentials, QString* error);
};
