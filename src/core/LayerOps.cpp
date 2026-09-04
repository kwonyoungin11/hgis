#include "LayerOps.h"
#include "GeorefService.h"
#include "SoilMapService.h"
#include "VworldSettings.h"
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <QStringConverter>
#include <QRegularExpression>
#include <cmath>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QScreen>
#include <QUrl>
#include <QWindow>
#include <QColor>
#include <QFont>
#include <QDir>
#include <QTemporaryFile>

#include <qgis.h>
#include <qgsproject.h>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsmapcanvas.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransformcontext.h>
#include <qgscoordinatetransform.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsfeature.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>
#include <qgslinestring.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgssymbol.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <qgsrenderer.h>
#include <qgsrectangle.h>
#include <qgslayertree.h>
#include <qgslayertreelayer.h>
#include <qgsbilinearrasterresampler.h>
#include <qgsrasterresamplefilter.h>
#include <qgsrasterdataprovider.h>
#include <qgsvectordataprovider.h>
#include <qgsrasterrenderer.h>
#include <qgsrastertransparency.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrastershader.h>
#include <qgscolorrampshader.h>
#include <qgscolorramplegendnodesettings.h>
#include <qgshillshaderenderer.h>
#include <qgsrasterbandstats.h>
#include <cpl_conv.h>
#include <qgsnetworkaccessmanager.h>
#include <qgslayertreegroup.h>
#include <qgsprojectviewsettings.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>
#include <qgstextformat.h>
#include <qgslabelobstaclesettings.h>
#include <qgsreferencedgeometry.h>
#include <QNetworkRequest>

QString LayerOps::reprojectVectorLayer(QgsVectorLayer* layer, const QString& targetCrsAuthId,
                                       const QString& outPath, QgsProject* project, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return {};
  }
  const QgsCoordinateReferenceSystem dest(targetCrsAuthId);
  if (!dest.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid target CRS");
    return {};
  }
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = outPath.endsWith(QLatin1String(".gpkg"), Qt::CaseInsensitive)
                        ? QStringLiteral("GPKG")
                        : QStringLiteral("ESRI Shapefile");
  opts.fileEncoding = QStringLiteral("UTF-8");
  opts.ct = QgsCoordinateTransform(layer->crs(), dest, project ? project->transformContext() : QgsCoordinateTransformContext());
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      layer, outPath, project ? project->transformContext() : QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  if (we != QgsVectorFileWriter::NoError) {
    if (errorOut) *errorOut = err.isEmpty() ? QStringLiteral("reproject write failed") : err;
    return {};
  }
  if (project) {
    auto* vl = new QgsVectorLayer(outPath, layer->name() + QStringLiteral("_") + targetCrsAuthId, QStringLiteral("ogr"));
    if (vl->isValid()) {
      vl->setCrs(dest);
      project->addMapLayer(vl);
    } else {
      delete vl;
    }
  }
  return outPath;
}

int LayerOps::ensureControlPointQualityFields(QgsVectorLayer* controlPoints) {
  if (!controlPoints || !controlPoints->isValid()) return 0;
  int added = 0;
  QgsFields fields = controlPoints->fields();
  auto ensure = [&](const QString& name, QMetaType::Type t) {
    if (fields.indexOf(name) < 0) {
      if (!controlPoints->isEditable()) controlPoints->startEditing();
      if (controlPoints->dataProvider()->addAttributes({QgsField(name, t)})) {
        ++added;
      }
    }
  };
  ensure(QStringLiteral("accuracy_m"), QMetaType::Type::Double);
  ensure(QStringLiteral("pdop"), QMetaType::Type::Double);
  ensure(QStringLiteral("fix_type"), QMetaType::Type::QString);
  ensure(QStringLiteral("pixel_x"), QMetaType::Type::Double);
  ensure(QStringLiteral("pixel_y"), QMetaType::Type::Double);
  if (added > 0) {
    controlPoints->updateFields();
    if (controlPoints->isEditable()) controlPoints->commitChanges();
  }
  return added;
}

bool LayerOps::applyDomainDrawStyle(QgsVectorLayer* layer, const QString& layerKeyIn) {
  if (!layer || !layer->isValid()) return false;
  const QString key = layerKeyIn.isEmpty() ? layerKeyOf(layer) : layerKeyIn;
  const Qgis::GeometryType gt = layer->geometryType();

  QColor fill(37, 99, 235, 90);
  QColor stroke(37, 99, 235, 255);
  double strokeW = 1.2;
  double markerSize = 3.5;

  if (key == QLatin1String("survey_area")) {
    fill = QColor(180, 83, 9, 70);
    stroke = QColor(146, 64, 14, 255);
    strokeW = 1.6;
  } else if (key == QLatin1String("feature_poly")) {
    fill = QColor(22, 163, 74, 90);
    stroke = QColor(17, 94, 44, 255);
    strokeW = 1.8;
  } else if (key == QLatin1String("feature_line") || key == QLatin1String("section_line")) {
    stroke = key == QLatin1String("section_line") ? QColor(190, 24, 93, 255) : QColor(202, 138, 4, 255);
    strokeW = 1.8;
  } else if (key == QLatin1String("control_points")) {
    fill = QColor(234, 179, 8, 255);
    stroke = QColor(161, 98, 7, 255);
    markerSize = 4.0;
  } else if (key == QLatin1String("artifact_point")) {
    fill = QColor(185, 28, 28, 255);
    stroke = QColor(127, 29, 29, 255);
    markerSize = 3.6;
  } else if (key == QLatin1String("trial_trench")) {
    // 시굴 트렌치 도면 관례: 붉은 외곽선 0.5, 채움은 거의 없음(위성·지적 위 판독).
    fill = QColor(220, 38, 38, 18);
    stroke = QColor(220, 38, 38, 255);
    strokeW = 0.5;
  }

  QgsSymbol* sym = nullptr;
  if (gt == Qgis::GeometryType::Polygon) {
    auto fs = QgsFillSymbol::createSimple({
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(strokeW)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
    });
    sym = fs.release();
  } else if (gt == Qgis::GeometryType::Line) {
    if (key == QLatin1String("section_line")) {
      auto ls = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), QStringLiteral("#FFFFFF")},
          {QStringLiteral("line_width"), QStringLiteral("3.0")},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
          {QStringLiteral("line_style"), QStringLiteral("solid")},
      });
      auto* core = new QgsSimpleLineSymbolLayer(stroke, strokeW, Qt::SolidLine);
      core->setWidthUnit(Qgis::RenderUnit::Millimeters);
      ls->appendSymbolLayer(core);
      sym = ls.release();
    } else {
      auto ls = QgsLineSymbol::createSimple({
          {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
          {QStringLiteral("line_width"), QString::number(strokeW)},
          {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
      });
      sym = ls.release();
    }
  } else if (gt == Qgis::GeometryType::Point) {
    auto ms = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QStringLiteral("0.6")},
        {QStringLiteral("size"), QString::number(markerSize)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    sym = ms.release();
  } else {
    sym = QgsSymbol::defaultSymbol(gt);
    if (sym)
      sym->setColor(stroke);
  }
  if (!sym) return false;
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), strokeW);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_marker_mm"), markerSize);
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  const QString detectedField = detectNameField(layer);
  if (!detectedField.isEmpty())
    applyNameAttributeLabels(layer, detectedField, 5.0, false);
  else if (gt == Qgis::GeometryType::Polygon)
    applyAreaM2Labels(layer);
  layer->triggerRepaint();
  return true;
}

QString LayerOps::detectNameField(const QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return {};
  const QgsFields fds = layer->fields();
  if (fds.isEmpty()) return {};

  // 1. Cultural heritage & Archaeology (문화유적분포지도, 발굴/지표조사구역, 유적, 고고학)
  const QStringList heritageCandidates = {
    QStringLiteral("사업명"), QStringLiteral("유적명"), QStringLiteral("유적명칭"),
    QStringLiteral("조사명"), QStringLiteral("보고서명"), QStringLiteral("소재지"),
    QStringLiteral("yujuk_nm"), QStringLiteral("yujeok_nm"), QStringLiteral("site_name"),
    QStringLiteral("hist_nm"), QStringLiteral("rem_nm"), QStringLiteral("명칭"),
    QStringLiteral("name"), QStringLiteral("title")
  };
  for (const QString& c : heritageCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 2. Cadastral (연속지적도, 지적도, 토지)
  const QStringList cadCandidates = {
    QStringLiteral("jibun"), QStringLiteral("지번"), QStringLiteral("a2"),
    QStringLiteral("a1"), QStringLiteral("pnu")
  };
  for (const QString& c : cadCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 3. Archaeology features (유구, 도면)
  const QStringList featCandidates = {
    QStringLiteral("feature_no"), QStringLiteral("유구번호"), QStringLiteral("유구명"),
    QStringLiteral("호수"), QStringLiteral("kind_ko"), QStringLiteral("kind")
  };
  for (const QString& c : featCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 4. Digital topo / buildings / roads (수치지형도, 건물, 도로, 지명)
  const QStringList topoCandidates = {
    QStringLiteral("buld_nm"), QStringLiteral("건물명"), QStringLiteral("지명"),
    QStringLiteral("road_nm"), QStringLiteral("도로명"), QStringLiteral("label"),
    QStringLiteral("kor_nm")
  };
  for (const QString& c : topoCandidates) {
    int idx = fds.lookupField(c);
    if (idx >= 0) return fds.at(idx).name();
  }

  // 5. Scan string fields containing name-related keywords
  for (int i = 0; i < fds.count(); ++i) {
    const QgsField f = fds.at(i);
    const QString n = f.name().toLower();
    if (f.type() == QMetaType::QString || f.typeName().contains(QLatin1String("char"), Qt::CaseInsensitive) ||
        f.typeName().contains(QLatin1String("string"), Qt::CaseInsensitive)) {
      if (n.contains(QStringLiteral("명")) || n.contains(QLatin1String("name")) ||
          n.contains(QLatin1String("title")) || n.contains(QLatin1String("label")) ||
          n.contains(QStringLiteral("지번")) || n.contains(QLatin1String("jibun"))) {
        return f.name();
      }
    }
  }

  // 6. First string field if available
  for (int i = 0; i < fds.count(); ++i) {
    const QgsField f = fds.at(i);
    if (f.type() == QMetaType::QString)
      return f.name();
  }

  return fds.at(0).name();
}

QString LayerOps::prepareShapefileEncoding(const QString& shpPath) {
  QFileInfo fi(shpPath);
  if (!fi.exists() || fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) != 0)
    return QString();

  const QString dir = fi.absolutePath();
  const QString base = fi.completeBaseName();
  const QString cpgPath = QDir(dir).filePath(base + QStringLiteral(".cpg"));
  const QString dbfPath = QDir(dir).filePath(base + QStringLiteral(".dbf"));

  // 1. 이미 .cpg 파일이 존재하는 경우 해당 인코딩을 GDAL에 적용
  if (QFile::exists(cpgPath)) {
    QFile cpgFile(cpgPath);
    if (cpgFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      const QString content = QString::fromUtf8(cpgFile.readAll()).trimmed();
      cpgFile.close();
      if (!content.isEmpty()) {
        const QString upper = content.toUpper();
        if (upper.contains(QLatin1String("UTF-8")) || upper.contains(QLatin1String("UTF8"))) {
          CPLSetConfigOption("SHAPE_ENCODING", "UTF-8");
          return QStringLiteral("UTF-8");
        }
        if (upper.contains(QLatin1String("CP949")) || upper.contains(QLatin1String("949")) ||
            upper.contains(QLatin1String("EUC-KR")) || upper.contains(QLatin1String("ANSI")) ||
            upper.contains(QLatin1String("SYSTEM"))) {
          CPLSetConfigOption("SHAPE_ENCODING", "CP949");
          return QStringLiteral("CP949");
        }
        CPLSetConfigOption("SHAPE_ENCODING", content.toLatin1().constData());
        return content;
      }
    }
  }

  // 2. .cpg 파일이 없는 경우: .dbf 앞부분(16KB)을 검사하여 UTF-8 vs CP949 자동 판별
  QString detected = QStringLiteral("CP949"); // 한국 공공데이터·지적도·유적도 SHP 기본값
  if (QFile::exists(dbfPath)) {
    QFile dbfFile(dbfPath);
    if (dbfFile.open(QIODevice::ReadOnly)) {
      const QByteArray sample = dbfFile.read(32768);
      dbfFile.close();

      bool hasNonAscii = false;
      bool validUtf8 = true;
      int utf8MultiByteCount = 0;

      const unsigned char* bytes = reinterpret_cast<const unsigned char*>(sample.constData());
      const int len = sample.size();
      for (int i = 0; i < len; ++i) {
        const unsigned char c = bytes[i];
        if (c > 127) {
          hasNonAscii = true;
          if ((c & 0xE0) == 0xC0 && c >= 0xC2) { // 2-byte UTF-8
            if (i + 1 < len && (bytes[i + 1] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              ++i;
            } else {
              validUtf8 = false;
              break;
            }
          } else if ((c & 0xF0) == 0xE0) { // 3-byte UTF-8 (한글 완성형은 EA..ED)
            if (i + 2 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              i += 2;
            } else {
              validUtf8 = false;
              break;
            }
          } else if ((c & 0xF8) == 0xF0 && c <= 0xF4) { // 4-byte UTF-8
            if (i + 3 < len && (bytes[i + 1] & 0xC0) == 0x80 && (bytes[i + 2] & 0xC0) == 0x80 &&
                (bytes[i + 3] & 0xC0) == 0x80) {
              ++utf8MultiByteCount;
              i += 3;
            } else {
              validUtf8 = false;
              break;
            }
          } else {
            validUtf8 = false;
            break;
          }
        }
      }

      // 비ASCII 바이트가 있고 온전한 UTF-8 멀티바이트가 충분히 존재할 때만 UTF-8
      if (hasNonAscii && validUtf8 && utf8MultiByteCount >= 4) {
        detected = QStringLiteral("UTF-8");
      } else {
        // CP949 바이트이거나 영문/숫자 헤더만 있는 경우: 한국 GIS 환경 관례상 CP949
        detected = QStringLiteral("CP949");
      }
    }
  }

  // 3. .cpg 파일이 없으면 자동 생성하여 영구 보존 (다음번 및 타 GIS 소프트웨어 호환)
  if (!QFile::exists(cpgPath)) {
    QFile outCpg(cpgPath);
    if (outCpg.open(QIODevice::WriteOnly | QIODevice::Text)) {
      outCpg.write(detected.toUtf8() + "\n");
      outCpg.close();
    }
  }

  // 4. GDAL 드라이버 레벨에서 SHAPE_ENCODING 옵션 설정
  CPLSetConfigOption("SHAPE_ENCODING", detected.toLatin1().constData());
  return detected;
}

bool LayerOps::setShapefileEncoding(QgsVectorLayer* layer, const QString& encoding) {
  if (!layer || !layer->isValid()) return false;
  const QString src = layer->source().split(QLatin1Char('|')).first();
  QFileInfo fi(src);
  if (fi.exists() && fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) == 0) {
    const QString cpgPath = QDir(fi.absolutePath()).filePath(fi.completeBaseName() + QStringLiteral(".cpg"));
    QFile cpgFile(cpgPath);
    if (cpgFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      cpgFile.write(encoding.toUtf8() + "\n");
      cpgFile.close();
    }
  }
  CPLSetConfigOption("SHAPE_ENCODING", encoding.toLatin1().constData());
  layer->setProviderEncoding(encoding);
  layer->reload();
  layer->updateFields();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::applyNameAttributeLabels(QgsVectorLayer* layer, const QString& fieldName,
                                       double fontSizePt, bool showArea) {
  if (!layer || !layer->isValid()) return false;

  QString targetField = fieldName;
  if (targetField.isEmpty())
    targetField = detectNameField(layer);

  if (fontSizePt <= 0.0)
    fontSizePt = 5.0;

  if (!targetField.isEmpty())
    layer->setCustomProperty(QStringLiteral("ka_hgis/label_field"), targetField);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_font_size"), fontSizePt);
  layer->setCustomProperty(QStringLiteral("ka_hgis/label_show_area"), showArea);

  QgsPalLayerSettings s;
  s.drawLabels = true;

  const bool isPolygon = (layer->geometryType() == Qgis::GeometryType::Polygon);
  const bool hasField = !targetField.isEmpty() && layer->fields().indexOf(targetField) >= 0;

  if (hasField && isPolygon && showArea) {
    s.fieldName = QStringLiteral("coalesce(\"%1\", '') || '\\n(' || format_number(area($geometry), 1) || ' ㎡)'")
                      .arg(targetField);
    s.isExpression = true;
  } else if (hasField) {
    s.fieldName = QStringLiteral("\"%1\"").arg(targetField);
    s.isExpression = true;
  } else if (isPolygon && showArea) {
    s.fieldName = QStringLiteral("format_number(area($geometry), 1) || ' ㎡'");
    s.isExpression = true;
  } else if (isPolygon) {
    s.fieldName = QStringLiteral("format_number(area($geometry), 1) || ' ㎡'");
    s.isExpression = true;
  } else if (layer->fields().count() > 0) {
    s.fieldName = QStringLiteral("\"%1\"").arg(layer->fields().at(0).name());
    s.isExpression = true;
  } else {
    return false;
  }

  if (isPolygon) {
    s.placement = Qgis::LabelPlacement::OverPoint;
    s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);
  } else if (layer->geometryType() == Qgis::GeometryType::Line) {
    s.placement = Qgis::LabelPlacement::Line;
  } else {
    s.placement = Qgis::LabelPlacement::AroundPoint;
  }

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSizeF(fontSizePt);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(fontSizePt);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(31, 35, 40));

  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

double LayerOps::labelFontSize(const QgsVectorLayer* layer, double defaultSize) {
  if (!layer) return defaultSize;
  const QVariant v = layer->customProperty(QStringLiteral("ka_hgis/label_font_size"));
  if (v.isValid() && v.toDouble() > 0.0)
    return v.toDouble();
  if (layer->labeling()) {
    const QgsPalLayerSettings s = layer->labeling()->settings();
    if (s.format().size() > 0.0)
      return s.format().size();
  }
  return defaultSize;
}

bool LayerOps::labelShowArea(const QgsVectorLayer* layer, bool defaultShow) {
  if (!layer) return defaultShow;
  const QVariant v = layer->customProperty(QStringLiteral("ka_hgis/label_show_area"));
  if (v.isValid())
    return v.toBool();
  if (layer->labeling()) {
    const QString expr = layer->labeling()->settings().fieldName;
    if (expr.contains(QLatin1String("area($geometry)")))
      return true;
  }
  return defaultShow;
}

QString LayerOps::currentLabelField(const QgsVectorLayer* layer) {
  if (!layer) return {};
  const QString f = layer->customProperty(QStringLiteral("ka_hgis/label_field")).toString();
  if (!f.isEmpty()) return f;
  return detectNameField(layer);
}

bool LayerOps::applyAreaM2Labels(QgsVectorLayer* layer) {
  if (!layer || !layer->isValid()) return false;
  if (layer->geometryType() != Qgis::GeometryType::Polygon) return false;

  QgsPalLayerSettings s;
  s.drawLabels = true;
  s.fieldName = QStringLiteral("format_number(area($geometry), 2) || ' ㎡'");
  s.isExpression = true;
  s.placement = Qgis::LabelPlacement::OverPoint;
  s.setPolygonPlacementFlags(Qgis::LabelPolygonPlacementFlag::AllowPlacementInsideOfPolygon);

  QgsLabelObstacleSettings obs = s.obstacleSettings();
  obs.setIsObstacle(false);
  s.setObstacleSettings(obs);

  QgsTextFormat fmt;
  QFont font = fmt.font();
  font.setFamily(QStringLiteral("Malgun Gothic"));
  font.setPointSize(5);
  font.setBold(true);
  fmt.setFont(font);
  fmt.setSize(5);
  fmt.setSizeUnit(Qgis::RenderUnit::Points);
  fmt.setColor(QColor(15, 23, 42));
  QgsTextBufferSettings buf = fmt.buffer();
  buf.setEnabled(true);
  buf.setSize(0.8);
  buf.setColor(QColor(255, 255, 255, 230));
  fmt.setBuffer(buf);
  s.setFormat(fmt);

  layer->setLabeling(new QgsVectorLayerSimpleLabeling(s));
  layer->setLabelsEnabled(true);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::hasToggleableLabels(const QgsMapLayer* layer) {
  const auto* vl = qobject_cast<const QgsVectorLayer*>(layer);
  if (!vl || !vl->isValid())
    return false;
  return vl->labeling() != nullptr || vl->labelsEnabled() ||
         vl->geometryType() == Qgis::GeometryType::Polygon ||
         vl->fields().count() > 0;
}

bool LayerOps::labelsVisible(const QgsMapLayer* layer) {
  const auto* vl = qobject_cast<const QgsVectorLayer*>(layer);
  return vl && vl->isValid() && vl->labelsEnabled();
}

bool LayerOps::setLabelsVisible(QgsMapLayer* layer, bool on) {
  auto* vl = qobject_cast<QgsVectorLayer*>(layer);
  if (!vl || !vl->isValid())
    return false;
  if (on && !vl->labeling()) {
    const QString nf = detectNameField(vl);
    if (!nf.isEmpty()) {
      applyNameAttributeLabels(vl, nf, 5.0, false);
      return true;
    } else if (vl->geometryType() == Qgis::GeometryType::Polygon && applyAreaM2Labels(vl)) {
      return true;
    }
  }
  vl->setLabelsEnabled(on);
  vl->triggerRepaint();
  return true;
}

bool LayerOps::applySimpleVectorStyle(QgsVectorLayer* layer, const QColor& fillIn, const QColor& strokeIn,
                                      double strokeWidthMm, double markerSizeMm, bool noFill,
                                      bool noStroke, bool dashed) {
  if (!layer || !layer->isValid()) return false;
  QColor fill = fillIn.isValid() ? fillIn : QColor(37, 99, 235, 90);
  QColor stroke = strokeIn.isValid() ? strokeIn : QColor(37, 99, 235, 255);
  if (strokeWidthMm <= 0.0) strokeWidthMm = 1.0;
  if (markerSizeMm <= 0.0) markerSizeMm = 3.5;
  if (noFill) fill = QColor(0, 0, 0, 0);
  if (noStroke) stroke = QColor(0, 0, 0, 0);
  if (noFill && noStroke) {
    noStroke = false;
    stroke = QColor(100, 100, 100, 255);
    strokeWidthMm = 0.4;
  }

  const Qgis::GeometryType gt = layer->geometryType();
  QgsSymbol* sym = nullptr;
  if (gt == Qgis::GeometryType::Polygon) {
    QVariantMap props{
        {QStringLiteral("color"), fill.name(QColor::HexArgb)},
        {QStringLiteral("style"), noFill ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("outline_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    };
    auto fs = QgsFillSymbol::createSimple(props);
    sym = fs.release();
  } else if (gt == Qgis::GeometryType::Line) {
    auto ls = QgsLineSymbol::createSimple({
        {QStringLiteral("line_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("line_width"), QString::number(noStroke ? 0.0 : strokeWidthMm)},
        {QStringLiteral("line_width_unit"), QStringLiteral("MM")},
        {QStringLiteral("line_style"),
         noStroke ? QStringLiteral("no") : (dashed ? QStringLiteral("dash") : QStringLiteral("solid"))},
    });
    sym = ls.release();
  } else if (gt == Qgis::GeometryType::Point) {
    auto ms = QgsMarkerSymbol::createSimple({
        {QStringLiteral("name"), QStringLiteral("circle")},
        {QStringLiteral("color"), noFill ? QStringLiteral("#00000000") : fill.name(QColor::HexArgb)},
        {QStringLiteral("outline_color"), stroke.name(QColor::HexArgb)},
        {QStringLiteral("outline_width"), noStroke ? QStringLiteral("0") : QStringLiteral("0.6")},
        {QStringLiteral("outline_style"), noStroke ? QStringLiteral("no") : QStringLiteral("solid")},
        {QStringLiteral("size"), QString::number(markerSizeMm)},
        {QStringLiteral("size_unit"), QStringLiteral("MM")},
    });
    sym = ms.release();
  } else {
    return false;
  }
  if (!sym) return false;

  layer->setCustomProperty(QStringLiteral("ka_hgis/style_fill"), fill.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_stroke"), stroke.name(QColor::HexArgb));
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_width_mm"), strokeWidthMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_marker_mm"), markerSizeMm);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_fill"), noFill);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_no_stroke"), noStroke);
  layer->setCustomProperty(QStringLiteral("ka_hgis/style_dashed"), dashed);
  layer->setRenderer(new QgsSingleSymbolRenderer(sym));
  layer->triggerRepaint();
  return true;
}

bool LayerOps::readSimpleVectorStyle(const QgsVectorLayer* layer, QColor* fill, QColor* stroke,
                                     double* strokeWidthMm, double* markerSizeMm, bool* noFill,
                                     bool* noStroke, bool* dashed) {
  if (!layer) return false;

  QColor f(37, 99, 235, 90);
  QColor s(37, 99, 235, 255);
  double w = 1.2;
  double m = 3.5;
  bool nf = false;
  bool ns = false;
  bool dash = false;

  const QString key = layerKeyOf(layer);
  if (key == QLatin1String("survey_area")) {
    f = QColor(180, 83, 9, 70);
    s = QColor(146, 64, 14, 255);
    w = 1.6;
  } else if (key == QLatin1String("feature_poly")) {
    f = QColor(22, 163, 74, 90);
    s = QColor(17, 94, 44, 255);
    w = 1.8;
  } else if (key == QLatin1String("feature_line")) {
    s = QColor(202, 138, 4, 255);
    w = 1.8;
  } else if (key == QLatin1String("section_line")) {
    s = QColor(190, 24, 93, 255);
    w = 1.8;
  } else if (key == QLatin1String("control_points")) {
    f = QColor(234, 179, 8, 255);
    s = QColor(161, 98, 7, 255);
    m = 4.0;
  } else if (key == QLatin1String("artifact_point")) {
    f = QColor(185, 28, 28, 255);
    s = QColor(127, 29, 29, 255);
    m = 3.6;
  }

  const QVariant cf = layer->customProperty(QStringLiteral("ka_hgis/style_fill"));
  const QVariant cs = layer->customProperty(QStringLiteral("ka_hgis/style_stroke"));
  const QVariant cw = layer->customProperty(QStringLiteral("ka_hgis/style_width_mm"));
  const QVariant cm = layer->customProperty(QStringLiteral("ka_hgis/style_marker_mm"));
  const QVariant cnf = layer->customProperty(QStringLiteral("ka_hgis/style_no_fill"));
  const QVariant cns = layer->customProperty(QStringLiteral("ka_hgis/style_no_stroke"));
  const QVariant cd = layer->customProperty(QStringLiteral("ka_hgis/style_dashed"));
  if (cf.isValid()) {
    const QColor parsed(cf.toString());
    if (parsed.isValid()) f = parsed;
  }
  if (cs.isValid()) {
    const QColor parsed(cs.toString());
    if (parsed.isValid()) s = parsed;
  }
  if (cw.isValid()) w = cw.toDouble();
  if (cm.isValid()) m = cm.toDouble();
  if (cnf.isValid()) nf = cnf.toBool();
  if (cns.isValid()) ns = cns.toBool();
  if (cd.isValid()) dash = cd.toBool();
  if (f.alpha() == 0) nf = true;
  if (s.alpha() == 0) ns = true;

  if (const QgsFeatureRenderer* ren = layer->renderer()) {
    if (const auto* single = dynamic_cast<const QgsSingleSymbolRenderer*>(ren)) {
      if (const QgsSymbol* sym = single->symbol()) {
        if (sym->color().isValid()) {
          if (layer->geometryType() == Qgis::GeometryType::Line)
            s = sym->color();
          else if (!nf)
            f = sym->color();
        }
      }
    }
  }

  if (fill) *fill = f;
  if (stroke) *stroke = s;
  if (strokeWidthMm) *strokeWidthMm = w;
  if (markerSizeMm) *markerSizeMm = m;
  if (noFill) *noFill = nf;
  if (noStroke) *noStroke = ns;
  if (dashed) *dashed = dash;
  return true;
}

bool LayerOps::applyFeaturePolyStyle(QgsVectorLayer* featurePoly) {
  if (!featurePoly || !featurePoly->isValid()) return false;
  QString field = QStringLiteral("kind");
  if (featurePoly->fields().indexOf(field) < 0) field = QStringLiteral("period");
  if (featurePoly->fields().indexOf(field) < 0)
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));

  QSet<QString> values;
  QgsFeatureIterator it = featurePoly->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    const QString v = f.attribute(field).toString().trimmed();
    if (!v.isEmpty()) values.insert(v);
  }
  if (values.isEmpty())
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));

  QgsCategoryList cats;
  int i = 0;
  const QList<QString> sorted = values.values();
  for (const QString& v : sorted) {
    QColor c = QColor::fromHsv((i * 47) % 360, 180, 230, 160);
    QgsSymbol* sym = QgsSymbol::defaultSymbol(featurePoly->geometryType());
    if (sym) {
      sym->setColor(c);
      cats.append(QgsRendererCategory(QVariant(v), sym, v));
    }
    ++i;
  }
  if (cats.isEmpty())
    return applyDomainDrawStyle(featurePoly, QStringLiteral("feature_poly"));
  auto* renderer = new QgsCategorizedSymbolRenderer(field, cats);
  featurePoly->setRenderer(renderer);
  featurePoly->triggerRepaint();
  return true;
}

