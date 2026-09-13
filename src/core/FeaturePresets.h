#pragma once

#include <QString>
#include <QVector>
#include <QColor>

class QgsVectorLayer;
class QgsFeature;
class QgsSymbol;

class FeaturePresets {
public:
  struct Kind {
    QString id;
    QString label;
    QString pattern;
    QString marker;
  };
  struct Period {
    QString id;
    QString label;
    QString color;
  };

  static FeaturePresets& instance();

  bool load(const QString& jsonPath);
  bool ensureLoaded();
  bool isLoaded() const { return !m_kinds.isEmpty() && !m_periods.isEmpty(); }

  const QVector<Kind>& kinds() const { return m_kinds; }
  const QVector<Period>& periods() const { return m_periods; }
  QString defaultKindLabel() const;
  QString defaultPeriodLabel() const;
  QString periodColorExpression(int alpha = 170) const;

  bool applyAttributes(QgsFeature* feature, const QString& kindLabel, const QString& periodLabel);
  bool applyRenderer(QgsVectorLayer* layer);

private:
  FeaturePresets() = default;
  QgsSymbol* symbolFor(int geomType, const Kind& kind) const;
  void applyPeriodColor(QgsSymbol* symbol, int geomType) const;

  QVector<Kind> m_kinds;
  QVector<Period> m_periods;
  bool m_tried = false;
};
