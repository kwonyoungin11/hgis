#pragma once

#include "core/PreparedReferenceMap.h"
#include <QEventLoopLocker>
#include <qgstaskmanager.h>

// The task manager owns this job until its worker has stopped. Keeping the event
// loop alive also keeps QGIS available when the last window closes during I/O.
class KaReferenceDownloadJob final : public QgsTask {
public:
  using Prepare = std::function<PreparedReferenceMap(QgsFeedback*)>;
  using Complete = std::function<void(const PreparedReferenceMap&)>;
  using CancellablePrepare = std::function<PreparedReferenceMap(QgsFeedback*, const std::function<bool()>&)>;

  KaReferenceDownloadJob(const QString& title, Prepare prepare, Complete complete);
  KaReferenceDownloadJob(const QString& title, CancellablePrepare prepare, Complete complete);

protected:
  bool run() override;
  void finished(bool success) override;

private:
  QEventLoopLocker m_quitLock;
  CancellablePrepare m_prepare;
  Complete m_complete;
  PreparedReferenceMap m_result;
};