bool LayerOps::mergePolygonFeatures(QgsVectorLayer* layer, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return false;
  }
  if (layer->geometryType() != Qgis::GeometryType::Polygon) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 레이어만 묶을 수 있습니다");
    return false;
  }

  QVector<QgsGeometry> geoms;
  QgsFeatureIds ids;
  QgsFeature first;
  bool hasFirst = false;
  QgsFeature f;
  QgsFeatureIterator it = layer->getFeatures();
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
    QgsGeometry g = f.geometry();
    if (!g.isGeosValid())
      g = g.makeValid();
    if (g.isEmpty()) continue;
    geoms.append(g);
    ids.insert(f.id());
    if (!hasFirst) {
      first = QgsFeature(f);
      hasFirst = true;
    }
  }
  if (geoms.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("묶을 폴리곤이 2개 이상 필요합니다 (현재 %1개)").arg(geoms.size());
    return false;
  }

  QgsGeometry multi = QgsGeometry::collectGeometry(geoms);
  if (multi.isEmpty())
    multi = QgsGeometry::unaryUnion(geoms);
  if (multi.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 결합 실패");
    return false;
  }
  if (!multi.isGeosValid())
    multi = multi.makeValid();

  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드 시작 실패");
    return false;
  }
  if (!layer->deleteFeatures(ids)) {
    if (errorOut) *errorOut = QStringLiteral("기존 피처 삭제 실패");
    if (startedHere) layer->rollBack();
    return false;
  }
  QgsFeature out(layer->fields());
  out.setAttributes(first.attributes());
  out.setGeometry(multi);
  if (!layer->addFeature(out)) {
    if (errorOut) *errorOut = QStringLiteral("결합 피처 추가 실패");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (startedHere && !layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char(';'));
    layer->rollBack();
    return false;
  }
  layer->triggerRepaint();
  return true;
}

QgsVectorLayer* LayerOps::clipLayerByBoundary(QgsVectorLayer* sourceLayer,
                                             QgsVectorLayer* boundaryLayer,
                                             QgsProject* project,
                                             QString* errorOut) {
  if (!sourceLayer || !sourceLayer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("자를 대상 레이어가 올바르지 않습니다.");
    return nullptr;
  }
  if (!boundaryLayer || !boundaryLayer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("기준 바운더리 레이어가 올바르지 않습니다.");
    return nullptr;
  }
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 유효하지 않습니다.");
    return nullptr;
  }

  // 1. 바운더리 레이어의 지오메트리를 대상 레이어 CRS로 변환 후 Union
  QgsCoordinateTransform xf;
  const bool needXf = boundaryLayer->crs().isValid() && sourceLayer->crs().isValid() &&
                     boundaryLayer->crs() != sourceLayer->crs();
  if (needXf) {
    xf = QgsCoordinateTransform(boundaryLayer->crs(), sourceLayer->crs(),
                                project ? project->transformContext()
                                        : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
  }

  QVector<QgsGeometry> bGeoms;
  QgsFeature bf;
  QgsFeatureIterator bit = boundaryLayer->getFeatures();
  while (bit.nextFeature(bf)) {
    if (!bf.hasGeometry() || bf.geometry().isEmpty()) continue;
    QgsGeometry bg = bf.geometry();
    if (needXf) {
      try {
        if (bg.transform(xf) != Qgis::GeometryOperationResult::Success) continue;
      } catch (...) {
        continue;
      }
    }
    if (!bg.isGeosValid())
      bg = bg.makeValid();
    if (!bg.isEmpty())
      bGeoms.append(bg);
  }

  if (bGeoms.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("바운더리 레이어에 유효한 지오메트리가 없습니다.");
    return nullptr;
  }

  QgsGeometry boundaryUnion = QgsGeometry::unaryUnion(bGeoms);
  if (boundaryUnion.isEmpty()) {
    boundaryUnion = QgsGeometry::collectGeometry(bGeoms);
  }
  if (boundaryUnion.isEmpty() || !boundaryUnion.isGeosValid()) {
    boundaryUnion = boundaryUnion.makeValid();
  }
  if (boundaryUnion.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("바운더리 결합 지오메트리 생성에 실패했습니다.");
    return nullptr;
  }

  // 2. 결과 메모리 레이어 생성
  const QString geomTypeStr = QgsWkbTypes::displayString(sourceLayer->wkbType());
  const QString uri = QStringLiteral("%1?crs=%2").arg(geomTypeStr, sourceLayer->crs().authid());
  const QString outTitle = QStringLiteral("[클립] %1").arg(sourceLayer->name());
  auto* clippedLayer = new QgsVectorLayer(uri, outTitle, QStringLiteral("memory"));
  if (!clippedLayer || !clippedLayer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("클립 결과 레이어 생성 실패");
    delete clippedLayer;
    return nullptr;
  }

  clippedLayer->dataProvider()->addAttributes(sourceLayer->fields().toList());
  clippedLayer->updateFields();

  // 3. 대상 레이어 피처 순회 및 교차(Intersection) 추출
  QgsFeature sf;
  QgsFeatureIterator sit = sourceLayer->getFeatures();
  QVector<QgsFeature> outFeatures;
  while (sit.nextFeature(sf)) {
    if (!sf.hasGeometry() || sf.geometry().isEmpty()) continue;
    QgsGeometry sg = sf.geometry();
    if (!sg.isGeosValid())
      sg = sg.makeValid();
    if (sg.isEmpty()) continue;

    if (!sg.intersects(boundaryUnion)) continue;

    QgsGeometry clippedGeom = sg.intersection(boundaryUnion);
    if (clippedGeom.isEmpty()) continue;
    if (!clippedGeom.isGeosValid())
      clippedGeom = clippedGeom.makeValid();
    if (clippedGeom.isEmpty()) continue;

    QgsFeature newFeat(clippedLayer->fields());
    newFeat.setAttributes(sf.attributes());
    newFeat.setGeometry(clippedGeom);
    outFeatures.append(newFeat);
  }

  if (outFeatures.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("바운더리와 겹치는 구간(피처)이 없습니다.");
    delete clippedLayer;
    return nullptr;
  }

  clippedLayer->dataProvider()->addFeatures(outFeatures);
  clippedLayer->updateExtents();

  if (sourceLayer->renderer()) {
    clippedLayer->setRenderer(sourceLayer->renderer()->clone());
  }

  markSurveyLayer(clippedLayer, QStringLiteral("clip_%1").arg(sourceLayer->id()));
  project->addMapLayer(clippedLayer);
  placeInLegendGroup(project, clippedLayer, kGroupSurveyData);

  return clippedLayer;
}

bool LayerOps::splitPolygonWithLine(QgsVectorLayer* layer,
                                   const QVector<QgsPointXY>& splitLine,
                                   QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("올바른 레이어가 아닙니다.");
    return false;
  }
  if (layer->geometryType() != Qgis::GeometryType::Polygon) {
    if (errorOut) *errorOut = QStringLiteral("폴리곤 레이어만 나눌 수 있습니다.");
    return false;
  }
  if (splitLine.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("분할선은 최소 2개 이상의 점이어야 합니다.");
    return false;
  }

  QgsGeometry lineGeom = QgsGeometry::fromPolylineXY(splitLine);
  if (lineGeom.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("유효한 분할선이 아닙니다.");
    return false;
  }

  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드 시작 실패");
    return false;
  }

  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature f;
  int splitCount = 0;

  const QgsFeatureIds selIds = layer->selectedFeatureIds();
  const bool filterSelected = !selIds.isEmpty();

  while (it.nextFeature(f)) {
    if (filterSelected && !selIds.contains(f.id())) continue;
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;

    QgsGeometry g = f.geometry();
    if (!g.intersects(lineGeom)) continue;

    QVector<QgsGeometry> newGeometries;
    QgsPointSequence topologyTestPoints;
    QgsLineString splitCurve(splitLine);
    Qgis::GeometryOperationResult res = g.splitGeometry(&splitCurve, newGeometries, false, false, topologyTestPoints, true);
    if (res == Qgis::GeometryOperationResult::Success && !newGeometries.isEmpty()) {
      layer->changeGeometry(f.id(), g);

      for (const auto& newG : newGeometries) {
        if (newG.isEmpty()) continue;
        QgsFeature newF(layer->fields());
        newF.setAttributes(f.attributes());
        newF.setGeometry(newG);
        layer->addFeature(newF);
      }
      ++splitCount;
    }
  }

  if (splitCount == 0) {
    if (startedHere) layer->rollBack();
    if (errorOut) *errorOut = QStringLiteral("분할선이 관통하는 폴리곤이 없습니다. 폴리곤 양쪽 경계를 완전히 가로질러야 합니다.");
    return false;
  }

  if (startedHere && !layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char(';'));
    layer->rollBack();
    return false;
  }

  layer->triggerRepaint();
  return true;
}

