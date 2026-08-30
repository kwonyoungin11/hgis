#pragma once
// SectionLayoutService: 단면도 조판 눈금 계산 및 QGIS 레이아웃 생성

#include <QList>
#include <QString>
#include <QVector>

#include <qgsrectangle.h>

class QgsMapLayer;
class QgsProject;

/// axisTicks() 반환 구조체. error가 비어 있으면 성공, 아니면 ticks는 항상 비어 있다.
struct AxisTickResult {
    QVector<double> ticks; ///< 성공 시 정렬된 눈금값
    QString         error; ///< 실패 이유 (한국어). 성공 시 isEmpty().
};

/// buildSectionLayout() 입력 옵션
struct SectionLayoutOptions {
    enum class Paper { A3, A4 };

    Paper   paper                   = Paper::A3;
    QString titleKo;                             ///< 도면명. 비어 있으면 "단면도" 사용.
    double  elevationOffsetM        = 0.0;       ///< 표고 보정값 (rasterY + offset = 표시값)
    double  elevationIntervalM      = 0.10;      ///< 표고 눈금 간격 (기본 0.10m)
    double  manualDistanceIntervalM = 0.0;       ///< 거리 눈금 간격 (0이면 auto 1-2-5)
    bool    showReferenceLine       = true;      ///< 붉은 점선 기준선 표시 여부
    double  referenceLineWidthMm    = 0.20;      ///< 기준선 굵기 (기본 0.20mm)
    QString referenceLineColor      = QStringLiteral("#D7191C"); ///< 기준선 색상
    /// 지도 축척 분모. 0 = 자동 맞춤(extent가 용지에 꽉 차도록).
    /// >0이면 사용자 지정값을 사용하되, 용지에 들어가지 않으면 자동 최솟값으로 올린다.
    double  scaleDenominator        = 0.0;
    /// 표제에 적는 수평 좌표계(5186/5187). 비어 있으면 EPSG:5187.
    /// 세계 XY로 눕힌 단면은 조판에 이 CRS를 쓰지 않는다(재투영하면 수평이 기운다).
    QString mapCrsAuthId;
    /// Double Box / Single Box / Line Ticks Up. 샘플은 용지가 아니라 스튜디오 스트립.
    QString scaleBarStyle = QStringLiteral("Double Box");
};

/// buildSectionLayout() 반환 결과
struct SectionLayoutResult {
    QString      layoutName;                      ///< 생성된 조판 이름. 오류 시 비어 있음.
    double       appliedScaleDenominator = 0.0;   ///< 적용된 축척 분모
    QgsRectangle appliedExtent;                   ///< 지도 항목에 설정된 범위
    QString      errorKo;                         ///< 오류 메시지 (한국어). 성공 시 isEmpty().
};

/// 단면도 눈금 계산 및 QGIS 조판 생성 서비스
class SectionLayoutService {
public:
    // ── Task 1/2: 순수 눈금 계산 (QGIS 의존 없음) ──────────────────────────

    /// [minVal, maxVal] 범위에서 interval 배수인 눈금값을 반환한다.
    ///
    /// 부동소수점 누적 대신 정수 인덱스 곱셈(i * interval)을 사용하므로
    /// 0.10m 간격 표고 눈금의 오차가 누적되지 않는다.
    ///
    /// 실패 조건 (error 설정, ticks 비어 있음 반환):
    ///   - interval <= 0 또는 비유한값
    ///   - minVal 또는 maxVal 비유한값
    ///   - minVal >= maxVal
    ///   - 눈금 수 > 500
    static AxisTickResult axisTicks(double minVal, double maxVal, double interval);

    /// 주어진 span에 대해 1-2-5 계열 중 targetTickCount 이하 눈금이 나오는
    /// 가장 작은 간격을 반환한다. 잘못된 인수는 1.0을 반환한다.
    static double niceDistanceInterval(double span, int targetTickCount = 7);

    // ── Task 3: QGIS 조판 생성 ──────────────────────────────────────────────

    /// 단면도 조판("section_sheet")을 프로젝트에 (재)생성한다.
    ///
    /// layers: 체크된 단면 GeoTIFF 레이어 목록 (체크 순서 보존).
    ///         비어 있으면 10m×2m 빈 용지와 표고·거리 눈금만 만든다.
    ///         회전된 지도 GT는 버리고 픽셀 가로=거리·세로=표고로 펼친다.
    ///         mapCrsAuthId는 표제·표시 CRS(재투영 없음).
    static SectionLayoutResult buildSectionLayout(
        QgsProject*                project,
        const QList<QgsMapLayer*>& layers,
        const SectionLayoutOptions& options = SectionLayoutOptions{});

    /// "section_sheet" 조판을 벡터 PDF로 내보낸다.
    /// 300 DPI, forceVectorOutput=true, rasterizeWholeImage=false,
    /// textRenderFormat=AlwaysText. 화면 DPI는 내보내기 후 복원한다.
    /// 성공 시 pdfPath, 실패 시 빈 문자열 (errorOut에 한국어 오류 설정).
    static QString exportSectionPdf(
        QgsProject*    project,
        const QString& pdfPath,
        QString*       errorOut = nullptr);
};
