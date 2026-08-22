// Task 4 (RED->GREEN): KaSectionDrawingStudio UI 테스트
// 검증 항목:
//   (a) sectionLayersPanel / sectionLayoutView / sectionPropertiesPanel 3열 존재
//   (b) A3 가로 420x297 section_sheet placeholder가 view에 연결
//   (c) 기본값: A3, 축척 자동(idx=0), 표고 보정 0.00, 표고 간격 0.10,
//       거리 자동=true, 기준선 켜짐·0.20mm, PDF 비활성
//   (d) 프로젝트 layersAdded 후 트리 항목 수 증가

#include <cmath>

#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTreeWidget>
#include <QWidget>

#include "app/KaSectionDrawingStudio.h"

#include <qgsapplication.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutmanager.h>
#include <qgslayoutpagecollection.h>
#include <qgslayoutitempage.h>
#include <qgsprintlayout.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>

class TestSectionStudio : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void threePanelsExist();
    void placeholderSheetA3();
    void defaultControlValues();
    void layerTreeIgnoresSurveyAndBasemap();
    void emptyPaperHasTicks();
    void geoTiffButtonAndCrsChoices();
    void referenceLineColorButtonExists();
    void scaleComboHasMoreSamples();

private:
    QgsProject* m_project = nullptr;
};

void TestSectionStudio::initTestCase()
{
    // QgsApplication already initialized in main()
}

void TestSectionStudio::cleanupTestCase()
{
}

void TestSectionStudio::init()
{
    m_project = new QgsProject();
}

void TestSectionStudio::cleanup()
{
    delete m_project;
    m_project = nullptr;
}

// ---- (a) 3열 objectName 검증 ----------------------------------------
void TestSectionStudio::threePanelsExist()
{
    KaSectionDrawingStudio studio(m_project);
    QVERIFY2(studio.findChild<QWidget*>(QStringLiteral("sectionLayersPanel")),
             "sectionLayersPanel not found");
    QVERIFY2(studio.findChild<QWidget*>(QStringLiteral("sectionLayoutView")),
             "sectionLayoutView not found");
    QVERIFY2(studio.findChild<QWidget*>(QStringLiteral("sectionPropertiesPanel")),
             "sectionPropertiesPanel not found");
}

// ---- (b) A3 가로 placeholder 420x297 ----------------------------------
void TestSectionStudio::placeholderSheetA3()
{
    KaSectionDrawingStudio studio(m_project);
    auto* lm = m_project->layoutManager();
    auto* ly = dynamic_cast<QgsPrintLayout*>(
                   lm->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet layout not found after studio construction");
    QVERIFY2(ly->pageCollection(),              "pageCollection is null");
    QVERIFY2(ly->pageCollection()->pageCount() > 0, "no pages in layout");
    auto* page = ly->pageCollection()->page(0);
    QVERIFY2(page, "page(0) is null");

    const double w = page->pageSize().width();
    const double h = page->pageSize().height();
    QVERIFY2(std::abs(w - 420.0) < 1.0,
             qPrintable(QStringLiteral("page width: %1mm (expected 420mm)").arg(w, 0, 'f', 2)));
    QVERIFY2(std::abs(h - 297.0) < 1.0,
             qPrintable(QStringLiteral("page height: %1mm (expected 297mm)").arg(h, 0, 'f', 2)));
}

// ---- (c) 기본값 검증 --------------------------------------------------
void TestSectionStudio::defaultControlValues()
{
    KaSectionDrawingStudio studio(m_project);

    // paper = A3
    auto* paperCombo = studio.findChild<QComboBox*>(QStringLiteral("paperCombo"));
    QVERIFY2(paperCombo, "paperCombo not found");
    QCOMPARE(paperCombo->currentText(), QStringLiteral("A3"));

    // scale 자동 맞춤 = index 0
    auto* scaleCombo = studio.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY2(scaleCombo, "scaleCombo not found");
    QCOMPARE(scaleCombo->currentIndex(), 0);

    // elevOffset = 0.00
    auto* elevOffset = studio.findChild<QDoubleSpinBox*>(QStringLiteral("elevationOffsetSpin"));
    QVERIFY2(elevOffset, "elevationOffsetSpin not found");
    QCOMPARE(elevOffset->value(), 0.00);

    // elevInterval = 0.10
    auto* elevInterval = studio.findChild<QDoubleSpinBox*>(QStringLiteral("elevationIntervalSpin"));
    QVERIFY2(elevInterval, "elevationIntervalSpin not found");
    QVERIFY2(std::abs(elevInterval->value() - 0.10) < 1e-9,
             qPrintable(QStringLiteral("elevInterval: %1, expected 0.10")
                            .arg(elevInterval->value(), 0, 'f', 4)));

    // 거리 자동 = checked
    auto* distAuto = studio.findChild<QCheckBox*>(QStringLiteral("distanceAutoCheck"));
    QVERIFY2(distAuto, "distanceAutoCheck not found");
    QVERIFY2(distAuto->isChecked(), "distanceAutoCheck should be checked by default");

    // 기준선 = checked
    auto* refLine = studio.findChild<QCheckBox*>(QStringLiteral("referenceLineCheck"));
    QVERIFY2(refLine, "referenceLineCheck not found");
    QVERIFY2(refLine->isChecked(), "referenceLineCheck should be checked by default");

    // 기준선 굵기 = 0.20
    auto* refWidth = studio.findChild<QDoubleSpinBox*>(QStringLiteral("referenceLineWidthSpin"));
    QVERIFY2(refWidth, "referenceLineWidthSpin not found");
    QVERIFY2(std::abs(refWidth->value() - 0.20) < 1e-9,
             qPrintable(QStringLiteral("refWidth: %1, expected 0.20")
                            .arg(refWidth->value(), 0, 'f', 4)));

    // PDF 저장 버튼 비활성 (GeoTIFF 전)
    auto* pdfBtn = studio.findChild<QPushButton*>(QStringLiteral("pdfSaveBtn"));
    QVERIFY2(pdfBtn, "pdfSaveBtn not found");
    QVERIFY2(!pdfBtn->isEnabled(), "pdfSaveBtn should be disabled initially");
}

void TestSectionStudio::layerTreeIgnoresSurveyAndBasemap()
{
    KaSectionDrawingStudio studio(m_project);
    auto* tree = studio.findChild<QTreeWidget*>(QStringLiteral("sectionLayersTree"));
    QVERIFY2(tree, "sectionLayersTree not found");
    QCOMPARE(tree->topLevelItemCount(), 0);

    auto* layer = new QgsVectorLayer(
        QStringLiteral("Point?crs=EPSG:5186"),
        QStringLiteral("강릉1호 1차 장축"),
        QStringLiteral("memory"));
    QVERIFY2(layer->isValid(), "test vector layer is not valid");
    m_project->addMapLayer(layer);
    QCoreApplication::processEvents();

    QCOMPARE(tree->topLevelItemCount(), 0);
}

void TestSectionStudio::emptyPaperHasTicks()
{
    KaSectionDrawingStudio studio(m_project);
    auto* ly = dynamic_cast<QgsPrintLayout*>(
        m_project->layoutManager()->layoutByName(QStringLiteral("section_sheet")));
    QVERIFY2(ly, "section_sheet missing");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_elevation_0")),
             "paper should already have elevation ticks");
    QVERIFY2(ly->itemById(QStringLiteral("ka_section_distance_0")),
             "paper should already have distance ticks");
}

