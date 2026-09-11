#include "DemColorRampLegend.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <QApplication>
#include <QFontMetricsF>
#include <QPainter>
#include <QPixmap>
#include <qgscolorramp.h>
#include <qgscolorramplegendnode.h>
#include <qgscolorrampshader.h>
#include <qgslayertreelayer.h>
#include <qgslegendsettings.h>
#include <qgsrasterlayer.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>

namespace {
constexpr double kWidth = 104.;
constexpr double kHeight = 156.;
constexpr double kRampTop = 24.;
constexpr double kRampHeight = 108.;
constexpr double kRampLeft = 5.;
constexpr double kRampWidth = 18.;
const QSizeF kLayoutSize(27., 27. * kHeight / kWidth);

struct RampSnapshot {
  QgsColorRampShader shader;
  QList<double> ticks;
  double minimum = 0.;
  double maximum = 0.;

  // Equal tick intervals keep low-elevation labels readable. Sampling still
  // uses the renderer's actual elevation and shader, never a new palette.
  double elevation(double position) const {
    const double index = std::clamp(position, 0., 1.) * (ticks.size() - 1);
    const int lower = std::min(static_cast<int>(index), static_cast<int>(ticks.size()) - 2);
    return std::lerp(ticks.at(lower), ticks.at(lower + 1), index - lower);
  }
  QColor color(double position) const {
    int r = 0, g = 0, b = 0, a = 0;
    if (!shader.shade(elevation(position), &r, &g, &b, &a)) return Qt::transparent;
    return QColor(r, g, b, a);
  }
};

QString tickText(double value) { return QString::number(value, 'g', 8); }

std::optional<RampSnapshot> snapshot(QgsRasterLayer* layer) {
  if (!layer || !layer->isValid()) return std::nullopt;
  const auto* renderer = dynamic_cast<const QgsSingleBandPseudoColorRenderer*>(layer->renderer());
  if (!renderer || !renderer->shader()) return std::nullopt;
  const auto* shader = dynamic_cast<const QgsColorRampShader*>(renderer->shader()->rasterShaderFunction());
  if (!shader || shader->colorRampType() != Qgis::ShaderInterpolationMethod::Linear) return std::nullopt;
  const auto items = shader->colorRampItemList();
  if (items.size() < 2) return std::nullopt;
  for (qsizetype i = 0; i < items.size(); ++i) {
    if (!std::isfinite(items.at(i).value) || !items.at(i).color.isValid() ||
        (i > 0 && items.at(i).value <= items.at(i - 1).value)) return std::nullopt;
  }
  RampSnapshot result;
  result.shader = *shader;
  result.minimum = renderer->classificationMin();
  result.maximum = renderer->classificationMax();
  if (!std::isfinite(result.minimum) || !std::isfinite(result.maximum) || result.minimum >= result.maximum) {
    result.minimum = items.first().value;
    result.maximum = items.last().value;
  }
  // Do not invent colors beyond the renderer's actual classification stops.
  result.minimum = std::max(result.minimum, items.first().value);
  result.maximum = std::min(result.maximum, items.last().value);
  if (!(result.minimum < result.maximum)) return std::nullopt;
  result.ticks.append(result.minimum);
  for (double value : {0., 100., 200., 500., 1000., 1500., 2000.})
    if (value > result.minimum && value < result.maximum) result.ticks.append(value);
  result.ticks.append(result.maximum);
  if (result.ticks.size() < 5) {
    result.ticks.clear();
    for (int i = 0; i < 5; ++i)
      result.ticks.append(std::lerp(result.minimum, result.maximum, i / 4.));
  } else if (result.ticks.size() > 7) {
    const auto all = result.ticks;
    result.ticks.clear();
    for (int i = 0; i < 7; ++i)
      result.ticks.append(all.at(qRound(i * (all.size() - 1.) / 6.)));
  }
  return result;
}

// This adapter keeps the inherited node's ramp API faithful to the legend
// axis. No live layer or renderer pointer escapes into a legend node.
class ShaderLegendRamp final : public QgsColorRamp {
public:
  explicit ShaderLegendRamp(const RampSnapshot& snapshot) : m_snapshot(snapshot) {}
  int count() const override { return static_cast<int>(m_snapshot.ticks.size()); }
  double value(int index) const override { return double(index) / (count() - 1); }
  QColor color(double value) const override { return m_snapshot.color(m_inverted ? 1. - value : value); }
  QString type() const override { return QStringLiteral("ka_dem_shader_legend"); }
  void invert() override { m_inverted = !m_inverted; }
  QgsColorRamp* clone() const override { return new ShaderLegendRamp(*this); }
  QVariantMap properties() const override { return {}; }
private:
  RampSnapshot m_snapshot;
  bool m_inverted = false;
};

// Both delivery paths paint in the same logical units. Layout export keeps
// text as painter text instead of baking it into a low-resolution preview.
void paintLegend(QPainter& painter, const RampSnapshot& ramp) {
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  QFont font = QApplication::font();
  font.setPixelSize(11);
  font.setBold(true);
  painter.setFont(font);
  painter.setPen(QColor(31, 35, 40));
  painter.drawText(QRectF(4., 0., kWidth - 8., 18.), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("표고 (m)"));
  font.setBold(false);
  painter.setFont(font);
  // Sample complete device rows, including both end colors. Subpixel strips
  // overwrite each other at screen resolution and used to hide the maximum
  // color underneath the border. Only the color bar is rasterized; text below
  // remains vector text in layout/PDF output.
  const QTransform transform = painter.deviceTransform();
  const double pixelsPerUnit = std::hypot(transform.m21(), transform.m22());
  const int rows = std::clamp(qRound(kRampHeight * pixelsPerUnit), 2, 4096);
  QImage strip(1, rows, QImage::Format_ARGB32_Premultiplied);
  for (int i = 0; i < rows; ++i)
    strip.setPixelColor(0, i, ramp.color(1. - double(i) / (rows - 1)));
  // Preserve the exact color at each labeled elevation when its position
  // falls between two device rows (e.g. fractional Windows display scaling).
  for (qsizetype i = 0; i < ramp.ticks.size(); ++i) {
    const double position = double(i) / (ramp.ticks.size() - 1);
    strip.setPixelColor(0, qRound((1. - position) * (rows - 1)), ramp.color(position));
  }
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter.drawImage(QRectF(kRampLeft, kRampTop, kRampWidth, kRampHeight), strip);
  painter.setPen(QPen(QColor(90, 95, 100), 0.6));
  painter.drawRect(QRectF(kRampLeft - 1., kRampTop - 1., kRampWidth + 2., kRampHeight + 2.));
  painter.setRenderHint(QPainter::Antialiasing, true);
  const QFontMetricsF metrics(font);
  for (qsizetype i = 0; i < ramp.ticks.size(); ++i) {
    const double y = kRampTop + (kRampHeight - 1.) * (1. - double(i) / (ramp.ticks.size() - 1)) + 0.5;
    painter.drawLine(QPointF(kRampLeft + kRampWidth + 1., y), QPointF(28., y));
    painter.setPen(QColor(31, 35, 40));
    const QString label = tickText(ramp.ticks.at(i));
    const QString fitted = metrics.horizontalAdvance(label) > kWidth - 34.
        ? QString::number(ramp.ticks.at(i), 'g', 5) : label;
    painter.drawText(QRectF(32., y - 9., kWidth - 34., 18.), Qt::AlignLeft | Qt::AlignVCenter, fitted);
  }
  font.setPixelSize(9);
  painter.setFont(font);
  painter.setPen(QColor(85, 90, 95));
  painter.drawText(QRectF(4., 141., kWidth - 8., 14.), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("비선형 눈금"));
  painter.restore();
}

class DemLegendNode final : public QgsColorRampLegendNode {
public:
  DemLegendNode(QgsLayerTreeLayer* layer, const RampSnapshot& snapshot)
      : QgsColorRampLegendNode(layer, new ShaderLegendRamp(snapshot), QgsColorRampLegendNodeSettings(),
                               snapshot.minimum, snapshot.maximum, nullptr, QStringLiteral("ka_dem_elevation")),
        m_snapshot(snapshot) {}

