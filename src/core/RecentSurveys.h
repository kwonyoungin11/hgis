#pragma once
#include <QString>
#include <QVector>

class QSettings;

class RecentSurveys {
public:
  struct Item {
    QString name;
    QString path;
    qint64 lastOpenedMs = 0;
  };

  static constexpr int kMaxItems = 12;

  static QSettings userSettings();
  static QVector<Item> load(QSettings& settings);
  static QString lastPath(QSettings& settings);
  static void remember(QSettings& settings, const QString& path, const QString& name);
  static void forget(QSettings& settings, const QString& path);
  static void setSkipAutoRestore(QSettings& settings, bool skip);
  static bool takeSkipAutoRestore(QSettings& settings);
};
