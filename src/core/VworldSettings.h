#pragma once
#include <QString>

class VworldSettings {
public:
  static QString loadApiKey();
  static void saveApiKey(const QString& key);
};