  QVariant data(int role) const override {
    if (role == Qt::DecorationRole) {
      const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.;
      if (m_preview.isNull() || !qFuzzyCompare(m_preview.devicePixelRatio(), dpr)) {
        m_preview = QPixmap(qRound(kWidth * dpr), qRound(kHeight * dpr));
        m_preview.setDevicePixelRatio(dpr);
        m_preview.fill(Qt::transparent);
        QPainter painter(&m_preview);
        paintLegend(painter, m_snapshot);
      }
      return m_preview;
    }
    if (role == Qt::SizeHintRole) return QSize(qRound(kWidth), qRound(kHeight));
    if (role == Qt::ToolTipRole || role == Qt::AccessibleTextRole) {
      QStringList ticks;
      for (double value : m_snapshot.ticks) ticks.append(tickText(value));
      return QStringLiteral("표고 (m) · 비선형 눈금\n%1").arg(ticks.join(QStringLiteral(", ")));
    }
    return QgsColorRampLegendNode::data(role);
  }
  void invalidateDisplayData() override { m_preview = QPixmap(); }
  QSizeF drawSymbol(const QgsLegendSettings& settings, ItemContext* context, double) const override {
    if (context && context->painter) {
      QPainter& painter = *context->painter;
      painter.save();
      const double left = settings.symbolAlignment() == Qt::AlignRight
          ? context->columnRight - kLayoutSize.width() : context->columnLeft;
      painter.translate(left, context->top);
      painter.scale(kLayoutSize.width() / kWidth, kLayoutSize.height() / kHeight);
      paintLegend(painter, m_snapshot);
      painter.restore();
    }
    return kLayoutSize;
  }
  QSizeF drawSymbolText(const QgsLegendSettings&, ItemContext*, QSizeF) const override { return {}; }
private:
  RampSnapshot m_snapshot;
  mutable QPixmap m_preview;
};
} // namespace

DemColorRampLegend::DemColorRampLegend(QgsRasterLayer* layer) : m_layer(layer) {
  connect(layer, &QgsMapLayer::styleChanged, this, &QgsMapLayerLegend::itemsChanged);
}

bool DemColorRampLegend::install(QgsRasterLayer* layer) {
  if (!snapshot(layer)) return false;
  if (dynamic_cast<DemColorRampLegend*>(layer->legend())) return true;
  layer->setLegend(new DemColorRampLegend(layer));
  return true;
}

QList<QgsLayerTreeModelLegendNode*> DemColorRampLegend::createLayerTreeModelLegendNodes(QgsLayerTreeLayer* node) {
  if (!node || !m_layer) return {};
  if (const auto ramp = snapshot(m_layer)) return {new DemLegendNode(node, *ramp)};
  std::unique_ptr<QgsMapLayerLegend> fallback(QgsMapLayerLegend::defaultRasterLegend(m_layer));
  return fallback->createLayerTreeModelLegendNodes(node);
}
