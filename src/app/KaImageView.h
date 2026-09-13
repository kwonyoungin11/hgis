#pragma once

#include <QGraphicsView>
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QLine>

class QGraphicsPixmapItem;
class QGraphicsItem;

class KaAlignLinkOverlay : public QWidget {
  Q_OBJECT
public:
  explicit KaAlignLinkOverlay(QWidget* parent = nullptr);
  void setLinks(const QVector<QLine>& done, const QLine& live, bool hasLive);

protected:
  void paintEvent(QPaintEvent* e) override;

private:
  void drawArrow(QPainter& p, const QLine& ln);
  void drawNumber(QPainter& p, const QPoint& at, int n, const QColor& ring);
  QVector<QLine> m_done;
  QLine m_live;
  bool m_hasLive = false;
};

class KaImageView : public QGraphicsView {
  Q_OBJECT
public:
  explicit KaImageView(QWidget* parent = nullptr);

  bool loadPath(const QString& path);
  // 원본이 Qt 로 열기엔 너무 큰 그림용. 화면에는 축소본을 보여 주되,
  // 클릭 좌표는 언제나 원본 픽셀로 돌려준다(정합 계산이 원본 기준이라서).
  bool setPreview(const QPixmap& preview, int sourceWidth, int sourceHeight);
  QString lastError() const { return m_lastError; }
  void clearMarks();
  void setMarks(const QVector<QPointF>& pts, const QPointF* pending = nullptr);
  void fitImage();
  bool hasImage() const { return m_pix != nullptr; }
  // 화면 1픽셀이 원본 몇 픽셀인지. 원본 그대로면 1.
  double sourceScale() const { return m_srcScale; }
  QPoint viewPosForPixel(double pixelX, double pixelY) const;

signals:
  void pixelClicked(double x, double y);
  void viewChanged();

protected:
  void wheelEvent(QWheelEvent* e) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;

private:
  void addMarkItem(double pixelX, double pixelY, int number, const QColor& ring);

  bool applyPixmap(const QPixmap& pm);
  double m_srcScale = 1.0;
  QString m_lastError;
  QGraphicsPixmapItem* m_pix = nullptr;
  QVector<QGraphicsItem*> m_marks;
  QPoint m_lastPan;
  bool m_panning = false;
  bool m_fitted = false;
};
