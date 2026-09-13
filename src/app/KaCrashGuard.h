#pragma once

#include <QString>

// 충돌 진단 장치. 처리되지 않은 예외·치명적 CRT 오류가 나면
// %LOCALAPPDATA%\ka-hgis\logs 에 (1) 사람이 읽을 수 있는 심볼 스택 로그와
// (2) 미니덤프(.dmp)를 남긴다. 경고 이상 Qt/QGIS 메시지도 세션 로그로 남겨
// "어디서 에러가 났는지"를 현장에서 재현 없이 추적할 수 있게 한다.
class KaCrashGuard {
public:
  // main() 최상단에서 한 번 호출. (QApplication 생성 전이어도 안전)
  static void install();

  // 세션 로그 한 줄 기록(스레드 안전, 연속 중복은 횟수로 접어서 기록).
  static void logLine(const QString& line);

  // 로그 폴더 경로(%LOCALAPPDATA%\ka-hgis\logs).
  static QString logDir();
};