bool LayerOps::splitTwoOverlappingFeatures(QgsVectorLayer* layer1, qint64 fid1,
                                          QgsVectorLayer* layer2, qint64 fid2,
                                          QString* errorOut) {
  if (!layer1 || !layer1->isValid() || !layer2 || !layer2->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("유효하지 않은 레이어입니다.");
    return false;
  }

  QgsFeature f1, f2;
  if (!layer1->getFeatures(QgsFeatureRequest(fid1)).nextFeature(f1) || !f1.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("첫 번째 피처를 찾을 수 없습니다.");
    return false;
  }
  if (!layer2->getFeatures(QgsFeatureRequest(fid2)).nextFeature(f2) || !f2.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("두 번째 피처를 찾을 수 없습니다.");
    return false;
  }

  QgsGeometry g1 = f1.geometry();
  QgsGeometry g2 = f2.geometry();

  if (layer1->crs().isValid() && layer2->crs().isValid() && layer1->crs() != layer2->crs()) {
    QgsCoordinateTransform xf(layer2->crs(), layer1->crs(),
                              QgsProject::instance() ? QgsProject::instance()->transformContext()
                                                     : QgsCoordinateTransformContext());
    xf.setBallparkTransformsAreAppropriate(true);
    try {
      if (g2.transform(xf) != Qgis::GeometryOperationResult::Success) {
        if (errorOut) *errorOut = QStringLiteral("좌표계 변환에 실패했습니다.");
        return false;
      }
    } catch (...) {
      if (errorOut) *errorOut = QStringLiteral("좌표계 변환 예외가 발생했습니다.");
      return false;
    }
  }

  if (!g1.isGeosValid()) g1 = g1.makeValid();
  if (!g2.isGeosValid()) g2 = g2.makeValid();

  if (!g1.intersects(g2)) {
    if (errorOut) *errorOut = QStringLiteral("선택한 두 도형이 서로 겹치지 않습니다.");
    return false;
  }

  QgsGeometry interGeom = g1.intersection(g2);
  if (interGeom.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("두 도형의 교차 영역을 계산할 수 없습니다.");
    return false;
  }
  if (!interGeom.isGeosValid()) interGeom = interGeom.makeValid();

  QgsVectorLayer* targetLayer = layer1;
  QgsFeature targetFeat = f1;
  QgsGeometry cutterGeom = g2;

  const QString k1 = layerKeyOf(layer1);
  if (k1 == QLatin1String("survey_area") || layer1->name().contains(QStringLiteral("바운더리"))) {
    targetLayer = layer2;
    targetFeat = f2;
    cutterGeom = f1.geometry();
    if (layer1->crs() != layer2->crs()) {
      QgsCoordinateTransform xf(layer1->crs(), layer2->crs(),
                                QgsProject::instance() ? QgsProject::instance()->transformContext()
                                                       : QgsCoordinateTransformContext());
      xf.setBallparkTransformsAreAppropriate(true);
      cutterGeom.transform(xf);
    }
    if (!cutterGeom.isGeosValid()) cutterGeom = cutterGeom.makeValid();
  }

  QgsGeometry targetGeom = targetFeat.geometry();
  if (!targetGeom.isGeosValid()) targetGeom = targetGeom.makeValid();

  QgsGeometry targetInter = targetGeom.intersection(cutterGeom);
  QgsGeometry targetDiff = targetGeom.difference(cutterGeom);

  if (targetInter.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("대상 도형의 교차 영역이 비어 있습니다.");
    return false;
  }
  if (targetDiff.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("대상 도형이 다른 도형 안에 완전히 포함되어 있어 추가 분할할 영역이 없습니다.");
    return false;
  }

  const bool startedHere = !targetLayer->isEditable();
  if (startedHere && !targetLayer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집 모드 시작 실패");
    return false;
  }

  if (!targetLayer->changeGeometry(targetFeat.id(), targetDiff)) {
    if (errorOut) *errorOut = QStringLiteral("기존 피처 지오메트리 업데이트 실패");
    if (startedHere) targetLayer->rollBack();
    return false;
  }

  QgsFeature newF(targetLayer->fields());
  newF.setAttributes(targetFeat.attributes());
  newF.setGeometry(targetInter);
  if (!targetLayer->addFeature(newF)) {
    if (errorOut) *errorOut = QStringLiteral("분할된 교차 피처 추가 실패");
    if (startedHere) targetLayer->rollBack();
    return false;
  }

  if (startedHere && !targetLayer->commitChanges()) {
    if (errorOut) *errorOut = targetLayer->commitErrors().join(QLatin1Char(';'));
    targetLayer->rollBack();
    return false;
  }

  targetLayer->triggerRepaint();
  return true;
}

static void applyKaNetworkHeaders(QNetworkRequest* req) {
  if (!req) return;
  req->setHeader(QNetworkRequest::UserAgentHeader,
                 QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) ka-hgis/0.3 QGIS"));
  const QString host = req->url().host();
  if (host.contains(QLatin1String("vworld.kr"), Qt::CaseInsensitive))
    req->setRawHeader("Referer", "https://localhost");
  const QUrl fixed = SoilMapService::rewriteArcGisCacheUrl(req->url());
  if (fixed != req->url())
    req->setUrl(fixed);
}

void LayerOps::ensureTileNetworkIdentity() {
  static bool once = false;
  if (once) return;
  once = true;
  QgsNetworkAccessManager::instance()->setCacheDisabled(false);
  QgsNetworkAccessManager::setRequestPreprocessor(&applyKaNetworkHeaders);
}

static bool uriLooksLikeXyz(const QString& source) {
  return source.contains(QLatin1String("type=xyz"), Qt::CaseInsensitive);
}

static void tuneBasemapLayer(QgsRasterLayer* rl, bool crispText = false) {
  if (!rl || !rl->isValid()) return;
  rl->setBlendMode(QPainter::CompositionMode_SourceOver);
  const QString src = rl->source();
  if (src.contains(QLatin1String("crs=EPSG:4326"), Qt::CaseInsensitive)) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")));
  } else if (uriLooksLikeXyz(src) || src.contains(QLatin1String("ka-hgis-vworld-cadastral"), Qt::CaseInsensitive) ||
             src.contains(QLatin1String("crs=EPSG:3857"), Qt::CaseInsensitive) ||
             src.contains(QLatin1String("crs=EPSG:900913"), Qt::CaseInsensitive)) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  } else if (!rl->crs().isValid()) {
    rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  }
  if (QgsRasterResampleFilter* rf = rl->resampleFilter()) {
    if (crispText) {
      rf->setZoomedInResampler(nullptr);
      rf->setZoomedOutResampler(nullptr);
    } else {
      rf->setZoomedInResampler(new QgsBilinearRasterResampler());
      rf->setZoomedOutResampler(new QgsBilinearRasterResampler());
    }
  }
  if (crispText) {
    if (QgsRasterDataProvider* dp = rl->dataProvider()) {
      dp->setDpi(192);
      dp->setZoomedInResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
      dp->setZoomedOutResamplingMethod(Qgis::RasterResamplingMethod::Nearest);
    }
  }
}

static void zoomCanvasToWorkingScale(QgsMapCanvas* canvas, const QString& crsAuthId,
                                     double targetScale = 50000.0) {
  if (!canvas) return;
  const QString auth = crsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186") : crsAuthId;
  const QgsRectangle cur = canvas->extent();
  const bool hasLocalView = !cur.isEmpty() && cur.isFinite() && cur.width() > 0 && cur.height() > 0 &&
                            canvas->scale() > 50.0 && canvas->scale() < 500000.0;
  if (!hasLocalView)
    LayerOps::zoomToKorea(canvas, auth);
  if (targetScale > 0.0)
    canvas->zoomScale(targetScale, true);
  LayerOps::clampCanvasToKorea(canvas);
}

QgsLayerTreeGroup* LayerOps::ensureLegendGroup(QgsProject* project, const QString& groupName) {
  Q_UNUSED(project);
  Q_UNUSED(groupName);
  // ORIG-3: never create empty legend groups. Flat additive layer list only.
  return nullptr;
}

void LayerOps::placeInLegendGroup(QgsProject* project, QgsMapLayer* layer, const QString& groupName,
                                  bool insertAtBottom) {
  Q_UNUSED(groupName);
  Q_UNUSED(insertAtBottom);
  if (!project || !layer) return;
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    if (QgsLayerTreeLayer* node = root->findLayer(layer->id())) {
      node->setItemVisibilityChecked(true);
    }
  }
  ensureSatelliteAtBottom(project);
  pruneEmptyLegendGroups(project);
}

void LayerOps::markSurveyLayer(QgsMapLayer* layer, const QString& layerKey) {
  if (!layer) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerKey), layerKey);
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleSurvey));
}

void LayerOps::knockOutRasterPaper(QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return;
  if (isBasemapLayer(layer)) return;
  // 내장 GT가 있는 GeoTIFF를 맞추기 대기로 숨기면 지도에는 보여도 조판에서 빠진다.
  if (isAlignPending(layer) && !GeorefService::looksUnreferencedRaster(layer))
    setAlignPending(layer, false);
  if (layer->bandCount() < 3) return;
  QgsRasterRenderer* rend = layer->renderer();
  if (!rend) return;
  if (const QgsRasterTransparency* old = rend->rasterTransparency()) {
    const auto list = old->transparentThreeValuePixelList();
    for (const auto& px : list) {
      if (std::abs(px.red - 255.0) < 1e-6 && px.opacity < 0.01)
        return;
    }
  }
  auto* trans = new QgsRasterTransparency();
  QVector<QgsRasterTransparency::TransparentThreeValuePixel> whites;
  whites.append(QgsRasterTransparency::TransparentThreeValuePixel(255, 255, 255, 0.0, 16, 16, 16));
  trans->setTransparentThreeValuePixelList(whites);
  rend->setRasterTransparency(trans);
  layer->triggerRepaint();
}

void LayerOps::knockOutProjectRasterPaper(QgsProject* project) {
  if (!project) return;
  for (QgsMapLayer* ml : project->mapLayers())
    knockOutRasterPaper(qobject_cast<QgsRasterLayer*>(ml));
}

void LayerOps::markReferenceLayer(QgsMapLayer* layer) {
  if (!layer) return;
  layer->setCustomProperty(QString::fromUtf8(kPropLayerRole), QString::fromUtf8(kRoleReference));
}

void LayerOps::applyThematicOverlayScaleRange(QgsMapLayer* layer) {
  if (!layer) return;
  layer->setScaleBasedVisibility(true);
  // QGIS minimum scale is exclusive; +1 keeps 1:100000 visible.
  layer->setMinimumScale(kThematicMinScaleDenom + 1.0);
  layer->setMaximumScale(0.0);
}

bool LayerOps::clampCanvasToThematicScale(QgsMapCanvas* canvas) {
  if (!canvas) return false;
  if (canvas->scale() > kThematicMinScaleDenom) {
    canvas->zoomScale(kThematicMinScaleDenom, true);
    return true;
  }
  return false;
}

QgsRectangle LayerOps::expandExtentToMaxSpan(const QgsRectangle& extent, double maxSpanMeters) {
  if (extent.isEmpty() || maxSpanMeters <= 0.0) return extent;
  const double w = extent.width();
  const double h = extent.height();
  if (w <= 0.0 || h <= 0.0) return extent;
  if (w > maxSpanMeters || h > maxSpanMeters) return extent;
  const double longer = std::max(w, h);
  const double factor = maxSpanMeters / longer;
  if (factor <= 1.0) return extent;
  QgsRectangle grown = extent;
  grown.scale(factor);
  return grown;
}

void LayerOps::setAlignPending(QgsMapLayer* layer, bool pending) {
  if (!layer) return;
  if (pending)
    layer->setCustomProperty(QString::fromUtf8(kPropAlignPending), true);
  else
    layer->removeCustomProperty(QString::fromUtf8(kPropAlignPending));
}

bool LayerOps::isAlignPending(const QgsMapLayer* layer) {
  return layer && layer->customProperty(QString::fromUtf8(kPropAlignPending)).toBool();
}

QString LayerOps::layerKeyOf(const QgsMapLayer* layer) {
  if (!layer) return {};
  return layer->customProperty(QString::fromUtf8(kPropLayerKey)).toString();
}

static QString stripLegendCrsSuffix(QString name) {
  int bracket = name.lastIndexOf(QStringLiteral(" [EPSG:"));
  if (bracket < 0) bracket = name.lastIndexOf(QStringLiteral(" [CRS"));
  if (bracket >= 0) name = name.left(bracket).trimmed();
  return name;
}

static QString friendlyLegendName(const QString& name) {
  const QString base = stripLegendCrsSuffix(name);
  if (base.contains(QStringLiteral("지적"))) {
    const bool bon = base.contains(QStringLiteral("본번"));
    const bool bu = base.contains(QStringLiteral("부번"));
    if (bon && !bu) return QStringLiteral("지적 본번");
    if (bu && !bon) return QStringLiteral("지적 부번");
    return QStringLiteral("지적");
  }
  if (base.contains(QStringLiteral("위성")))
    return QStringLiteral("위성");
  return base;
}

static bool legendTitlesMatch(const QString& a, const QString& b) {
  if (a == b) return true;
  if (a.startsWith(b + QLatin1String(" [")) || b.startsWith(a + QLatin1String(" [")))
    return true;
  const QString soil = QStringLiteral("토양도(흙토람)");
  if (a.startsWith(soil) && b.startsWith(soil))
    return true;
  return friendlyLegendName(a) == friendlyLegendName(b);
}

void LayerOps::applyLegendCrsLabel(QgsMapLayer* layer) {
  if (!layer) return;
  const QString shown = friendlyLegendName(layer->name());
  if (!shown.isEmpty() && layer->name() != shown)
    layer->setName(shown);
  const QString auth = layer->crs().isValid() ? layer->crs().authid() : QString();
  if (!auth.isEmpty()) {
    layer->setAbstract(QStringLiteral("좌표계 %1").arg(auth));
    layer->setCustomProperty(QStringLiteral("ka_hgis/crs_label"), auth);
  }
}

bool LayerOps::isReferenceLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  if (layer->customProperty(QString::fromUtf8(kPropLayerRole)).toString() ==
      QLatin1String(kRoleReference))
    return true;
  const QString n = layer->name();
  return n.contains(QStringLiteral("OSM")) || n.contains(QStringLiteral("VWorld")) ||
         n.contains(QStringLiteral("Carto")) || n.contains(QStringLiteral("Google")) ||
         n.contains(QStringLiteral("고도맵")) || n.contains(QStringLiteral("지형맵")) ||
         n == QLatin1String("DEM") || n.contains(QStringLiteral("OpenTopoMap")) ||
         n.contains(QStringLiteral("고지형")) || n == QLatin1String("위성") ||
         n.startsWith(QLatin1String("지적"));
}

bool LayerOps::isBasemapLayer(const QgsMapLayer* layer) {
  if (!layer) return false;
  // Live tiles only. A user SHP named "지적…" must not survive 새 조사.
  const QString p = layer->providerType();
  return p == QLatin1String("wms") || p == QLatin1String("xyz") || p == QLatin1String("vectortile");
}

QgsVectorLayer* LayerOps::findByLayerKey(QgsProject* project, const QString& layerKey) {
  if (!project || layerKey.isEmpty()) return nullptr;
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* v = qobject_cast<QgsVectorLayer*>(l);
    if (!v) continue;
    if (layerKeyOf(v) == layerKey) return v;
  }
  const auto byName = project->mapLayersByName(layerKey);
  if (!byName.isEmpty())
    return qobject_cast<QgsVectorLayer*>(byName.first());
  return nullptr;
}

QgsVectorLayer* LayerOps::digitizeTargetLayer(QgsProject* project, QgsVectorLayer* current,
                                              const QString& requiredKey) {
  if (requiredKey.isEmpty())
    return nullptr;
  if (current && current->isValid() && layerKeyOf(current) == requiredKey)
    return current;
  return findByLayerKey(project, requiredKey);
}

QStringList LayerOps::domainLayerKeys() {
  return {QStringLiteral("survey_area"), QStringLiteral("feature_poly"), QStringLiteral("feature_line"),
          QStringLiteral("section_line"), QStringLiteral("control_points"),
          QStringLiteral("artifact_point"), QStringLiteral("trial_trench")};
}

void LayerOps::removeSurveyDomainLayers(QgsProject* project) {
  if (!project) return;
  const QStringList domainKeys = domainLayerKeys();
  QStringList ids;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    if (isBasemapLayer(l)) continue;
    const QString key = layerKeyOf(l);
    // 도면 레이어(survey_area, feature_poly 등)만 제거하고, 외부에서 불러온 SHP/DXF 등 사용자 레이어는 온전히 유지한다.
    if (!domainKeys.contains(key))
      continue;
    if (auto* v = qobject_cast<QgsVectorLayer*>(l)) {
      if (v->isEditable())
        v->rollBack();
    }
    ids.append(l->id());
  }
  if (!ids.isEmpty())
    project->removeMapLayers(ids);
  pruneEmptyLegendGroups(project);
}

void LayerOps::pruneEmptyLegendGroups(QgsProject* project) {
  if (!project) return;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return;
  const QList<QgsLayerTreeNode*> children = root->children();
  for (QgsLayerTreeNode* n : children) {
    auto* g = qobject_cast<QgsLayerTreeGroup*>(n);
    if (!g) continue;
    if (g->children().isEmpty())
      root->removeChildNode(g);
  }
}

