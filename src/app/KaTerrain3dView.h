#pragma once

#include <QImage>
#include <QWidget>

#include "core/Terrain3dService.h"

class KaTerrain3dView : public QWidget {
  Q_OBJECT
public:
  explicit KaTerrain3dView(QWidget* parent = nullptr);

  void setMesh(const Terrain3dService::Mesh& mesh, const QImage& texture);
  void setTexture(const QImage& texture);
  float yawDeg() const { return m_yaw; }
  float pitchDeg() const { return m_pitch; }
  float distance() const { return m_dist; }
  void setDistance(float distance);
  QImage renderView(int w, int h) const;
  bool hasMesh() const { return !m_mesh.positions.isEmpty(); }

signals:
  void viewChanged();

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  Terrain3dService::Mesh m_mesh;
  QImage m_texture;
  float m_yaw = 35.0f;
  float m_pitch = 42.0f;
  float m_dist = 180.0f;
  QPoint m_last;
  bool m_dragging = false;
};
