#include <QtTest>
#include <QFile>
#include <QCoreApplication>

#include "core/FeaturePresets.h"

#include <qgsapplication.h>
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>

class TestPresets : public QObject {
  Q_OBJECT
private slots:
  void loadsJson();
  void writesKindAndPeriod();
  void appliesRenderer();
};

void TestPresets::loadsJson() {
  const QString path = QStringLiteral("D:/qgis/data/styles/feature_presets.json");
  QVERIFY2(QFile::exists(path), qPrintable(path));
  QVERIFY(FeaturePresets::instance().load(path));
  QVERIFY(FeaturePresets::instance().kinds().size() >= 6);
  QVERIFY(FeaturePresets::instance().periods().size() >= 6);
  QCOMPARE(FeaturePresets::instance().defaultKindLabel(), QStringLiteral("기타"));
  QCOMPARE(FeaturePresets::instance().defaultPeriodLabel(), QStringLiteral("미정"));
  QVERIFY(FeaturePresets::instance().periodColorExpression().contains(QStringLiteral("청동기")));
}

void TestPresets::writesKindAndPeriod() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("fp"),
                                QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  vl->dataProvider()->addAttributes(
      {QgsField(QStringLiteral("kind"), QMetaType::Type::QString),
       QgsField(QStringLiteral("period"), QMetaType::Type::QString)});
  vl->updateFields();
  QgsFeature f(vl->fields());
  f.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200010, 450010)));
  QVERIFY(FeaturePresets::instance().applyAttributes(&f, QStringLiteral("수혈"),
                                                    QStringLiteral("청동기")));
  QCOMPARE(f.attribute(QStringLiteral("kind")).toString(), QStringLiteral("수혈"));
  QCOMPARE(f.attribute(QStringLiteral("period")).toString(), QStringLiteral("청동기"));
  delete vl;
}

void TestPresets::appliesRenderer() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"), QStringLiteral("fp"),
                                QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  vl->dataProvider()->addAttributes(
      {QgsField(QStringLiteral("kind"), QMetaType::Type::QString),
       QgsField(QStringLiteral("period"), QMetaType::Type::QString)});
  vl->updateFields();
  QVERIFY(FeaturePresets::instance().applyRenderer(vl));
  QVERIFY(vl->renderer());
  QCOMPARE(vl->renderer()->type(), QStringLiteral("categorizedSymbol"));
  delete vl;
}

#include "test_presets.moc"

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable(
      "QGIS_PREFIX_PATH", QFile::exists(QStringLiteral("D:/OSGeo4W/apps/qgis-dev"))
                              ? QStringLiteral("D:/OSGeo4W/apps/qgis-dev")
                              : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestPresets tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