void TestSectionStudio::geoTiffButtonAndCrsChoices()
{
    KaSectionDrawingStudio studio(m_project);

    auto* addBtn = studio.findChild<QPushButton*>(QStringLiteral("addGeoTiffBtn"));
    QVERIFY2(addBtn, "addGeoTiffBtn not found");
    QVERIFY(addBtn->text().contains(QStringLiteral("GeoTIFF")));

    auto* crsCombo = studio.findChild<QComboBox*>(QStringLiteral("crsCombo"));
    QVERIFY2(crsCombo, "crsCombo not found");
    QVERIFY2(crsCombo->findData(QStringLiteral("EPSG:5187")) >= 0, "missing 5187");
    QVERIFY2(crsCombo->findData(QStringLiteral("EPSG:5186")) >= 0, "missing 5186");
    QCOMPARE(studio.selectedCrsAuthId(), QStringLiteral("EPSG:5187"));
}

void TestSectionStudio::referenceLineColorButtonExists()
{
    KaSectionDrawingStudio studio(m_project);
    auto* btn = studio.findChild<QPushButton*>(QStringLiteral("referenceLineColorBtn"));
    QVERIFY2(btn, "referenceLineColorBtn not found — 색상을 고를 수 있어야 한다");
    QCOMPARE(btn->property("lineColor").toString(), QStringLiteral("#D7191C"));
}

void TestSectionStudio::scaleComboHasMoreSamples()
{
    KaSectionDrawingStudio studio(m_project);
    auto* scaleCombo = studio.findChild<QComboBox*>(QStringLiteral("scaleCombo"));
    QVERIFY2(scaleCombo, "scaleCombo not found");
    QVERIFY2(scaleCombo->findText(QStringLiteral("1:10")) >= 0, "missing 1:10");
    QVERIFY2(scaleCombo->findText(QStringLiteral("1:25")) >= 0, "missing 1:25");
    QVERIFY2(scaleCombo->findText(QStringLiteral("1:40")) >= 0, "missing 1:40");
    QVERIFY2(scaleCombo->findText(QStringLiteral("1:250")) >= 0, "missing 1:250");
}

#include "test_section_studio.moc"

int main(int argc, char** argv)
{
    QgsApplication app(argc, argv, true);
    const QString prefix = qEnvironmentVariable(
        "QGIS_PREFIX_PATH",
        QFile::exists(QStringLiteral("A:/OSGeo4W/apps/qgis-dev"))
            ? QStringLiteral("A:/OSGeo4W/apps/qgis-dev")
            : QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
    QgsApplication::setPrefixPath(prefix, true);
    QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
    QgsApplication::initQgis();
    TestSectionStudio tc;
    const int rc = QTest::qExec(&tc, argc, argv);
    QgsApplication::exitQgis();
    return rc;
}
