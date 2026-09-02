#include "RecentSurveys.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

static QString listKey() { return QStringLiteral("RecentSurveys/items"); }

QSettings RecentSurveys::userSettings() {
  return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("ka-hgis"),
                   QStringLiteral("ka-hgis"));
}

QVector<RecentSurveys::Item> RecentSurveys::load(QSettings& settings) {
  QVector<Item> out;
  const QStringList raw = settings.value(listKey()).toStringList();
  for (const QString& line : raw) {
    const QStringList parts = line.split(QLatin1Char('\t'));
    if (parts.size() < 2)
      continue;
    Item it;
    it.path = parts.at(0).trimmed();
    it.name = parts.at(1).trimmed();
    if (parts.size() >= 3)
      it.lastOpenedMs = parts.at(2).toLongLong();
    if (it.path.isEmpty() || !QFileInfo::exists(it.path))
      continue;
    if (it.name.isEmpty())
      it.name = QFileInfo(it.path).completeBaseName();
    out.push_back(it);
    if (out.size() >= kMaxItems)
      break;
  }
  return out;
}

QString RecentSurveys::lastPath(QSettings& settings) {
  const QVector<Item> items = load(settings);
  return items.isEmpty() ? QString() : items.first().path;
}

void RecentSurveys::remember(QSettings& settings, const QString& path, const QString& name) {
  const QString abs = QFileInfo(path).absoluteFilePath();
  if (abs.isEmpty() || !QFileInfo::exists(abs))
    return;
  const QString shown = name.trimmed().isEmpty() ? QFileInfo(abs).completeBaseName() : name.trimmed();
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  QVector<Item> cur = load(settings);
  QVector<Item> next;
  next.push_back({shown, abs, now});
  for (const Item& it : cur) {
    if (QFileInfo(it.path).absoluteFilePath() == abs)
      continue;
    next.push_back(it);
    if (next.size() >= kMaxItems)
      break;
  }
  QStringList raw;
  for (const Item& it : next)
    raw << (it.path + QLatin1Char('\t') + it.name + QLatin1Char('\t') + QString::number(it.lastOpenedMs));
  settings.setValue(listKey(), raw);
  settings.sync();
}

void RecentSurveys::forget(QSettings& settings, const QString& path) {
  const QString abs = QFileInfo(path).absoluteFilePath();
  QVector<Item> cur = load(settings);
  QStringList raw;
  for (const Item& it : cur) {
    if (QFileInfo(it.path).absoluteFilePath() == abs)
      continue;
    raw << (it.path + QLatin1Char('\t') + it.name + QLatin1Char('\t') + QString::number(it.lastOpenedMs));
  }
  settings.setValue(listKey(), raw);
  settings.sync();
}