QgsVectorLayer* LayerOps::ensureDomainLayer(QgsProject* project, const QString& gpkgPath,
                                            const QString& layerKey, const QString& titleKo,
                                            QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return nullptr;
  }
  if (auto* existing = findByLayerKey(project, layerKey))
    return existing;
  if (gpkgPath.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("먼저 「새 조사」로 저장 경로를 만드세요.");
    return nullptr;
  }
  auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, layerKey),
                                titleKo, QStringLiteral("ogr"));
  const bool canCreateMissing = layerKey == QLatin1String("artifact_point")
                                || layerKey == QLatin1String("trial_trench");
  if (!vl->isValid() && canCreateMissing) {
    delete vl;
    vl = nullptr;
    const QgsCoordinateReferenceSystem crs = project->crs().isValid()
        ? project->crs()
        : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
    const QString memUri = (layerKey == QLatin1String("trial_trench"))
                               ? QStringLiteral("Polygon?crs=%1").arg(crs.authid())
                               : QStringLiteral("Point?crs=%1").arg(crs.authid());
    QgsVectorLayer mem(memUri, titleKo, QStringLiteral("memory"));
    if (mem.isValid()) {
      QgsFields fields;
      if (layerKey == QLatin1String("trial_trench")) {
        fields.append(QgsField(QStringLiteral("name"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("width"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("length"), QMetaType::Type::Double));
      } else {
      fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("artifact_no"), QMetaType::Type::QString));
      fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      }
      mem.dataProvider()->addAttributes(fields.toList());
      mem.updateFields();
      mem.setCrs(crs);
      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("GPKG");
      opts.layerName = layerKey;
      opts.fileEncoding = QStringLiteral("UTF-8");
      opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
      QString errMsg, newFn, newLayer;
      if (QgsVectorFileWriter::writeAsVectorFormatV3(
              &mem, gpkgPath, project->transformContext(), opts, &errMsg, &newFn, &newLayer) ==
          QgsVectorFileWriter::NoError) {
        vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkgPath, layerKey),
                                titleKo, QStringLiteral("ogr"));
      } else if (errorOut) {
        *errorOut = errMsg;
      }
    }
  }
  if (!vl || !vl->isValid()) {
    if (errorOut && errorOut->isEmpty())
      *errorOut = vl ? vl->error().message() : QStringLiteral("레이어를 열 수 없습니다.");
    delete vl;
    return nullptr;
  }
  // OGR/GPKG keeps a feature cache. A previous legend-only delete leaves the
  // table; without reload, 「조사구역」 다시 만들기가 지운 면을 그대로 보여 준다.
  if (QgsDataProvider* p = vl->dataProvider())
    p->reloadData();
  vl->updateExtents();
  vl->setName(titleKo);
  markSurveyLayer(vl, layerKey);
  applyLegendCrsLabel(vl);
  applyDomainDrawStyle(vl, layerKey);
  project->addMapLayer(vl, true);
  pruneEmptyLegendGroups(project);
  return vl;
}

QgsVectorLayer* LayerOps::createUserPolygonLayer(QgsProject* project, const QString& gpkgPath,
                                                 const QString& titleKo, const QString& crsAuthId,
                                                 QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  QString crsId = crsAuthId.trimmed();
  if (crsId.isEmpty()) crsId = QStringLiteral("EPSG:5186");
  QgsCoordinateReferenceSystem crs(crsId);
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("좌표계가 올바르지 않습니다.");
    return nullptr;
  }

  const QString key = QStringLiteral("user_poly_%1").arg(QDateTime::currentMSecsSinceEpoch());
  QgsVectorLayer mem(QStringLiteral("Polygon?crs=%1").arg(crs.authid()), titleKo, QStringLiteral("memory"));
  if (!mem.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("면 레이어를 만들 수 없습니다.");
    return nullptr;
  }
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();
  mem.setCrs(crs);

  QString loadPath;
  if (!gpkgPath.isEmpty() && QFile::exists(gpkgPath)) {
    QgsVectorFileWriter::SaveVectorOptions opts;
    opts.driverName = QStringLiteral("GPKG");
    opts.layerName = key;
    opts.fileEncoding = QStringLiteral("UTF-8");
    opts.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    QString errMsg, newFn, newLayer;
    const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
        &mem, gpkgPath, QgsCoordinateTransformContext(), opts, &errMsg, &newFn, &newLayer);
    if (we != QgsVectorFileWriter::NoError) {
      if (errorOut) *errorOut = errMsg.isEmpty() ? QStringLiteral("GPKG에 레이어를 쓰지 못했습니다.") : errMsg;
      return nullptr;
    }
    loadPath = QStringLiteral("%1|layername=%2").arg(gpkgPath, key);
  } else {
    loadPath = QStringLiteral("Polygon?crs=%1").arg(crs.authid());
  }

  auto* vl = new QgsVectorLayer(loadPath, titleKo, gpkgPath.isEmpty() ? QStringLiteral("memory")
                                                                      : QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = vl->error().message();
    delete vl;
    return nullptr;
  }
  if (gpkgPath.isEmpty()) {
    vl->dataProvider()->addAttributes(fields.toList());
    vl->updateFields();
    vl->setCrs(crs);
  }
  vl->setName(titleKo);
  markSurveyLayer(vl, key);
  applyLegendCrsLabel(vl);
  applyAreaM2Labels(vl);
  project->addMapLayer(vl, true);
  placeInLegendGroup(project, vl, QString::fromUtf8(kGroupSurveyData));
  pruneEmptyLegendGroups(project);
  return vl;
}

void LayerOps::ensureSatelliteAtBottom(QgsProject* project) {
  if (!project) return;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return;

  QList<QgsMapLayer*> nonSat;
  QList<QgsMapLayer*> sat;

  for (QgsLayerTreeNode* child : root->children()) {
    if (auto* lnode = qobject_cast<QgsLayerTreeLayer*>(child)) {
      if (QgsMapLayer* l = lnode->layer()) {
        const QString name = lnode->name().isEmpty() ? l->name() : lnode->name();
        if (name.contains(QStringLiteral("위성"))) {
          sat.append(l);
        } else {
          nonSat.append(l);
        }
      }
    }
  }

  if (sat.isEmpty()) return;

  bool needReorder = false;
  bool seenSat = false;
  for (QgsLayerTreeNode* child : root->children()) {
    if (auto* lnode = qobject_cast<QgsLayerTreeLayer*>(child)) {
      if (QgsMapLayer* l = lnode->layer()) {
        const QString name = lnode->name().isEmpty() ? l->name() : lnode->name();
        if (name.contains(QStringLiteral("위성"))) {
          seenSat = true;
        } else if (seenSat) {
          needReorder = true;
          break;
        }
      }
    }
  }

  if (!needReorder) return;

  const QList<QgsMapLayer*> newOrder = nonSat + sat;
  root->reorderGroupLayers(newOrder);
}

QList<QgsMapLayer*> LayerOps::visibleLayersPaintOrder(QgsProject* project) {
  QList<QgsMapLayer*> visible;
  if (!project) return visible;
  ensureSatelliteAtBottom(project);
  QgsLayerTree* root = project->layerTreeRoot();
  QList<QgsMapLayer*> ordered = root ? root->layerOrder() : QList<QgsMapLayer*>();
  if (ordered.isEmpty()) {
    const QMap<QString, QgsMapLayer*> all = project->mapLayers();
    for (auto it = all.constBegin(); it != all.constEnd(); ++it) {
      if (it.value() && it.value()->isValid())
        ordered.append(it.value());
    }
  }

  auto pushVisible = [&](QgsMapLayer* l) {
    if (!l || !l->isValid()) return;
    if (isAlignPending(l)) return;
    if (root) {
      if (QgsLayerTreeLayer* n = root->findLayer(l->id())) {
        if (!n->itemVisibilityChecked()) return;
      }
    }
    visible.append(l);
  };
  for (QgsMapLayer* l : ordered)
    pushVisible(l);
  if (visible.isEmpty()) {
    for (QgsMapLayer* l : project->mapLayers())
      pushVisible(l);
  }

  // 위성 레이어는 캔버스 렌더링 순서에서도 항상 가장 바닥(스택의 맨 끝)으로 배치
  QList<QgsMapLayer*> sats;
  for (int i = visible.size() - 1; i >= 0; --i) {
    if (visible[i] && visible[i]->name().contains(QStringLiteral("위성"))) {
      sats.prepend(visible.takeAt(i));
    }
  }
  for (QgsMapLayer* sat : sats) {
    visible.append(sat);
  }

  return visible;
}

void LayerOps::applyCanvasScreenDpi(QgsMapCanvas* canvas) {
  if (!canvas) return;
  // FHD 100% → 4K 150–200% (DPR 1.0–2.0). PassThrough 배율 그대로 쓴다.
  // 같은 값이라도 다시 쓰면 렌더 설정이 더러워져 캐시를 버리고 전체를 다시 그린다.
  // 이 함수는 9곳에서 불리므로 값이 실제로 달라질 때만 손댄다.
  const qreal dpr = canvas->devicePixelRatioF();
  if (dpr > 0.05 &&
      !qFuzzyCompare(canvas->mapSettings().devicePixelRatio(), static_cast<float>(dpr)))
    canvas->mapSettings().setDevicePixelRatio(static_cast<float>(dpr));
  QWindow* wh = canvas->windowHandle();
  if (!wh && canvas->window())
    wh = canvas->window()->windowHandle();
  if (wh && wh->screen()) {
    const double dpi = wh->screen()->logicalDotsPerInch();
    if (dpi > 10.0 && !qFuzzyCompare(canvas->mapSettings().outputDpi(), dpi))
      canvas->mapSettings().setOutputDpi(dpi);
  }
}

bool LayerOps::canvasDisplayEventNeedsTileRefresh(int eventType) {
  const auto t = static_cast<QEvent::Type>(eventType);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  if (t == QEvent::DevicePixelRatioChange)
    return true;
#endif
  Q_UNUSED(t);
  return false;
}

void LayerOps::syncMapCanvas(QgsProject* project, QgsMapCanvas* canvas, bool zoomKorea) {
  if (!project || !canvas) return;
  knockOutProjectRasterPaper(project);
  applyCanvasScreenDpi(canvas);

  QList<QgsMapLayer*> visible = visibleLayersPaintOrder(project);
  const bool layersChanged = (visible != canvas->layers());

  if (project->crs().isValid())
    canvas->setDestinationCrs(project->crs());
  else
    canvas->setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::ensureOtfEnabled(project, canvas, canvas->mapSettings().destinationCrs().authid());

  if (layersChanged)
    canvas->setLayers(visible);
  canvas->setCachingEnabled(true);
  // QgsMapCanvas::setRenderFlag(true)는 이미 켜져 있어도 refresh()를 강제한다.
  // 여기가 레이어를 만질 때마다 불리므로 필요 없는 전체 렌더가 한 번씩 더 붙었다.
  if (!canvas->renderFlag())
    canvas->setRenderFlag(true);
  const bool wasFrozen = canvas->isFrozen();
  if (!wasFrozen)
    canvas->freeze(false);
  // XYZ + OTF(QgsRasterProjector) + parallel job: provider_wms deleteLater AV on Windows.
  // 병렬 렌더만 끈다 — 미리보기까지 끄면 화면을 끄는 동안 지도가 비어 깜빡인다.
  canvas->setParallelRenderingEnabled(false);

  if (zoomKorea) {
    const QString auth = canvas->mapSettings().destinationCrs().isValid()
                             ? canvas->mapSettings().destinationCrs().authid()
                             : QStringLiteral("EPSG:5186");
    LayerOps::applyKoreaMapLimits(project, canvas);
    QgsRectangle kr = LayerOps::koreaExtentForCrs(auth);
    if (kr.isEmpty() || !kr.isFinite())
      kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:3857"));
    if (!kr.isEmpty() && kr.isFinite()) {
      canvas->setExtent(kr);
      canvas->zoomToFeatureExtent(kr);
      if (canvas->scale() > 3000000.0)
        canvas->zoomScale(1200000.0, true);
    }
    LayerOps::clampCanvasToKorea(canvas);
  }
  if ((layersChanged || zoomKorea) && !canvas->isDrawing())
    canvas->refresh();
}

static bool extentUsable(const QgsRectangle& ext) {
  return !ext.isNull() && ext.isFinite();
}

static QgsRectangle vectorFeatureExtent(QgsVectorLayer* vl) {
  if (!vl) return {};
  vl->updateExtents();
  QgsRectangle ext = vl->extent();
  if (extentUsable(ext)) return ext;
  QgsRectangle acc;
  bool any = false;
  QgsFeatureIterator it = vl->getFeatures(QgsFeatureRequest().setNoAttributes());
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry() || f.geometry().isEmpty()) continue;
    const QgsRectangle b = f.geometry().boundingBox();
    if (!extentUsable(b)) continue;
    if (!any) {
      acc = b;
      any = true;
    } else {
      acc.combineExtentWith(b);
    }
  }
  return any ? acc : QgsRectangle();
}

bool LayerOps::zoomToLayerMax(QgsMapCanvas* canvas, QgsMapLayer* layer) {
  if (!canvas || !layer || !layer->isValid()) return false;

  QgsRectangle ext;
  if (auto* vl = qobject_cast<QgsVectorLayer*>(layer))
    ext = vectorFeatureExtent(vl);
  else
    ext = layer->extent();

  const QgsCoordinateReferenceSystem layerCrs = layer->crs();
  const QgsCoordinateReferenceSystem mapCrs = canvas->mapSettings().destinationCrs().isValid()
                                                  ? canvas->mapSettings().destinationCrs()
                                                  : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186"));
  const QString mapAuth = mapCrs.isValid() ? mapCrs.authid() : QStringLiteral("EPSG:5186");
  const QgsRectangle kr = koreaExtentForCrs(mapAuth);

  if (layerCrs.isValid() && mapCrs.isValid() && extentUsable(ext) &&
      layerCrs.authid() != mapCrs.authid()) {
    try {
      QgsCoordinateTransform xf(layerCrs, mapCrs, QgsProject::instance()
                                                      ? QgsProject::instance()->transformContext()
                                                      : QgsCoordinateTransformContext());
      xf.setBallparkTransformsAreAppropriate(true);
      ext = xf.transformBoundingBox(ext);
    } catch (...) {
      if (qobject_cast<QgsRasterLayer*>(layer)) {
        zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
        refreshCanvasIfIdle(canvas);
        return true;
      }
      return false;
    }
  }

  const bool worldLike = !extentUsable(ext) ||
                         (!kr.isNull() && kr.isFinite() &&
                          (ext.width() > kr.width() * 1.2 || ext.height() > kr.height() * 1.2));
  if (qobject_cast<QgsRasterLayer*>(layer) && worldLike) {
    zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
    refreshCanvasIfIdle(canvas);
    return true;
  }

  if (!extentUsable(ext))
    return false;

  if (!kr.isEmpty() && kr.isFinite() && (ext.width() > kr.width() * 1.2 || ext.height() > kr.height() * 1.2)) {
    zoomCanvasToWorkingScale(canvas, mapAuth, 50000.0);
    refreshCanvasIfIdle(canvas);
    return true;
  }

  const double minW = mapCrs.isGeographic() ? 0.004 : 80.0;
  if (ext.width() < minW || ext.height() < minW) {
    const QgsPointXY c = ext.center();
    const double pad = minW * 0.5;
    ext = QgsRectangle(c.x() - pad, c.y() - pad, c.x() + pad, c.y() + pad);
  }
  ext.scale(1.15);
  canvas->setExtent(ext);
  canvas->zoomToFeatureExtent(ext);
  const QgsRectangle after = canvas->extent();
  if (!kr.isEmpty() && kr.isFinite() &&
      (after.width() > kr.width() * 1.12 || after.height() > kr.height() * 1.12))
    clampCanvasToKorea(canvas);
  refreshCanvasIfIdle(canvas);
  return true;
}

bool LayerOps::isolateAndZoomToLayer(QgsProject* project, QgsMapCanvas* canvas, QgsMapLayer* layer,
                                     bool keepReference) {
  if (!layer || !layer->isValid()) return false;
  if (canvas && !zoomToLayerMax(canvas, layer))
    return false;

  if (project) {
    if (QgsLayerTree* root = project->layerTreeRoot()) {
      for (QgsMapLayer* l : project->mapLayers()) {
        if (!l) continue;
        QgsLayerTreeLayer* n = root->findLayer(l->id());
        if (!n) continue;
        const bool show =
            (l == layer) ||
            (keepReference && !isReferenceLayer(layer) && isReferenceLayer(l));
        n->setItemVisibilityChecked(show);
      }
    }
  }
  if (canvas) {
    QList<QgsMapLayer*> vis;
    vis.append(layer);
    if (project && keepReference && !isReferenceLayer(layer)) {
      for (QgsMapLayer* l : project->mapLayers()) {
        if (l && l != layer && l->isValid() && isReferenceLayer(l))
          vis.append(l);
      }
    }
    canvas->setLayers(vis);
    refreshCanvasIfIdle(canvas);
  }
  return true;
}

void LayerOps::zoomToFullMax(QgsMapCanvas* canvas) {
  if (!canvas) return;
  const QString auth = canvas->mapSettings().destinationCrs().isValid()
                           ? canvas->mapSettings().destinationCrs().authid()
                           : QStringLiteral("EPSG:5186");
  LayerOps::zoomToKorea(canvas, auth);
  LayerOps::clampCanvasToKorea(canvas);
}

static void syncCanvasToProject(QgsProject* project, QgsMapCanvas* canvas) {
  LayerOps::syncMapCanvas(project, canvas, false);
}

static bool isFatalVworldAuthError(const QString& raw) {
  return raw.contains(QStringLiteral("INVALID_KEY"), Qt::CaseInsensitive) ||
         raw.contains(QStringLiteral("등록되지 않은")) ||
         raw.contains(QStringLiteral("인증키")) ||
         raw.contains(QStringLiteral("인증URL 불일치"));
}

static QString friendlyBasemapError(const QString& raw) {
  const QString r = raw;
  if (isFatalVworldAuthError(r) ||
      r.contains(QStringLiteral("InvalidParameterValue"), Qt::CaseInsensitive)) {
    return QStringLiteral(
        "등록되지 않은 VWorld 키입니다. 도움말 → VWorld API 키 설정에서 확인하세요.");
  }
  return r;
}

static QgsRasterLayer* tryCreateXyzLayer(const QString& url, const QString& name, QString* errDetail) {
  auto* rl = new QgsRasterLayer(url, name, QStringLiteral("wms"));
  if (rl->isValid()) return rl;
  if (errDetail) *errDetail = friendlyBasemapError(rl->error().message());
  delete rl;
  return nullptr;
}

