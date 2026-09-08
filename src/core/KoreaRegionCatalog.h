#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

struct KoreaRegionBounds {
  double west;
  double south;
  double east;
  double north;
};

struct KoreaSido {
  QString name;
  QString shortName;
  QStringList cities;
};

class KoreaRegionCatalog {
public:
  static QVector<KoreaSido> allSido();
  static QStringList sidoNames();
  static QStringList citiesOf(const QString& sidoName);
  static QStringList dongsOf(const QString& sidoName, const QString& cityName);
  static QString canonicalSido(const QString& name);
  // WGS84 mainland/Jeju overview framing, not administrative boundary geometry.
  static std::optional<KoreaRegionBounds> overviewBounds(const QString& sido);
  static QString composeAddress(const QString& sido, const QString& city, const QString& dong,
                                const QString& lot);
};
