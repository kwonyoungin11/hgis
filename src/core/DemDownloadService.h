#pragma once

#include "PreparedReferenceMap.h"

class QgsRectangle;

// Worker-only preparation. The caller adds the returned local raster on the GUI
// thread and retains its independently owned files after successful insertion.
class DemDownloadService {
public:
  static PreparedReferenceMap prepare(
      const QgsRectangle& wgs84Extent, const QString& targetBasePath,
      QgsFeedback* feedback,
      const std::function<bool()>& cancellationRequested = {});
};