static bool addXyzBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& url,
                          const QString& name, QString* errorOut, bool crispText = false) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  LayerOps::ensureTileNetworkIdentity();

  {
    QStringList removeIds;
    for (QgsMapLayer* old : project->mapLayers()) {
      if (!old) continue;
      const QString n = old->name();
      if (legendTitlesMatch(n, name))
        removeIds.append(old->id());
    }
    for (const QString& id : removeIds)
      project->removeMapLayer(id);
  }

  QString detail;
  QgsRasterLayer* rl = tryCreateXyzLayer(url, name, &detail);
  if (!rl) {
    if (errorOut) {
      *errorOut = QStringLiteral("Basemap 실패 (%1): %2").arg(name, detail.isEmpty()
                                                                   ? QStringLiteral("invalid wms/xyz layer")
                                                                   : detail);
    }
    return false;
  }
  tuneBasemapLayer(rl, crispText);
  LayerOps::markReferenceLayer(rl);
  QgsMapLayer* added = project->addMapLayer(rl, false);
  if (!added) {
    if (errorOut)
      *errorOut = QStringLiteral("Basemap 실패 (%1): addMapLayer rejected").arg(name);
    delete rl;
    return false;
  }
  LayerOps::applyLegendCrsLabel(added);
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    QgsLayerTreeLayer* node = root->addLayer(added);
    if (node) {
      node->setItemVisibilityChecked(true);
    }
  }
  LayerOps::ensureSatelliteAtBottom(project);
  LayerOps::pruneEmptyLegendGroups(project);
  if (project->mapLayer(added->id()) == nullptr) {
    if (errorOut)
      *errorOut = QStringLiteral("Basemap 실패 (%1): layer removed after add").arg(name);
    return false;
  }
  if (canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    const QgsRectangle before = canvas->extent();
    const QgsPointXY centerBefore = before.center();
    const bool keepCenter = !before.isEmpty() && before.isFinite() && before.width() > 0 &&
                            canvas->scale() > 50.0 && canvas->scale() < 500000.0;
    const bool needScaleOnly = canvas->extent().isEmpty() || !canvas->extent().isFinite() ||
                               canvas->scale() > 400000.0 || canvas->scale() < 100.0;
    syncCanvasToProject(project, canvas);
    if (keepCenter) {
      canvas->setCenter(centerBefore);
      if (canvas->scale() > 80000.0)
        canvas->zoomScale(10000.0, true);
    } else if (needScaleOnly) {
      zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
    }
    LayerOps::refreshXyzBasemapTiles(canvas);
  }
  return project->mapLayer(added->id()) != nullptr;
}

bool LayerOps::addOsmBasemap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  LayerOps::ensureTileNetworkIdentity();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://tile.openstreetmap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://tile.openstreetmap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=19&zmin=0&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://basemaps.cartocdn.com/light_all/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=20&zmin=0&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://a.basemaps.cartocdn.com/light_all/%7Bz%7D/%7Bx%7D/%7By%7D.png&zmax=20&zmin=0&tilePixelRatio=1"),
  };
  QString lastErr;
  for (int i = 0; i < uris.size(); ++i) {
    const QString layerName = (i >= 2) ? QStringLiteral("Carto Light") : QStringLiteral("OSM");
    QString err;
    if (addXyzBasemap(project, canvas, uris.at(i), layerName, &err)) {
      if (canvas) {
        syncCanvasToProject(project, canvas);
        LayerOps::zoomToKorea(canvas, project && project->crs().isValid()
                                          ? project->crs().authid()
                                          : QStringLiteral("EPSG:5186"));
      }
      if (errorOut && i >= 2)
        *errorOut = QStringLiteral("OSM 대체: Carto Light 사용");
      return true;
    }
    lastErr = err;
  }
  if (errorOut) *errorOut = lastErr.isEmpty() ? QStringLiteral("OSM/Carto 타일 레이어 생성 실패") : lastErr;
  return false;
}

static bool requireVworldKey(const QString& apiKey, QString* errorOut) {
  if (!apiKey.trimmed().isEmpty()) return true;
  if (errorOut) {
    *errorOut = QStringLiteral(
        "VWorld API 키가 없습니다. 도움말 → VWorld API 키 설정에서 키를 입력하세요.");
  }
  return false;
}

static bool addBasemapWithFallbacks(QgsProject* project, QgsMapCanvas* canvas,
                                    const QStringList& uris, const QString& name,
                                    QString* errorOut) {
  QString last;
  for (const QString& uri : uris) {
    QString err;
    if (addXyzBasemap(project, canvas, uri, name, &err))
      return true;
    last = err;
  }
  if (errorOut) *errorOut = last;
  return false;
}

bool LayerOps::addVworldBaseMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString key = apiKey.trimmed();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Base/%7Bz%7D/%7By%7D/%7Bx%7D.png"
          "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Base/service/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1"),
  };
  const bool ok =
      addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 배경"), errorOut);
  if (ok && canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    if (canvas->scale() > 200000.0)
      zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
  }
  return ok;
}

void LayerOps::refreshXyzBasemapTiles(QgsMapCanvas* canvas) {
  if (!canvas) return;
  applyCanvasScreenDpi(canvas);
  // 병렬 렌더만 끈다(WMS 중첩 이벤트 루프 AV 방지). 미리보기 작업은 끄지 않는다.
  // 예전에는 여기서 setPreviewJobsEnabled(false)를 불렀는데, 이 함수가 11곳에서
  // 불리는 탓에 화면이 뜨거나 배경지도를 올릴 때마다 미리보기가 다시 꺼졌다.
  // 그러면 화면을 끄는 동안 캔버스에 보여줄 그림이 없어 지도가 꺼졌다 켜진다.
  canvas->setParallelRenderingEnabled(false);
  // Aborting an in-flight WMS job drops TileDownloadManager objects that
  // still finish and call deleteLater — ACCESS_VIOLATION on Windows.
  if (canvas->isDrawing())
    return;
  canvas->refresh();
}

bool LayerOps::addVworldSatelliteMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  const QString key = apiKey.trimmed();
  QStringList uris;
  if (!key.isEmpty()) {
    uris << QStringLiteral(
                "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
                "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1&http-header:referer=https://localhost")
                .arg(key);
    uris << QStringLiteral(
                "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
                "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1")
                .arg(key);
  }
  uris << QStringLiteral(
      "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg"
      "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1&http-header:referer=https://localhost");
  uris << QStringLiteral(
      "type=xyz&url=https://xdworld.vworld.kr/2d/Satellite/service/%7Bz%7D/%7Bx%7D/%7By%7D.jpeg"
      "&zmax=19&zmin=6&crs=EPSG:3857&tilePixelRatio=1");
  const bool ok =
      addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 위성"), errorOut);
  if (ok && canvas) {
    const QString workAuth = project && project->crs().isValid()
                                 ? project->crs().authid()
                                 : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    zoomCanvasToWorkingScale(canvas, workAuth, 50000.0);
    refreshXyzBasemapTiles(canvas);
  }
  return ok;
}

static QString makeVworldWmsUri(const QString& apiKey, const QString& layers, const QString& styles,
                                const QString& crsAuthId) {
  const QString key = apiKey.trimmed();
  const QString crs = crsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:3857") : crsAuthId.trimmed();
  // GitHub baseline (eac6c9c): KEY/DOMAIN + tiled WMS (tilePixelRatio=2). That path drew parcels.
  const QString baseUrl =
      QStringLiteral("https://api.vworld.kr/req/wms?KEY=%1&DOMAIN=localhost").arg(key);
  const QString encUrl = QString::fromLatin1(QUrl::toPercentEncoding(baseUrl));
  const QString stylePart = styles.isEmpty() ? QStringLiteral("styles")
                                             : QStringLiteral("styles=%1").arg(styles);
  return QStringLiteral(
             "IgnoreGetMapUrl=1&IgnoreGetFeatureInfoUrl=1&contextualWMSLegend=0"
             "&crs=%1&dpiMode=7&format=image/png&transparent=true&featureCount=10"
             "&tilePixelRatio=2&stepWidth=512&stepHeight=512"
             "&layers=%2&%3&url=%4")
      .arg(crs, layers, stylePart, encUrl);
}

static bool addGdalVworldCadastral(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey,
                                   const QString& layers, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("No project");
    return false;
  }
  const QString xml = QStringLiteral(
                          "<GDAL_WMS>"
                          "<Service name=\"WMS\">"
                          "<Version>1.3.0</Version>"
                          "<ServerUrl>https://api.vworld.kr/req/wms?key=%1&amp;domain=localhost&amp;</ServerUrl>"
                          "<Layers>%2</Layers>"
                          "<Styles>lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun</Styles>"
                          "<CRS>EPSG:3857</CRS>"
                          "<ImageFormat>image/png</ImageFormat>"
                          "<Transparent>TRUE</Transparent>"
                          "<BBoxOrder>xyXY</BBoxOrder>"
                          "</Service>"
                          "<DataWindow>"
                          "<UpperLeftX>13500000</UpperLeftX>"
                          "<UpperLeftY>4800000</UpperLeftY>"
                          "<LowerRightX>14800000</LowerRightX>"
                          "<LowerRightY>3800000</LowerRightY>"
                          "<SizeX>16384</SizeX>"
                          "<SizeY>16384</SizeY>"
                          "</DataWindow>"
                          "<Projection>EPSG:3857</Projection>"
                          "<BandsCount>4</BandsCount>"
                          "<BlockSizeX>512</BlockSizeX>"
                          "<BlockSizeY>512</BlockSizeY>"
                          "<UserAgent>Mozilla/5.0 ka-hgis/0.3</UserAgent>"
                          "<Referer>https://localhost</Referer>"
                          "</GDAL_WMS>")
                          .arg(apiKey.trimmed(), layers);
  const QString xmlPath = QDir::temp().filePath(QStringLiteral("ka-hgis-vworld-cadastral.xml"));
  {
    QFile f(xmlPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      if (errorOut) *errorOut = QStringLiteral("지적 설정 파일을 쓰지 못했습니다.");
      return false;
    }
    f.write(xml.toUtf8());
  }

  const QString name = QStringLiteral("VWorld 지적(본번·부번)");
  QStringList removeIds;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (!old) continue;
    const QString n = old->name();
    if (n == name || n.startsWith(name + QLatin1String(" [")))
      removeIds.append(old->id());
  }
  for (const QString& id : removeIds)
    project->removeMapLayer(id);

  auto* rl = new QgsRasterLayer(xmlPath, name, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut)
      *errorOut = friendlyBasemapError(rl->error().message());
    delete rl;
    return false;
  }
  // OTF only works if the layer CRS is the server CRS (3857), not the work CRS.
  rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  LayerOps::markReferenceLayer(rl);
  rl->setOpacity(1.0);
  QgsMapLayer* added = project->addMapLayer(rl, true);
  if (!added) {
    delete rl;
    if (errorOut) *errorOut = QStringLiteral("지적 레이어를 프로젝트에 넣지 못했습니다.");
    return false;
  }
  LayerOps::applyLegendCrsLabel(added);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    syncCanvasToProject(project, canvas);
    if (canvas->scale() > 80000.0 || canvas->scale() < 200.0)
      zoomCanvasToWorkingScale(canvas, workAuth, 25000.0);
    LayerOps::refreshXyzBasemapTiles(canvas);
  }
  return true;
}

QStringList LayerOps::cadastralWmsCrsCandidates(const QString& workCrsAuthId) {
  Q_UNUSED(workCrsAuthId);
  // Do not put 5186/5187/5179 here. Caps only list 4326 bbox; QGIS then
  // reports "Cannot calculate extent" and the layer never draws.
  return {
      QStringLiteral("EPSG:4326"),
      QStringLiteral("EPSG:3857"),
      QStringLiteral("EPSG:900913"),
  };
}

bool LayerOps::addVworldCadastralMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString workCrs = (project && project->crs().isValid())
                              ? project->crs().authid()
                              : QStringLiteral("EPSG:5186");
  // Restore GitHub baseline path that drew parcels: EPSG:3857 tiled WMS, empty styles.
  const QString uriBoth = makeVworldWmsUri(
      apiKey, QStringLiteral("lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun"), QString(), QStringLiteral("EPSG:3857"));
  const QString uriBon =
      makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bonbun"), QString(), QStringLiteral("EPSG:3857"));
  const QString uriBu =
      makeVworldWmsUri(apiKey, QStringLiteral("lp_pa_cbnd_bubun"), QString(), QStringLiteral("EPSG:3857"));

  QString err;
  bool ok = addXyzBasemap(project, canvas, uriBoth, QStringLiteral("VWorld 지적(본번·부번)"), &err, true);
  if (!ok) {
    const bool okBon = addXyzBasemap(project, canvas, uriBon, QStringLiteral("VWorld 지적 본번"), &err, true);
    const bool okBu = addXyzBasemap(project, canvas, uriBu, QStringLiteral("VWorld 지적 부번"), &err, true);
    ok = okBon || okBu;
  }
  if (!ok)
    ok = addGdalVworldCadastral(project, canvas, apiKey,
                                QStringLiteral("lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun"), &err);
  if (!ok) {
    if (errorOut)
      *errorOut = err.isEmpty() ? QStringLiteral("지적도 WMS 추가 실패") : err;
    return false;
  }
  if (canvas) {
    LayerOps::ensureOtfEnabled(project, canvas, workCrs);
    for (QgsMapLayer* l : project->mapLayers()) {
      if (!l) continue;
      const QString n = l->name();
      const bool vworldCad = (n.contains(QStringLiteral("VWorld")) && n.contains(QStringLiteral("지적"))) ||
                             n == QLatin1String("지적") || n.startsWith(QLatin1String("지적 본번")) ||
                             n.startsWith(QLatin1String("지적 부번")) || n.startsWith(QLatin1String("지적("));
      if (vworldCad)
        l->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
    }
    LayerOps::syncMapCanvas(project, canvas, false);
    const double s = canvas->scale();
    if (s > 80000.0 || s < 200.0)
      canvas->zoomScale(25000.0, true);
    LayerOps::refreshXyzBasemapTiles(canvas);
  }
  return true;
}

double LayerOps::suggestCadastralScale(double currentScale, double target, double maxOk) {
  if (currentScale <= 0.0) return target;
  if (currentScale > maxOk) return target;
  return currentScale;
}

LayerOps::FieldBasemapPackResult LayerOps::prepareFieldBasemapPack(
    QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey,
    const QString& workCrsAuthId, QString* errorOut) {
  FieldBasemapPackResult r;
  if (!requireVworldKey(apiKey, errorOut)) return r;

  const QString work = workCrsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186")
                                                         : workCrsAuthId.trimmed();
  ensureOtfEnabled(project, canvas, work);

  QgsPointXY keepCenter;
  bool hadLocal = false;
  double keepScale = 10000.0;
  if (canvas) {
    const QgsRectangle e = canvas->extent();
    if (!e.isEmpty() && e.isFinite() && e.width() > 0 && canvas->scale() > 50.0 &&
        canvas->scale() < 500000.0) {
      keepCenter = e.center();
      hadLocal = true;
      keepScale = canvas->scale();
    }
  }

  QString satErr;
  r.satelliteOk = addVworldSatelliteMap(project, canvas, apiKey, &satErr);
  QString cadErr;
  r.cadastralOk = addVworldCadastralMap(project, canvas, apiKey, &cadErr);

  if (!r.satelliteOk && !r.cadastralOk) {
    if (errorOut) {
      *errorOut = satErr.isEmpty() ? cadErr : satErr;
      if (errorOut->isEmpty())
        *errorOut = QStringLiteral("현장 배경(위성·지적) 추가 실패");
    }
    return r;
  }

  if (canvas) {
    ensureOtfEnabled(project, canvas, work);
    syncMapCanvas(project, canvas, false);
    if (hadLocal) {
      canvas->setCenter(keepCenter);
      const double next = suggestCadastralScale(keepScale, 5000.0, 15000.0);
      canvas->zoomScale(next, true);
    } else {
      const double next = suggestCadastralScale(canvas->scale(), 4000.0, 5000.0);
      if (qAbs(next - canvas->scale()) > 1.0)
        canvas->zoomScale(next, true);
    }
    clampCanvasToKorea(canvas);
    refreshCanvasIfIdle(canvas);
  }
  if (errorOut && (!r.satelliteOk || !r.cadastralOk)) {
    QStringList parts;
    if (!r.satelliteOk && !satErr.isEmpty()) parts << satErr;
    if (!r.cadastralOk && !cadErr.isEmpty()) parts << cadErr;
    *errorOut = parts.join(QStringLiteral(" / "));
  }
  return r;
}

bool LayerOps::addVworldHybridMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QString key = apiKey.trimmed();
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Hybrid/%7Bz%7D/%7By%7D/%7Bx%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857&tilePixelRatio=1")
          .arg(key),
      QStringLiteral(
          "type=xyz&url=https://xdworld.vworld.kr/2d/Hybrid/service/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=19&zmin=0&crs=EPSG:3857&tilePixelRatio=1"),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 하이브리드"), errorOut);
}

bool LayerOps::addVworldContourMap(QgsProject* project, QgsMapCanvas* canvas, const QString& apiKey, QString* errorOut) {
  if (!requireVworldKey(apiKey, errorOut)) return false;
  const QStringList uris = {
      makeVworldWmsUri(apiKey, QStringLiteral("lt_c_upisuq"), QStringLiteral("lt_c_upisuq"),
                       QStringLiteral("EPSG:3857")),
      makeVworldWmsUri(apiKey, QStringLiteral("lt_c_upisuq"), QString(), QStringLiteral("EPSG:3857")),
  };
  return addBasemapWithFallbacks(project, canvas, uris, QStringLiteral("VWorld 등고선"), errorOut);
}

bool LayerOps::addElevationHillshadeMap(QgsProject* project, QgsMapCanvas* canvas,
                                        const QString& apiKey, QString* errorOut) {
  Q_UNUSED(apiKey);
  const QString name = QStringLiteral("지형맵");
  // XYZ only. VWorld WMS GetMap + OTF 5186 + pan was crash-20260901-102801
  // (provider_wms deleteLater / QgsRasterProjector).
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://tile.opentopomap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=17&zmin=5&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://a.tile.opentopomap.org/%7Bz%7D/%7Bx%7D/%7By%7D.png"
          "&zmax=17&zmin=5&crs=EPSG:3857&tilePixelRatio=1"),
  };
  if (!addBasemapWithFallbacks(project, canvas, uris, name, errorOut))
    return false;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (l && legendTitlesMatch(l->name(), name))
      LayerOps::placeInLegendGroup(project, l, QStringLiteral("참조 지도"));
  }
  return true;
}

static void removeLayersNamed(QgsProject* project, const QString& name) {
  if (!project) return;
  QStringList ids;
  for (QgsMapLayer* old : project->mapLayers()) {
    if (old && legendTitlesMatch(old->name(), name))
      ids.append(old->id());
  }
  for (const QString& id : ids)
    project->removeMapLayer(id);
}

static QString copernicusCogVsicurl(int latFloor, int lonFloor) {
  const QString ns = latFloor >= 0
                         ? QStringLiteral("N%1").arg(latFloor, 2, 10, QChar('0'))
                         : QStringLiteral("S%1").arg(-latFloor, 2, 10, QChar('0'));
  const QString ew = lonFloor >= 0
                         ? QStringLiteral("E%1").arg(lonFloor, 3, 10, QChar('0'))
                         : QStringLiteral("W%1").arg(-lonFloor, 3, 10, QChar('0'));
  const QString tile = QStringLiteral("Copernicus_DSM_COG_10_%1_00_%2_00_DEM").arg(ns, ew);
  return QStringLiteral("/vsicurl/https://copernicus-dem-30m.s3.amazonaws.com/%1/%1.tif").arg(tile);
}

