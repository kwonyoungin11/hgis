#pragma once
#include <QWidget>
#include <QPointer>
#include <QJsonObject>
#include <QMap>
#include <QQueue>
#include <QStringList>
#include <qgsgeometry.h>
#include <memory>
#include <vector>

class QgsMapCanvas;
class QgsRubberBand;
class QgsTask;
class KaTopographicBrowser;
class KaTopographicImportDialog;
class QCheckBox;
class QLabel;
class QTimer;

// Created only by the explicit numerical-topography command. No startup restore.
class KaTopographicScopePanel final : public QWidget {
  Q_OBJECT
public:
  KaTopographicScopePanel(QgsMapCanvas* canvas, KaTopographicBrowser* browser,
                         KaTopographicImportDialog* importer, QWidget* parent = nullptr,
                         const QString& libraryDirectory = {});
  ~KaTopographicScopePanel() override;
  void refreshScope();
  void activate();
  void stop();
  bool isActive() const;
  void setLibraryDirectory(const QString& directory);
  void acceptDownload(const QString& path, const QJsonObject& officialRecord);
signals:
  void mapRequested();
  void importFinished(bool success);
  void statusChanged(const QString& message);
  void processingChanged(bool processing);
  void attentionRequired(const QString& message);
  void preparationProgressChanged(const QString& phase, const QString& detail);
private:
  void setStatus(const QString& message);
  void setAttention(const QString& message);
  void updateProcessing();
  void updatePreparationProgress();
  void updateCount();
  void clearBoundaries();
  void startNext();
  void refreshBrowser();
  QPointer<QgsMapCanvas> m_canvas;
  QPointer<KaTopographicBrowser> m_browser;
  QPointer<KaTopographicImportDialog> m_importer;
  QPointer<QgsTask> m_task;
  QCheckBox* m_enabled = nullptr;
  QLabel* m_count = nullptr;
  QLabel* m_status = nullptr;
  QTimer* m_scopeTimer = nullptr;
  std::vector<std::unique_ptr<QgsRubberBand>> m_boundaries;
  QString m_libraryDirectory;
  QStringList m_wanted;
  QStringList m_available;
  QMap<QString,QString> m_reviewMessages;
  QString m_scopeKey;
  QQueue<QPair<QString, QJsonObject>> m_downloads;
  double m_easting5179 = 0;
  double m_northing5179 = 0;
  quint64 m_generation = 0;
  quint64 m_taskSerial = 0;
  QString m_taskPhase;
  QString m_taskDetail;
  QString m_importDetail;
  QString m_reportedPhase;
  QString m_reportedDetail;
  bool m_restorePending = false;
  bool m_processing = false;
  bool m_importLoading = false;
  QgsGeometry m_searchArea;
};
