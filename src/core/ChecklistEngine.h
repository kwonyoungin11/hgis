#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonObject>

struct CheckResult {
  QString id;
  QString severity;
  QString messageKo;
  bool passed = false;
};

class ChecklistEngine : public QObject {
  Q_OBJECT
public:
  explicit ChecklistEngine(QObject* parent = nullptr);
  bool loadRules(const QString& jsonPath);
  QVector<CheckResult> evaluate(const QJsonObject& projectState) const;
  int ruleCount() const { return m_rules.size(); }
private:
  struct Rule {
    QString id;
    QString severity;
    QString messageKo;
    QString checkType;
  };
  QVector<Rule> m_rules;
  static bool evalOne(const Rule& r, const QJsonObject& state);
};