QString LayerOps::copernicusCogUriForWgs84(double latDeg, double lonDeg) {
  const int latFloor = static_cast<int>(std::floor(latDeg));
  const int lonFloor = static_cast<int>(std::floor(lonDeg));
  return copernicusCogVsicurl(latFloor, lonFloor);
}

static bool tryAddCopernicusViewDem(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  if (!project || !canvas) return false;
  QgsRectangle ext = canvas->extent();
  if (ext.isEmpty() || !ext.isFinite()) return false;
  QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem canvasCrs = canvas->mapSettings().destinationCrs();
  QgsRectangle wgsExt = ext;
  if (canvasCrs.isValid() && canvasCrs != wgs) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, wgs, QgsCoordinateTransformContext());
      wgsExt = tr.transformBoundingBox(ext);
    } catch (...) {
      return false;
    }
  }
  const QgsPointXY c = wgsExt.center();
  int latFloor = static_cast<int>(std::floor(c.y()));
  int lonFloor = static_cast<int>(std::floor(c.x()));
  if (latFloor < -90 || latFloor > 89 || lonFloor < -180 || lonFloor > 179)
    return false;
  CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");
  const QString uri = copernicusCogVsicurl(latFloor, lonFloor);
  auto* rl = new QgsRasterLayer(uri, QStringLiteral("DEM"), QStringLiteral("gdal"));
  if (!rl->isValid() || rl->bandCount() < 1) {
    if (errorOut) *errorOut = rl->error().message();
    delete rl;
    return false;
  }
  QgsRectangle statsExt;
  if (rl->crs().isValid() && canvasCrs.isValid() && rl->crs() != canvasCrs) {
    try {
      const QgsCoordinateTransform tr(canvasCrs, rl->crs(), QgsCoordinateTransformContext());
      statsExt = tr.transformBoundingBox(ext);
    } catch (...) {
    }
  } else if (rl->crs() == canvasCrs) {
    statsExt = ext;
  }
  LayerOps::applyDemElevationStyle(rl, statsExt);
  LayerOps::markReferenceLayer(rl);
  removeLayersNamed(project, QStringLiteral("DEM"));
  if (!project->addMapLayer(rl, true)) {
    delete rl;
    return false;
  }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  LayerOps::ensureDemRelief(project, rl);
  return true;
}

double LayerOps::demElevationClassStep(double zMin, double zMax) {
  if (!std::isfinite(zMin) || !std::isfinite(zMax) || zMax <= zMin) return 20.0;
  // About 8 legend rows. 5 m steps on a 120 m site made a 24-line tree.
  const double raw = (zMax - zMin) / 8.0;
  const double nice[] = {1.0, 2.0, 5.0, 10.0, 15.0, 20.0, 25.0, 50.0, 100.0, 200.0, 250.0, 500.0};
  for (double s : nice) {
    if (s + 1e-9 >= raw) return s;
  }
  return 500.0;
}

namespace {

QColor demRampColor(double t) {
  const QColor cols[] = {QColor(48, 18, 59),  QColor(33, 102, 172), QColor(67, 170, 139),
                         QColor(201, 219, 87), QColor(253, 174, 97), QColor(165, 0, 38)};
  t = std::clamp(t, 0.0, 1.0);
  const double x = t * 5.0;
  const int i = std::min(4, static_cast<int>(std::floor(x)));
  const double u = x - double(i);
  const QColor& a = cols[i];
  const QColor& b = cols[i + 1];
  return QColor(int(a.red() + (b.red() - a.red()) * u),
                int(a.green() + (b.green() - a.green()) * u),
                int(a.blue() + (b.blue() - a.blue()) * u));
}

}  // namespace

QList<LayerOps::DemElevationClass> LayerOps::buildDemElevationClasses(double zMin, double zMax,
                                                                      int classCount,
                                                                      double stepMeters) {
  if (!std::isfinite(zMin) || !std::isfinite(zMax) || zMax <= zMin) {
    zMin = 0.0;
    zMax = 200.0;
  }
  if (zMax - zMin < 2.0) {
    zMin -= 1.0;
    zMax += 1.0;
  }
  double step = stepMeters;
  if (!(step > 0.0)) step = demElevationClassStep(zMin, zMax);
  const double z0 = std::floor(zMin / step) * step;
  double z1 = std::ceil(zMax / step) * step;
  if (z1 <= z0) z1 = z0 + step;
  int n = classCount;
  if (n < 2) n = static_cast<int>(std::lround((z1 - z0) / step));
  if (n < 2) n = 2;
  if (classCount < 2 && n > 8) n = 8;
  if (n > 12) n = 12;
  QList<DemElevationClass> out;
  out.reserve(n);
  for (int i = 1; i <= n; ++i) {
    DemElevationClass c;
    c.lo = z0 + step * double(i - 1);
    const double hi = z0 + step * double(i);
    const bool last = (i == n);
    c.hi = last ? std::numeric_limits<double>::infinity() : hi;
    const double t = n <= 1 ? 0.0 : double(i - 1) / double(n - 1);
    c.color = demRampColor(t);
    c.label = last ? QStringLiteral("%1 m 이상").arg(c.lo, 0, 'f', 0)
                   : QStringLiteral("%1–%2 m").arg(c.lo, 0, 'f', 0).arg(hi, 0, 'f', 0);
    out.append(c);
  }
  return out;
}

QList<LayerOps::DemElevationClass> LayerOps::readDemElevationClasses(const QgsRasterLayer* layer) {
  QList<DemElevationClass> out;
  if (!layer) return out;
  auto* rend = dynamic_cast<const QgsSingleBandPseudoColorRenderer*>(layer->renderer());
  if (!rend || !rend->shader()) return out;
  auto* fn = dynamic_cast<const QgsColorRampShader*>(rend->shader()->rasterShaderFunction());
  if (!fn) return out;
  const QList<QgsColorRampShader::ColorRampItem> items = fn->colorRampItemList();
  double prev = rend->classificationMin();
  if (!std::isfinite(prev)) prev = 0.0;
  for (int i = 0; i < items.size(); ++i) {
    DemElevationClass c;
    c.lo = prev;
    c.hi = items[i].value;
    c.color = items[i].color;
    c.label = items[i].label;
    out.append(c);
    if (std::isfinite(items[i].value)) prev = items[i].value;
  }
  return out;
}

bool LayerOps::applyDemElevationStyle(QgsRasterLayer* layer) {
  return applyDemElevationStyle(layer, QgsRectangle(), DemElevationStyle());
}

bool LayerOps::applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent) {
  return applyDemElevationStyle(layer, statsExtent, DemElevationStyle());
}

bool LayerOps::applyDemElevationStyle(QgsRasterLayer* layer, const QgsRectangle& statsExtent,
                                      const DemElevationStyle& style) {
  if (!layer || !layer->isValid() || layer->bandCount() < 1) return false;
  QgsRasterDataProvider* dp = layer->dataProvider();
  if (!dp) return false;
  QList<DemElevationClass> classes = style.classes;
  double zMin = 0.0;
  double zMax = 200.0;
  if (classes.isEmpty()) {
    // Sampled min/max (25000) across statsExtent or whole raster if extent is narrow/missing.
    // Last Discrete class is +inf so high peaks above the sample stay colored.
    QgsRasterBandStats st;
    if (!statsExtent.isEmpty() && statsExtent.isFinite()) {
      st = dp->bandStatistics(1, Qgis::RasterBandStatistic::Min | Qgis::RasterBandStatistic::Max,
                              statsExtent, 25000);
    }
    if (!std::isfinite(st.minimumValue) || !std::isfinite(st.maximumValue) ||
        st.maximumValue <= st.minimumValue || (st.maximumValue - st.minimumValue < 150.0)) {
      QgsRasterBandStats wholeSt =
          dp->bandStatistics(1, Qgis::RasterBandStatistic::Min | Qgis::RasterBandStatistic::Max,
                             QgsRectangle(), 25000);
      if (std::isfinite(wholeSt.minimumValue) && std::isfinite(wholeSt.maximumValue) &&
          wholeSt.maximumValue > wholeSt.minimumValue) {
        st = wholeSt;
      }
    }
    zMin = st.minimumValue;
    zMax = st.maximumValue;
    if (zMin < 0.0 && zMin > -10.0) zMin = 0.0;
    if (!std::isfinite(zMin) || !std::isfinite(zMax) || zMax <= zMin) {
      zMin = 0.0;
      zMax = 300.0;
    }
    classes = buildDemElevationClasses(zMin, zMax, style.classCount, style.stepMeters);
  } else {
    zMin = classes.first().lo;
    zMax = zMin;
    for (const DemElevationClass& c : classes) {
      if (std::isfinite(c.lo) && c.lo < zMin) zMin = c.lo;
      if (std::isfinite(c.hi) && c.hi > zMax) zMax = c.hi;
      if (std::isfinite(c.lo) && c.lo > zMax) zMax = c.lo;
    }
    if (!(zMax > zMin)) zMax = zMin + 1.0;
    classes.last().hi = std::numeric_limits<double>::infinity();
  }
  if (classes.size() < 2) return false;
  QList<QgsColorRampShader::ColorRampItem> items;
  for (int i = 0; i < classes.size(); ++i) {
    const DemElevationClass& c = classes[i];
    const bool last = (i == classes.size() - 1);
    const double cut = last ? std::numeric_limits<double>::infinity() : c.hi;
    QString label = c.label.trimmed();
    if (label.isEmpty()) {
      label = last ? QStringLiteral("%1 m 이상").arg(c.lo, 0, 'f', 0)
                   : QStringLiteral("%1–%2 m").arg(c.lo, 0, 'f', 0).arg(c.hi, 0, 'f', 0);
    }
    items.append(QgsColorRampShader::ColorRampItem(cut, c.color, label));
  }
  auto* ramp = new QgsColorRampShader(zMin, zMax);
  ramp->setColorRampType(Qgis::ShaderInterpolationMethod::Discrete);
  ramp->setClassificationMode(Qgis::ShaderClassificationMethod::EqualInterval);
  ramp->setClip(false);
  ramp->setColorRampItemList(items);
  auto* legend = new QgsColorRampLegendNodeSettings();
  legend->setUseContinuousLegend(false);
  legend->setSuffix(QString());
  ramp->setLegendSettings(legend);
  auto* shader = new QgsRasterShader();
  shader->setRasterShaderFunction(ramp);
  auto* rend = new QgsSingleBandPseudoColorRenderer(dp, 1, shader);
  rend->setClassificationMin(zMin);
  rend->setClassificationMax(zMax);
  QgsRasterMinMaxOrigin origin;
  origin.setLimits(Qgis::RasterRangeLimit::NotSet);
  origin.setExtent(Qgis::RasterRangeExtent::WholeRaster);
  rend->setMinMaxOrigin(origin);
  layer->setRenderer(rend);
  if (QgsRasterResampleFilter* rf = layer->resampleFilter()) {
    rf->setZoomedInResampler(new QgsBilinearRasterResampler());
    rf->setZoomedOutResampler(new QgsBilinearRasterResampler());
  }
  layer->setOpacity(0.88);
  layer->triggerRepaint();
  return true;
}

bool LayerOps::addTilePackBasemap(QgsProject* project, QgsMapCanvas* canvas, const QString& path,
                                  const QString& name, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return false;
  }
  if (!QFileInfo::exists(path)) {
    if (errorOut) *errorOut = QStringLiteral("타일팩 파일이 없습니다: %1").arg(path);
    return false;
  }
  removeLayersNamed(project, name);
  auto* rl = new QgsRasterLayer(path, name, QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut)
      *errorOut = QStringLiteral("타일팩을 열 수 없습니다: %1").arg(rl->error().message());
    delete rl;
    return false;
  }
  markReferenceLayer(rl);
  QgsMapLayer* added = project->addMapLayer(rl, true);
  if (!added) {
    if (errorOut) *errorOut = QStringLiteral("타일팩 레이어를 넣지 못했습니다.");
    delete rl;
    return false;
  }
  applyLegendCrsLabel(added);
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    if (QgsLayerTreeLayer* node = root->findLayer(added->id()))
      node->setItemVisibilityChecked(true);
  }
  pruneEmptyLegendGroups(project);
  if (canvas)
    refreshCanvasIfIdle(canvas);
  return true;
}

bool LayerOps::addDemElevationRaster(QgsProject* project, QgsMapCanvas* canvas, const QString& path,
                                     QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return false;
  }
  const QFileInfo fi(path);
  if (!fi.exists()) {
    if (errorOut) *errorOut = QStringLiteral("DEM 파일이 없습니다.");
    return false;
  }
  auto* rl = new QgsRasterLayer(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  if (!rl->isValid() || rl->bandCount() < 1) {
    if (errorOut)
      *errorOut = QStringLiteral("국토지리원 DEM(.img)을 열 수 없습니다: %1")
                      .arg(rl->error().message());
    delete rl;
    return false;
  }
  LayerOps::applyDemElevationStyle(rl);
  LayerOps::markReferenceLayer(rl);
  LayerOps::applyLegendCrsLabel(rl);
  removeLayersNamed(project, QStringLiteral("DEM"));
  if (!project->addMapLayer(rl, true)) {
    delete rl;
    if (errorOut) *errorOut = QStringLiteral("DEM 레이어를 넣지 못했습니다.");
    return false;
  }
  LayerOps::placeInLegendGroup(project, rl, QStringLiteral("참조 지도"));
  LayerOps::ensureDemRelief(project, rl);
  if (canvas && !canvas->isDrawing()) {
    const QgsRectangle e = rl->extent();
    if (!rl->crs().isGeographic() && e.isFinite() && e.width() > 0 && e.width() < 20000.0 &&
        e.height() < 20000.0)
      LayerOps::zoomToLayerMax(canvas, rl);
    else
      LayerOps::syncMapCanvas(project, canvas, false);
  }
  return true;
}

QgsRasterLayer* LayerOps::ensureDemRelief(QgsProject* project, QgsRasterLayer* demLayer) {
  if (!project || !demLayer) return nullptr;
  const QString reliefTitle = QStringLiteral("지형 음영");
  QgsRasterLayer* shade = nullptr;
  for (QgsMapLayer* ml : project->mapLayers()) {
    if (ml && ml->name() == reliefTitle && ml->isValid()) {
      shade = qobject_cast<QgsRasterLayer*>(ml);
      if (shade) break;
    }
  }
  if (!shade) {
    const QString src = demLayer->source();
    const bool isLocal = demLayer->providerType().compare(QLatin1String("gdal"), Qt::CaseInsensitive) == 0 &&
                         !src.startsWith(QLatin1String("/vsicurl"), Qt::CaseInsensitive) &&
                         QFile::exists(src);
    if (isLocal) {
      auto* hs = new QgsRasterLayer(src, reliefTitle, QStringLiteral("gdal"));
      if (hs->isValid()) {
        if (demLayer->crs().isValid()) hs->setCrs(demLayer->crs());
        auto* rend = new QgsHillshadeRenderer(hs->dataProvider(), 1, 315.0, 45.0);
        rend->setZFactor(hs->crs().isGeographic() ? 111120.0 : 3.0);
        rend->setMultiDirectional(true);
        hs->setRenderer(rend);
        shade = hs;
      } else {
        delete hs;
      }
    }
    if (!shade) {
      ensureTileNetworkIdentity();
      const QString uri = QStringLiteral(
          "type=xyz&url=https://server.arcgisonline.com/ArcGIS/rest/services/Elevation/"
          "World_Hillshade/MapServer/tile/%7Bz%7D/%7By%7D/%7Bx%7D"
          "&zmax=16&zmin=1&crs=EPSG:3857&tilePixelRatio=1");
      auto* rl = new QgsRasterLayer(uri, reliefTitle, QStringLiteral("wms"));
      if (rl->isValid()) {
        rl->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
        shade = rl;
      } else {
        delete rl;
      }
    }
    if (!shade && demLayer->bandCount() >= 1) {
      auto* hs = new QgsRasterLayer(demLayer->source(), reliefTitle, demLayer->providerType());
      if (hs->isValid()) {
        if (demLayer->crs().isValid()) hs->setCrs(demLayer->crs());
        auto* rend = new QgsHillshadeRenderer(hs->dataProvider(), 1, 315.0, 45.0);
        rend->setZFactor(hs->crs().isGeographic() ? 111120.0 : 3.0);
        rend->setMultiDirectional(true);
        hs->setRenderer(rend);
        shade = hs;
      } else {
        delete hs;
      }
    }
    if (!shade) return nullptr;

    shade->setBlendMode(QPainter::CompositionMode_Multiply);
    shade->setOpacity(0.55);
    shade->setCustomProperty(QStringLiteral("ka_hgis/omit_sheet_legend"), true);
    LayerOps::markReferenceLayer(shade);
    // addToLegend = false prevents adding to root then deleting, which would trigger
    // QgsLayerTreeRegistryBridge and destroy the layer.
    if (!project->addMapLayer(shade, false)) {
      delete shade;
      return nullptr;
    }
    QgsLayerTree* root = project->layerTreeRoot();
    if (root) {
      QgsLayerTreeLayer* demNode = root->findLayer(demLayer->id());
      auto* parent = demNode ? qobject_cast<QgsLayerTreeGroup*>(demNode->parent()) : nullptr;
      if (!parent) parent = root;
      const int demIdx = demNode ? parent->children().indexOf(demNode) : 0;
      parent->insertLayer(demIdx < 0 ? 0 : demIdx, shade);
      if (QgsLayerTreeLayer* shadeNode = root->findLayer(shade->id())) {
        shadeNode->setItemVisibilityChecked(true);
      }
    }
  }

  shade->setBlendMode(QPainter::CompositionMode_Multiply);
  shade->setOpacity(0.55);
  if (QgsLayerTree* root = project->layerTreeRoot()) {
    if (QgsLayerTreeLayer* shadeNode = root->findLayer(shade->id()))
      shadeNode->setItemVisibilityChecked(true);
  }
  shade->triggerRepaint();
  return shade;
}

bool LayerOps::addDemColorReliefMap(QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  const QString name = QStringLiteral("DEM");
  // One-click: Copernicus GLO-30 /vsicurl/copernicus-dem + applyDemElevationStyle
  // (meter legend). GIBS RGB stays fallback when the COG cannot open.
  if (tryAddCopernicusViewDem(project, canvas, errorOut))
    return true;
  // NASA GIBS ASTER GDEM color + hillshade. XYZ/WMTS REST uses z/y/x.
  // Never VWorld WMS GetMap (crash-20260901-102801).
  const QStringList uris = {
      QStringLiteral(
          "type=xyz&url=https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/"
          "ASTER_GDEM_Color_Shaded_Relief/default/GoogleMapsCompatible_Level12/"
          "%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
          "&zmax=12&zmin=2&crs=EPSG:3857&tilePixelRatio=1"),
      QStringLiteral(
          "type=xyz&url=https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/"
          "ASTER_GDEM_Color_Shaded_Relief/default/2000-01-01/GoogleMapsCompatible_Level12/"
          "%7Bz%7D/%7By%7D/%7Bx%7D.jpeg"
          "&zmax=12&zmin=2&crs=EPSG:3857&tilePixelRatio=1"),
  };
  if (!addBasemapWithFallbacks(project, canvas, uris, name, errorOut))
    return false;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (l && legendTitlesMatch(l->name(), name))
      LayerOps::placeInLegendGroup(project, l, QStringLiteral("참조 지도"));
  }
  return true;
}

