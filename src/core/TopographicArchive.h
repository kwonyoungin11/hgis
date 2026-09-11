#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <functional>

namespace TopographicArchive {
struct Limits {
  quint64 maxTotalBytes = 4ULL * 1024 * 1024 * 1024;
  quint64 maxFileBytes = 1024ULL * 1024 * 1024;
  int maxEntries = 10000;
  int maxArchiveDepth = 4;
};
struct Result {
  QString directory;
  QStringList files;
  QString error;
  bool canceled = false;
  bool reused = false;
};

// Thread-safe value API. Never changes source files. A ZIP is published only
// after every entry has been copied and verified. No partial directory is returned.
// Non-archive vector sources are returned read-only in their existing directory.
Result prepare(const QString& source, const QString& libraryRoot,
               const Limits& limits = {}, const std::function<bool()>& isCanceled = {});
}
