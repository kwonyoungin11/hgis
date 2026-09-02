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
  void clearMarks();
  void setMarks(const QVector<QPointF>& pts, const QPointF* pending = nullptr);
  void fitImage();
  bool hasImage() const { return m_pix != nullptr; }
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

  QGraphicsPixmapItem* m_pix = nullptr;
  QVector<QGraphicsItem*> m_marks;
  QPoint m_lastPan;
  bool m_panning = false;
  bool m_fitted = false;
};