// 토양도 참조 스타일. categoryField 값별 반투명 채움 + 옅은 외곽선.
// 값이 수백 개면(토양부호 등) 범례가 무의미해지므로 단색으로 떨어진다.
static void applySoilCategoryStyle(QgsVectorLayer* layer, const QString& categoryField) {
  if (!layer || !layer->isValid()) return;
  const Qgis::GeometryType gt = layer->geometryType();

  auto makeSymbol = [gt](const QColor& c) -> QgsSymbol* {
    if (gt == Qgis::GeometryType::Polygon) {
      auto fs = QgsFillSymbol::createSimple({
          {QStringLiteral("color"), c.name(QColor::HexArgb)},
          {QStringLiteral("outline_color"), QColor(90, 96, 104, 130).name(QColor::HexArgb)},
          {QStringLiteral("outline_width"), QStringLiteral("0.12")},
          {QStringLiteral("outline_width_unit"), QStringLiteral("MM")},
      });
      return fs.release();
    }
    QgsSymbol* s = QgsSymbol::defaultSymbol(gt);
    if (s) s->setColor(c);
    return s;
  };

  const int fieldIdx = categoryField.isEmpty() ? -1 : layer->fields().indexOf(categoryField);
  if (fieldIdx >= 0) {
    constexpr int kMaxCategories = 200;
    const QSet<QVariant> uniq = layer->uniqueValues(fieldIdx, kMaxCategories + 1);
    if (!uniq.isEmpty() && uniq.size() <= kMaxCategories) {
      QStringList sorted;
      QHash<QString, QVariant> byText;
      for (const QVariant& v : uniq) {
        const QString t = v.toString().trimmed();
        if (t.isEmpty()) continue;
        if (!byText.contains(t)) {
          byText.insert(t, v);
          sorted.append(t);
        }
      }
      sorted.sort();
      QgsCategoryList cats;
      int i = 0;
      for (const QString& t : sorted) {
        // 황금각 색상환: 인접 폴리곤이 비슷한 색으로 붙지 않게 한다.
        const QColor c = QColor::fromHsv((i * 47) % 360, 140, 220, 150);
        if (QgsSymbol* sym = makeSymbol(c))
          cats.append(QgsRendererCategory(byText.value(t), sym, t));
        ++i;
      }
      // 빈 값·미분류는 회색 "기타"로 표시(없으면 해당 폴리곤이 아예 안 그려진다).
      if (QgsSymbol* rest = makeSymbol(QColor(150, 150, 150, 90)))
        cats.append(QgsRendererCategory(QVariant(), rest, QStringLiteral("기타")));
      if (!cats.isEmpty()) {
        layer->setRenderer(new QgsCategorizedSymbolRenderer(categoryField, cats));
        layer->triggerRepaint();
        return;
      }
    }
  }
  if (QgsSymbol* sym = makeSymbol(QColor(189, 183, 107, 110))) {
    layer->setRenderer(new QgsSingleSymbolRenderer(sym));
    layer->triggerRepaint();
  }
}

QgsVectorLayer* LayerOps::addSoilShapefile(QgsProject* project, QgsMapCanvas* canvas,
                                           const QString& path, const QString& crsOverrideAuthId,
                                           const QString& categoryField, QString* errorOut) {
  if (!project) {
    if (errorOut) *errorOut = QStringLiteral("프로젝트가 없습니다.");
    return nullptr;
  }
  const QFileInfo fi(path);
  auto* layer = new QgsVectorLayer(path, fi.completeBaseName(), QStringLiteral("ogr"));
  if (!layer->isValid()) {
    if (errorOut)
      *errorOut = QStringLiteral("토양도 SHP를 열 수 없습니다: %1").arg(layer->error().message());
    delete layer;
    return nullptr;
  }

  // 정부 배포 SHP의 DBF는 대부분 CP949. .cpg가 없으면 한글 속성이 깨진다.
  const QString cpg = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".cpg"));
  if (fi.suffix().compare(QLatin1String("shp"), Qt::CaseInsensitive) == 0 && !QFile::exists(cpg))
    layer->setProviderEncoding(QStringLiteral("CP949"));

  // 흙토람 고시 좌표계 = EPSG:2097(중부원점/Bessel). 사용자가 고른 값이 우선,
  // 아니면 파일 좌표계 유지, 그것도 없으면 2097로 가정한다.
  const QString overrideAuth = crsOverrideAuthId.trimmed();
  if (!overrideAuth.isEmpty())
    layer->setCrs(QgsCoordinateReferenceSystem(overrideAuth));
  else if (!layer->crs().isValid())
    layer->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:2097")));

  applySoilCategoryStyle(layer, categoryField.trimmed());

  LayerOps::markReferenceLayer(layer);
  LayerOps::applyLegendCrsLabel(layer);
  if (!project->addMapLayer(layer, true)) {
    delete layer;
    if (errorOut) *errorOut = QStringLiteral("토양도 레이어를 프로젝트에 넣지 못했습니다.");
    return nullptr;
  }
  LayerOps::placeInLegendGroup(project, layer, QStringLiteral("참조 지도"));
  LayerOps::applyThematicOverlayScaleRange(layer);
  if (canvas) {
    const QString workAuth = project->crs().isValid() ? project->crs().authid()
                                                      : QStringLiteral("EPSG:5186");
    LayerOps::ensureOtfEnabled(project, canvas, workAuth);
    LayerOps::syncMapCanvas(project, canvas, false);
    LayerOps::zoomToLayerMax(canvas, layer);
  }
  return layer;
}

static QList<QgsMapLayer*> layersMatchingBaseName(QgsProject* project, const QString& name) {
  QList<QgsMapLayer*> out;
  if (!project) return out;
  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l) continue;
    const QString n = l->name();
    if (legendTitlesMatch(n, name))
      out.append(l);
  }
  return out;
}

bool LayerOps::setLayerOpacity(QgsProject* project, QgsMapCanvas* canvas, const QString& name, double opacity) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  const double op = qBound(0.0, opacity, 1.0);
  for (QgsMapLayer* l : layers) {
    if (auto* rl = qobject_cast<QgsRasterLayer*>(l)) {
      rl->setOpacity(op);
    }
  }
  refreshCanvasIfIdle(canvas);
  return true;
}

bool LayerOps::toggleLayerVisibility(QgsProject* project, QgsMapCanvas* canvas, const QString& name, bool visible) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  for (QgsMapLayer* l : layers) {
    if (!l) continue;
    if (QgsLayerTreeLayer* node = root->findLayer(l->id())) {
      node->setItemVisibilityChecked(visible);
    }
  }
  refreshCanvasIfIdle(canvas);
  return true;
}

bool LayerOps::isLayerVisible(QgsProject* project, const QString& name) {
  if (!project) return false;
  const auto layers = layersMatchingBaseName(project, name);
  if (layers.isEmpty()) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  for (QgsMapLayer* l : layers) {
    if (!l) continue;
    if (QgsLayerTreeLayer* node = root->findLayer(l->id())) {
      if (node->itemVisibilityChecked()) return true;
    }
  }
  return false;
}

void LayerOps::refreshCanvasIfIdle(QgsMapCanvas* canvas) {
  if (!canvas || canvas->isDrawing()) return;
  // 병렬 렌더만 끈다. 미리보기는 유지해야 팬·줌에서 지도가 안 꺼진다.
  canvas->setParallelRenderingEnabled(false);
  canvas->refresh();
}

bool LayerOps::addKoreaBasemap(QgsProject* project, QgsMapCanvas* canvas, KoreaBasemap kind,
                               QString* errorOut) {
  const QString vworldKey = VworldSettings::loadApiKey();
  switch (kind) {
  case KoreaBasemap::VWorldBase:
    return addVworldBaseMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::VWorldSatellite:
    return addVworldSatelliteMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::VWorldHybrid:
    return addVworldHybridMap(project, canvas, vworldKey, errorOut);
  case KoreaBasemap::GoogleRoad: {
    const QString url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Dm%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D"
        "&zmax=20&zmin=0&crs=EPSG:3857");
    return addXyzBasemap(project, canvas, url, QStringLiteral("Google 도로"), errorOut);
  }
  case KoreaBasemap::GoogleSatellite: {
    const QString url = QStringLiteral(
        "type=xyz&url=https://mt1.google.com/vt/lyrs%3Ds%26x%3D%7Bx%7D%26y%3D%7By%7D%26z%3D%7Bz%7D"
        "&zmax=20&zmin=0&crs=EPSG:3857");
    return addXyzBasemap(project, canvas, url, QStringLiteral("Google 위성"), errorOut);
  }
  case KoreaBasemap::Osm:
  default:
    return addOsmBasemap(project, canvas, errorOut);
  }
}

bool LayerOps::ensureOtfEnabled(QgsProject* project, QgsMapCanvas* canvas, const QString& workCrsAuthId) {
  const QString auth = workCrsAuthId.trimmed().isEmpty() ? QStringLiteral("EPSG:5186") : workCrsAuthId.trimmed();
  const QgsCoordinateReferenceSystem crs(auth);
  if (!crs.isValid()) return false;

  QgsCoordinateTransformContext ctx;
  if (project) {
    ctx = project->transformContext();
    project->setTransformContext(ctx);
    project->setCrs(crs);
  }
  if (canvas)
    canvas->setDestinationCrs(crs);
  return true;
}

bool LayerOps::setWorkCrs(QgsProject* project, QgsMapCanvas* canvas, const QString& epsgAuthId,
                          QString* errorOut, bool zoomKorea) {
  const QgsCoordinateReferenceSystem crs(epsgAuthId);
  if (!crs.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid CRS %1").arg(epsgAuthId);
    return false;
  }
  QgsRectangle prev;
  QgsCoordinateReferenceSystem prevCrs;
  if (canvas) {
    prev = canvas->extent();
    prevCrs = canvas->mapSettings().destinationCrs();
  }
  ensureOtfEnabled(project, canvas, epsgAuthId);
  if (canvas) {
    if (zoomKorea) {
      zoomToKorea(canvas, epsgAuthId);
    } else if (!prev.isEmpty() && prevCrs.isValid() && prevCrs != crs) {
      try {
        QgsCoordinateTransform xf(prevCrs, crs, project ? project->transformContext()
                                                        : QgsCoordinateTransformContext());
        xf.setBallparkTransformsAreAppropriate(true);
        canvas->setExtent(xf.transformBoundingBox(prev));
      } catch (...) {
        zoomToKorea(canvas, epsgAuthId);
      }
    } else if (!prev.isEmpty()) {
      canvas->setExtent(prev);
    }
    refreshXyzBasemapTiles(canvas);
  }
  return true;
}

QgsRectangle LayerOps::koreaExtentForCrs(const QString& epsgAuthId) {
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem dest(epsgAuthId);
  const QgsRectangle krWgs(124.5, 33.0, 132.0, 39.5);
  if (!dest.isValid()) return krWgs;
  try {
    const QgsCoordinateTransform xf(wgs, dest, QgsCoordinateTransformContext());
    return xf.transformBoundingBox(krWgs);
  } catch (...) {
    return QgsRectangle();
  }
}

QgsRectangle LayerOps::satelliteFillExtentForCrs(const QString& epsgAuthId) {
  const QgsCoordinateReferenceSystem wgs(QStringLiteral("EPSG:4326"));
  const QgsCoordinateReferenceSystem merc(QStringLiteral("EPSG:3857"));
  const QgsCoordinateReferenceSystem dest(epsgAuthId);
  const QgsRectangle krWgs(124.5, 33.0, 132.0, 39.5);
  const QgsCoordinateTransformContext ctx;
  try {
    const QgsCoordinateTransform toMerc(wgs, merc, ctx);
    const QgsRectangle mercRect = toMerc.transformBoundingBox(krWgs);
    if (!dest.isValid() || dest.authid() == QLatin1String("EPSG:3857"))
      return mercRect;
    const QgsCoordinateTransform toDest(merc, dest, ctx);
    QgsPointXY sw(mercRect.xMinimum(), mercRect.yMinimum());
    QgsPointXY se(mercRect.xMaximum(), mercRect.yMinimum());
    QgsPointXY ne(mercRect.xMaximum(), mercRect.yMaximum());
    QgsPointXY nw(mercRect.xMinimum(), mercRect.yMaximum());
    sw = toDest.transform(sw);
    se = toDest.transform(se);
    ne = toDest.transform(ne);
    nw = toDest.transform(nw);
    const double xMin = std::max(sw.x(), nw.x());
    const double xMax = std::min(se.x(), ne.x());
    const double yMin = std::max(sw.y(), se.y());
    const double yMax = std::min(nw.y(), ne.y());
    if (!(xMax > xMin) || !(yMax > yMin))
      return toDest.transformBoundingBox(mercRect);
    return QgsRectangle(xMin, yMin, xMax, yMax);
  } catch (...) {
    return koreaExtentForCrs(epsgAuthId);
  }
}

void LayerOps::applyKoreaMapLimits(QgsProject* project, QgsMapCanvas* canvas) {
  const QString auth = (project && project->crs().isValid())
                           ? project->crs().authid()
                           : (canvas && canvas->mapSettings().destinationCrs().isValid()
                                  ? canvas->mapSettings().destinationCrs().authid()
                                  : QStringLiteral("EPSG:5186"));
  QgsRectangle kr = koreaExtentForCrs(auth);
  if (kr.isEmpty() || !kr.isFinite()) return;
  const QgsCoordinateReferenceSystem crs(auth);
  if (project && crs.isValid() && project->viewSettings()) {
    const QgsReferencedRectangle ref(kr, crs);
    project->viewSettings()->setPresetFullExtent(ref);
    project->viewSettings()->setDefaultViewExtent(ref);
  }
  if (canvas && crs.isValid())
    canvas->setDestinationCrs(crs);
}

static QgsRectangle extentFittedInside(const QgsRectangle& kr, double viewAspect) {
  if (kr.isEmpty() || !kr.isFinite() || viewAspect <= 0.05)
    return kr;
  const double krAspect = kr.width() / kr.height();
  if (viewAspect > krAspect) {
    const double h = kr.width() / viewAspect;
    const double cy = kr.center().y();
    return QgsRectangle(kr.xMinimum(), cy - h * 0.5, kr.xMaximum(), cy + h * 0.5);
  }
  const double w = kr.height() * viewAspect;
  const double cx = kr.center().x();
  return QgsRectangle(cx - w * 0.5, kr.yMinimum(), cx + w * 0.5, kr.yMaximum());
}

static double canvasViewAspect(const QgsMapCanvas* canvas) {
  if (!canvas) return 1.0;
  const QSize out = canvas->mapSettings().outputSize();
  if (out.width() >= 2 && out.height() >= 2)
    return double(out.width()) / double(out.height());
  const int w = qMax(1, canvas->width());
  const int h = qMax(1, canvas->height());
  return double(w) / double(h);
}

bool LayerOps::clampCanvasToKorea(QgsMapCanvas* canvas) {
  if (!canvas) return false;
  if (canvas->isDrawing()) return false;
  const QString auth = canvas->mapSettings().destinationCrs().isValid()
                           ? canvas->mapSettings().destinationCrs().authid()
                           : QStringLiteral("EPSG:5186");
  const QgsRectangle kr = satelliteFillExtentForCrs(auth);
  if (kr.isEmpty() || !kr.isFinite()) return false;

  const QgsRectangle fitted = extentFittedInside(kr, canvasViewAspect(canvas));
  const QgsRectangle cur = canvas->extent();
  if (cur.isEmpty() || !cur.isFinite()) {
    canvas->setExtent(fitted);
    return true;
  }

  const double tol = qMax(kr.width(), kr.height()) * 0.03;
  const bool alreadySnapped =
      qAbs(cur.center().x() - fitted.center().x()) < tol &&
      qAbs(cur.center().y() - fitted.center().y()) < tol &&
      cur.width() <= fitted.width() * 1.05 + 1.0 &&
      cur.height() <= fitted.height() * 1.05 + 1.0;

  // VWorld 위성은 한반도만. 한국보다 넓게 줌아웃하면 타일이 잘린 사각형으로 보인다.
  if (cur.width() > kr.width() || cur.height() > kr.height()) {
    if (alreadySnapped)
      return false;
    canvas->setExtent(fitted);
    return true;
  }

  double minX = cur.xMinimum();
  double maxX = cur.xMaximum();
  double minY = cur.yMinimum();
  double maxY = cur.yMaximum();
  const double w = cur.width();
  const double h = cur.height();

  if (minX < kr.xMinimum()) {
    minX = kr.xMinimum();
    maxX = minX + w;
  }
  if (maxX > kr.xMaximum()) {
    maxX = kr.xMaximum();
    minX = maxX - w;
  }
  if (minY < kr.yMinimum()) {
    minY = kr.yMinimum();
    maxY = minY + h;
  }
  if (maxY > kr.yMaximum()) {
    maxY = kr.yMaximum();
    minY = maxY - h;
  }

  const QgsRectangle clamped(minX, minY, maxX, maxY);
  const double eps = qMax(kr.width(), kr.height()) * 1e-9;
  if (qAbs(clamped.xMinimum() - cur.xMinimum()) > eps ||
      qAbs(clamped.yMinimum() - cur.yMinimum()) > eps ||
      qAbs(clamped.xMaximum() - cur.xMaximum()) > eps ||
      qAbs(clamped.yMaximum() - cur.yMaximum()) > eps) {
    canvas->setExtent(clamped);
    return true;
  }
  return false;
}

void LayerOps::zoomToKorea(QgsMapCanvas* canvas, const QString& epsgAuthId, bool refresh) {
  if (!canvas) return;
  QgsRectangle ext = satelliteFillExtentForCrs(epsgAuthId);
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = koreaExtentForCrs(epsgAuthId);
  }
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = koreaExtentForCrs(QStringLiteral("EPSG:3857"));
    const QgsCoordinateReferenceSystem destCrs = canvas->mapSettings().destinationCrs();
    if (destCrs.isValid() && destCrs.authid() != QLatin1String("EPSG:3857")) {
      try {
        const QgsCoordinateTransform xf(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")),
                                        destCrs, QgsCoordinateTransformContext());
        ext = xf.transformBoundingBox(ext);
      } catch (...) {
      }
    }
  }
  if (ext.isEmpty() || !ext.isFinite()) {
    ext = QgsRectangle(124.5, 33.0, 132.0, 39.5);
  }
  canvas->setExtent(extentFittedInside(ext, canvasViewAspect(canvas)));
  if (!canvas->renderFlag())
    canvas->setRenderFlag(true);
  if (!refresh) return;
  canvas->freeze(false);
  refreshCanvasIfIdle(canvas);
}

