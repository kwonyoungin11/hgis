#pragma once
#include <QString>

class KaApplication {
public:
  static int run(int argc, char** argv);
  static QString resolvePrefixPath();
};