QString LayerOps::convertToShp5179(QgsVectorLayer* layer, const QString& outShpPath,
                                   QgsProject* project, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Invalid layer");
    return {};
  }
  QString path = outShpPath;
  if (!path.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive))
    path += QStringLiteral(".shp");
  return reprojectVectorLayer(layer, QStringLiteral("EPSG:5179"), path, project, errorOut);
}

QString LayerOps::convertFileToShp5179(const QString& inPath, const QString& outShpPath,
                                       QgsProject* project, QString* errorOut) {
  if (!QFile::exists(inPath)) {
    if (errorOut) *errorOut = QStringLiteral("Input not found");
    return {};
  }
  auto* vl = new QgsVectorLayer(inPath, QFileInfo(inPath).completeBaseName(), QStringLiteral("ogr"));
  if (!vl->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Cannot open: %1").arg(inPath);
    delete vl;
    return {};
  }
  const QString out = convertToShp5179(vl, outShpPath, project, errorOut);
  delete vl;
  return out;
}

QString LayerOps::georeferenceImageSimple(const QString& imagePath, QgsVectorLayer* controlPoints,
                                          QgsProject* project, QgsMapCanvas* canvas, QString* errorOut) {
  if (!QFile::exists(imagePath)) {
    if (errorOut) *errorOut = QStringLiteral("Image not found");
    return {};
  }
  if (!controlPoints || controlPoints->featureCount() < 2) {
    if (errorOut) *errorOut = QStringLiteral("Need >=2 control points");
    return {};
  }
  QImage img(imagePath);
  if (img.isNull()) {
    if (errorOut) *errorOut = QStringLiteral("Cannot read image");
    return {};
  }
  const double w = img.width();
  const double h = img.height();
  if (w < 2 || h < 2) {
    if (errorOut) *errorOut = QStringLiteral("Image too small");
    return {};
  }

  struct Gcp {
    double px = -1, py = -1;
    QgsPointXY map;
    bool hasPixel = false;
  };
  QVector<Gcp> gcps;
  QgsFeatureIterator it = controlPoints->getFeatures();
  QgsFeature f;
  while (it.nextFeature(f)) {
    if (!f.hasGeometry()) continue;
    Gcp g;
    g.map = f.geometry().asPoint();
    const int ix = controlPoints->fields().indexOf(QStringLiteral("pixel_x"));
    const int iy = controlPoints->fields().indexOf(QStringLiteral("pixel_y"));
    if (ix >= 0 && iy >= 0) {
      bool okx = false, oky = false;
      const double px = f.attribute(ix).toDouble(&okx);
      const double py = f.attribute(iy).toDouble(&oky);
      if (okx && oky) {
        g.px = px;
        g.py = py;
        g.hasPixel = true;
      }
    }
    gcps.append(g);
  }
  if (gcps.size() < 2) {
    if (errorOut) *errorOut = QStringLiteral("Control points lack geometry");
    return {};
  }

  double rotA = 0, rotB = 0, rotD = 0, rotE = 0, ulx = 0, uly = 0;
  const bool allPixel = std::all_of(gcps.begin(), gcps.end(), [](const Gcp& g) { return g.hasPixel; })
                        && gcps.size() >= 2;

  bool lsSolved = false;
  if (allPixel && gcps.size() >= 3) {
    double sxx = 0, sxy = 0, sx = 0, syy = 0, sy = 0, sn = 0;
    double sxX = 0, syX = 0, sX = 0, sxY = 0, syY = 0, sY = 0;
    for (const Gcp& g : gcps) {
      const double x = g.px, y = g.py;
      sxx += x * x; sxy += x * y; sx += x;
      syy += y * y; sy += y; sn += 1;
      sxX += x * g.map.x(); syX += y * g.map.x(); sX += g.map.x();
      sxY += x * g.map.y(); syY += y * g.map.y(); sY += g.map.y();
    }
    auto solve3 = [](double a11, double a12, double a13, double a21, double a22, double a23,
                     double a31, double a32, double a33, double b1, double b2, double b3,
                     double& x1, double& x2, double& x3) -> bool {
      const double det = a11 * (a22 * a33 - a23 * a32) - a12 * (a21 * a33 - a23 * a31) + a13 * (a21 * a32 - a22 * a31);
      if (std::abs(det) < 1e-18) return false;
      x1 = (b1 * (a22 * a33 - a23 * a32) - a12 * (b2 * a33 - a23 * b3) + a13 * (b2 * a32 - a22 * b3)) / det;
      x2 = (a11 * (b2 * a33 - a23 * b3) - b1 * (a21 * a33 - a23 * a31) + a13 * (a21 * b3 - b2 * a31)) / det;
      x3 = (a11 * (a22 * b3 - b2 * a32) - a12 * (a21 * b3 - b2 * a31) + b1 * (a21 * a32 - a22 * a31)) / det;
      return true;
    };
    double a = 0, b = 0, c = 0, d = 0, e = 0, fpar = 0;
    if (solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxX, syX, sX, a, b, c)
        && solve3(sxx, sxy, sx, sxy, syy, sy, sx, sy, sn, sxY, syY, sY, d, e, fpar)) {
      rotA = a; rotB = b; rotD = d; rotE = e;
      ulx = c + 0.5 * (rotA + rotB);
      uly = fpar + 0.5 * (rotD + rotE);
      lsSolved = true;
    }
  }

  if (!lsSolved) {
    const QgsPointXY g0 = gcps[0].map;
    const QgsPointXY g1 = gcps[1].map;
    const double dx = g1.x() - g0.x();
    const double dy = g1.y() - g0.y();
    const double dist = std::hypot(dx, dy);
    if (dist < 1e-9) {
      if (errorOut) *errorOut = QStringLiteral("GCP0 and GCP1 too close");
      return {};
    }
    rotA = dx / w;
    rotB = 0.0;
    rotD = 0.0;
    rotE = (std::abs(dy) < 1e-6) ? -std::abs(rotA) : (dy / h);
    if (gcps.size() >= 3) {
      const double dy2 = gcps[2].map.y() - g0.y();
      if (std::abs(dy2) > 1e-6) rotE = dy2 / h;
    }
    ulx = g0.x() + rotA * 0.5;
    uly = g0.y() + rotE * (h - 0.5);
  }

  QString wfPath = imagePath;
  const QString ext = QFileInfo(imagePath).suffix().toLower();
  if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
    wfPath = imagePath.left(imagePath.size() - int(ext.size())) + QStringLiteral("jgw");
  else if (ext == QLatin1String("png"))
    wfPath = imagePath.left(imagePath.size() - 3) + QStringLiteral("pgw");
  else if (ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
    wfPath = imagePath.left(imagePath.size() - int(ext.size())) + QStringLiteral("tfw");
  else
    wfPath = imagePath + QStringLiteral(".wld");

  QFile wf(wfPath);
  if (!wf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot write world file");
    return {};
  }
  QTextStream ts(&wf);
  ts.setEncoding(QStringConverter::Utf8);
  // standard world file 6 lines
  ts << QString::number(rotA, 'g', 16) << "\n";
  ts << QString::number(rotD, 'g', 16) << "\n";
  ts << QString::number(rotB, 'g', 16) << "\n";
  ts << QString::number(rotE, 'g', 16) << "\n";
  ts << QString::number(ulx, 'g', 16) << "\n";
  ts << QString::number(uly, 'g', 16) << "\n";
  wf.close();

  auto* rl = new QgsRasterLayer(imagePath, QFileInfo(imagePath).completeBaseName(), QStringLiteral("gdal"));
  if (!rl->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("Georef raster invalid after worldfile");
    delete rl;
    return {};
  }
  if (project) {
    if (project->crs().isValid()) rl->setCrs(project->crs());
    project->addMapLayer(rl);
  }
  if (canvas) {
    canvas->setExtent(rl->extent());
    canvas->refresh();
  }
  return wfPath;
}

bool LayerOps::undoCommittedFeature(QgsVectorLayer* layer, qint64 featureId, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 되돌릴 수 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid()) {
    if (errorOut) *errorOut = QStringLiteral("되돌릴 도형을 찾지 못했습니다.");
    return false;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  if (!layer->deleteFeature(fid)) {
    if (errorOut) *errorOut = QStringLiteral("도형을 지우지 못했습니다.");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (!layer->commitChanges(false)) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char('\n'));
    layer->rollBack();
    return false;
  }
  if (QgsDataProvider* p = layer->dataProvider())
    p->reloadData();
  if (startedHere)
    layer->startEditing();
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::purgeCommittedFeatures(QgsVectorLayer* layer, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 비울 수 없습니다.");
    return false;
  }
  const QString src = layer->source().split(QLatin1Char('|')).first();
  if (src.endsWith(QLatin1String(".shp"), Qt::CaseInsensitive)) {
    if (errorOut) *errorOut = QStringLiteral("외부 SHP 파일은 물리적으로 도형을 비울 수 없습니다.");
    return false;
  }
  const QString key = layerKeyOf(layer);
  if (key.startsWith(QLatin1String("user:"))) {
    if (errorOut) *errorOut = QStringLiteral("외부 추가 레이어의 원본 파일은 비울 수 없습니다.");
    return false;
  }
  const QgsFeatureIds ids = layer->allFeatureIds();
  if (ids.isEmpty()) {
    if (QgsDataProvider* p = layer->dataProvider())
      p->reloadData();
    layer->updateExtents();
    return true;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  if (!layer->deleteFeatures(ids)) {
    if (errorOut) *errorOut = QStringLiteral("도형을 지우지 못했습니다.");
    if (startedHere) layer->rollBack();
    return false;
  }
  if (!layer->commitChanges()) {
    if (errorOut) *errorOut = layer->commitErrors().join(QLatin1Char('\n'));
    layer->rollBack();
    return false;
  }
  if (QgsVectorDataProvider* p = layer->dataProvider()) {
    if (layer->featureCount() > 0 && !p->deleteFeatures(layer->allFeatureIds())) {
      if (errorOut) *errorOut = QStringLiteral("GPKG에서 도형을 지우지 못했습니다.");
      return false;
    }
    p->reloadData();
  }
  layer->updateExtents();
  layer->triggerRepaint();
  if (layer->featureCount() > 0) {
    if (errorOut) *errorOut = QStringLiteral("지운 뒤에도 도형이 남아 있습니다.");
    return false;
  }
  return true;
}

bool LayerOps::moveFeatureVertex(QgsVectorLayer* layer, qint64 featureId, int vertex,
                                 double x, double y, QString* errorOut) {
  if (!layer || !layer->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("레이어가 없습니다.");
    return false;
  }
  if (isReferenceLayer(layer)) {
    if (errorOut) *errorOut = QStringLiteral("참조 지도는 고칠 수 없습니다.");
    return false;
  }
  if (vertex < 0) {
    if (errorOut) *errorOut = QStringLiteral("꼭짓점이 없습니다.");
    return false;
  }
  const QgsFeatureId fid = static_cast<QgsFeatureId>(featureId);
  QgsFeature existing = layer->getFeature(fid);
  if (!existing.isValid() || !existing.hasGeometry()) {
    if (errorOut) *errorOut = QStringLiteral("고칠 도형을 찾지 못했습니다.");
    return false;
  }
  QgsGeometry g = existing.geometry();
  if (!g.moveVertex(x, y, vertex)) {
    if (errorOut) *errorOut = QStringLiteral("꼭짓점을 옮기지 못했습니다.");
    return false;
  }
  const bool startedHere = !layer->isEditable();
  if (startedHere && !layer->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("편집을 열 수 없습니다.");
    return false;
  }
  if (!layer->changeGeometry(fid, g)) {
    if (errorOut) *errorOut = QStringLiteral("도형을 고치지 못했습니다.");
    if (startedHere) layer->rollBack();
    return false;
  }
  layer->updateExtents();
  layer->triggerRepaint();
  return true;
}

bool LayerOps::hasVisibleReferenceLayer(QgsProject* project) {
  if (!project) return false;
  QgsLayerTree* root = project->layerTreeRoot();
  if (!root) return false;

  for (QgsMapLayer* l : project->mapLayers()) {
    if (!l || !isReferenceLayer(l)) continue;
    QgsLayerTreeLayer* node = root->findLayer(l->id());
    if (node && node->isVisible()) return true;
  }
  return false;
}

bool LayerOps::removeConfirmedLayers(QgsProject* project, QgsMapCanvas* canvas, const QStringList& layerIds) {
  if (!project || layerIds.isEmpty()) return false;
  for (const QString& id : layerIds) {
    project->removeMapLayer(id);
  }
  if (canvas)
    refreshCanvasIfIdle(canvas);
  return true;
}

static QString normalizeCsvHeader(QString h) {
  h = h.trimmed().toLower();
  h.remove(QLatin1Char('"'));
  h.replace(QLatin1Char(' '), QLatin1Char('_'));
  if (h == QLatin1String("id") || h == QLatin1String("point") || h == QLatin1String("pid"))
    return QStringLiteral("point_id");
  if (h == QLatin1String("lon") || h == QLatin1String("easting") || h == QLatin1String("east"))
    return QStringLiteral("x");
  if (h == QLatin1String("lat") || h == QLatin1String("northing") || h == QLatin1String("north"))
    return QStringLiteral("y");
  if (h == QLatin1String("acc") || h == QLatin1String("accuracy"))
    return QStringLiteral("accuracy_m");
  if (h == QLatin1String("fix") || h == QLatin1String("fixtype"))
    return QStringLiteral("fix_type");
  if (h == QLatin1String("proj") || h == QLatin1String("crs"))
    return QStringLiteral("projection");
  return h;
}

int LayerOps::importControlPointsCsv(QgsVectorLayer* controlPoints, const QString& csvPath, QString* errorOut) {
  if (!controlPoints || !controlPoints->isValid()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 레이어가 없습니다. 먼저 새 조사를 만드세요.");
    return -1;
  }
  QFile f(csvPath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("CSV를 열 수 없습니다: %1").arg(csvPath);
    return -1;
  }
  QTextStream ts(&f);
  const QRegularExpression sep(QStringLiteral("[,;\\t]"));

  QStringList headers;
  QList<QStringList> rows;
  while (!ts.atEnd()) {
    const QString line = ts.readLine().trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
    const QStringList parts = line.split(sep);
    if (parts.isEmpty()) continue;
    const QString h0 = normalizeCsvHeader(parts.first());
    if (headers.isEmpty() &&
        (h0 == QLatin1String("point_id") || h0 == QLatin1String("x") ||
         parts.first().trimmed().compare(QStringLiteral("id"), Qt::CaseInsensitive) == 0 ||
         parts.first().trimmed().compare(QStringLiteral("point_id"), Qt::CaseInsensitive) == 0)) {
      for (const QString& p : parts) headers.append(normalizeCsvHeader(p));
      continue;
    }
    if (parts.size() < 3) continue;
    rows.append(parts);
  }
  if (headers.isEmpty()) {
    headers = {QStringLiteral("point_id"), QStringLiteral("x"), QStringLiteral("y"),
               QStringLiteral("datum"), QStringLiteral("ellipsoid"), QStringLiteral("projection"),
               QStringLiteral("accuracy_m"), QStringLiteral("pdop"), QStringLiteral("fix_type")};
  }
  if (rows.isEmpty()) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 가져올 데이터 행이 없습니다.");
    return 0;
  }

  auto col = [&](const QString& name) -> int {
    return headers.indexOf(name);
  };
  const int iId = col(QStringLiteral("point_id"));
  const int iX = col(QStringLiteral("x"));
  const int iY = col(QStringLiteral("y"));
  if (iX < 0 || iY < 0) {
    if (errorOut) *errorOut = QStringLiteral("CSV에 x,y(또는 lon/lat) 열이 필요합니다.");
    return -1;
  }

  if (!controlPoints->isEditable() && !controlPoints->startEditing()) {
    if (errorOut) *errorOut = QStringLiteral("control_points 편집 모드 실패");
    return -1;
  }

  int added = 0;
  for (const QStringList& p : rows) {
    auto cell = [&](int idx) -> QString {
      if (idx < 0 || idx >= p.size()) return {};
      return p.at(idx).trimmed().remove(QLatin1Char('"'));
    };
    bool okX = false, okY = false;
    const double x = cell(iX).toDouble(&okX);
    const double y = cell(iY).toDouble(&okY);
    if (!okX || !okY) continue;

    QgsFeature feat(controlPoints->fields());
    const QString pid = iId >= 0 ? cell(iId) : QStringLiteral("P%1").arg(added + 1);
    auto setStr = [&](const char* field, int idx) {
      const int fi = controlPoints->fields().indexOf(QString::fromUtf8(field));
      if (fi >= 0 && idx >= 0) feat.setAttribute(fi, cell(idx));
    };
    auto setNum = [&](const char* field, int idx) {
      const int fi = controlPoints->fields().indexOf(QString::fromUtf8(field));
      if (fi < 0 || idx < 0) return;
      bool ok = false;
      const double v = cell(idx).toDouble(&ok);
      if (ok) feat.setAttribute(fi, v);
      else if (!cell(idx).isEmpty()) feat.setAttribute(fi, cell(idx));
    };
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("point_id"));
      if (fi >= 0) feat.setAttribute(fi, pid);
    }
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("x"));
      if (fi >= 0) feat.setAttribute(fi, x);
    }
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("y"));
      if (fi >= 0) feat.setAttribute(fi, y);
    }
    setStr("datum", col(QStringLiteral("datum")));
    setStr("ellipsoid", col(QStringLiteral("ellipsoid")));
    setStr("projection", col(QStringLiteral("projection")));
    setStr("origin", col(QStringLiteral("origin")));
    setStr("fix_type", col(QStringLiteral("fix_type")));
    setNum("accuracy_m", col(QStringLiteral("accuracy_m")));
    setNum("pdop", col(QStringLiteral("pdop")));
    {
      const int fi = controlPoints->fields().indexOf(QStringLiteral("accuracy"));
      const int ia = col(QStringLiteral("accuracy_m"));
      if (fi >= 0 && ia >= 0 && !cell(ia).isEmpty()) feat.setAttribute(fi, cell(ia));
    }
    feat.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(x, y)));
    if (controlPoints->addFeature(feat)) ++added;
  }

  if (!controlPoints->commitChanges()) {
    if (errorOut) {
      *errorOut = QStringLiteral("커밋 실패: %1").arg(controlPoints->commitErrors().join(QLatin1Char(';')));
    }
    controlPoints->rollBack();
    return -1;
  }
  return added;
}

