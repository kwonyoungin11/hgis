#include "KaCrashGuard.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace {

// 크래시 핸들러 안에서는 Qt 할당을 피해야 해서 경로를 미리 넓은 문자열로 잡아 둔다.
#ifdef Q_OS_WIN
wchar_t g_logDirW[MAX_PATH] = L"";
#endif
std::atomic<bool> g_inCrash{false};
QtMessageHandler g_prevQtHandler = nullptr;

QString logDirQ() {
  const QByteArray localAppData = qgetenv("LOCALAPPDATA");
  QString base = localAppData.isEmpty() ? QDir::tempPath()
                                        : QString::fromLocal8Bit(localAppData);
  return base + QStringLiteral("/ka-hgis/logs");
}

#ifdef Q_OS_WIN

void appendUtf8(FILE* f, const char* s) { fputs(s, f); }

void writeTimestamp(FILE* f) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  char buf[64];
  _snprintf_s(buf, _TRUNCATE, "%04u-%02u-%02u %02u:%02u:%02u.%03u", st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
  appendUtf8(f, buf);
}

// 주소 하나를 "모듈!심볼+오프셋 (파일:줄)"로 심볼화해 기록. PDB가 없으면 모듈+RVA만.
void writeFrame(FILE* f, int idx, DWORD64 addr) {
  char line[1024];
  char modName[MAX_PATH] = "?";
  HMODULE mod = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(addr), &mod) &&
      mod) {
    wchar_t modPath[MAX_PATH];
    if (GetModuleFileNameW(mod, modPath, MAX_PATH)) {
      const wchar_t* baseName = wcsrchr(modPath, L'\\');
      baseName = baseName ? baseName + 1 : modPath;
      WideCharToMultiByte(CP_UTF8, 0, baseName, -1, modName, sizeof(modName), nullptr, nullptr);
    }
  }
  const DWORD64 rva = mod ? addr - reinterpret_cast<DWORD64>(mod) : addr;

  alignas(SYMBOL_INFO) char symBuf[sizeof(SYMBOL_INFO) + 256] = {};
  auto* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
  sym->SizeOfStruct = sizeof(SYMBOL_INFO);
  sym->MaxNameLen = 255;
  DWORD64 disp = 0;
  const bool haveSym = SymFromAddr(GetCurrentProcess(), addr, &disp, sym);

  IMAGEHLP_LINE64 li = {};
  li.SizeOfStruct = sizeof(li);
  DWORD lineDisp = 0;
  const bool haveLine = SymGetLineFromAddr64(GetCurrentProcess(), addr, &lineDisp, &li);

  if (haveSym && haveLine)
    _snprintf_s(line, _TRUNCATE, "  #%02d %s!%s+0x%llx (%s:%lu)\n", idx, modName, sym->Name,
                static_cast<unsigned long long>(disp), li.FileName, li.LineNumber);
  else if (haveSym)
    _snprintf_s(line, _TRUNCATE, "  #%02d %s!%s+0x%llx\n", idx, modName, sym->Name,
                static_cast<unsigned long long>(disp));
  else
    _snprintf_s(line, _TRUNCATE, "  #%02d %s+0x%llx\n", idx, modName,
                static_cast<unsigned long long>(rva));
  appendUtf8(f, line);
}

// 예외 CONTEXT 기준 스택워크(크래시 지점). ctx가 없으면 현재 스레드 스택.
void writeStack(FILE* f, CONTEXT* ctx) {
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
  SymInitialize(GetCurrentProcess(), nullptr, TRUE);

  if (!ctx) {
    void* frames[62] = {};
    const USHORT n = RtlCaptureStackBackTrace(1, 62, frames, nullptr);
    for (USHORT i = 0; i < n; ++i)
      writeFrame(f, i, reinterpret_cast<DWORD64>(frames[i]));
    return;
  }

  CONTEXT c = *ctx;
  STACKFRAME64 sf = {};
  sf.AddrPC.Offset = c.Rip;
  sf.AddrPC.Mode = AddrModeFlat;
  sf.AddrFrame.Offset = c.Rbp;
  sf.AddrFrame.Mode = AddrModeFlat;
  sf.AddrStack.Offset = c.Rsp;
  sf.AddrStack.Mode = AddrModeFlat;
  for (int i = 0; i < 62; ++i) {
    if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(), &sf, &c,
                     nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
      break;
    if (!sf.AddrPC.Offset) break;
    writeFrame(f, i, sf.AddrPC.Offset);
  }
}

FILE* openCrashLog(wchar_t* outStem, size_t stemLen) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t path[MAX_PATH];
  _snwprintf_s(outStem, stemLen, _TRUNCATE, L"%s\\crash-%04u%02u%02u-%02u%02u%02u", g_logDirW,
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  _snwprintf_s(path, _TRUNCATE, L"%s.log", outStem);
  FILE* f = nullptr;
  _wfopen_s(&f, path, L"ab");
  return f;
}

void writeMiniDump(const wchar_t* stem, EXCEPTION_POINTERS* ep) {
  wchar_t path[MAX_PATH];
  _snwprintf_s(path, _TRUNCATE, L"%s.dmp", stem);
  HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  MINIDUMP_EXCEPTION_INFORMATION mei = {};
  mei.ThreadId = GetCurrentThreadId();
  mei.ExceptionPointers = ep;
  mei.ClientPointers = FALSE;
  const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo |
                                               MiniDumpWithIndirectlyReferencedMemory);
  MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), h, type, ep ? &mei : nullptr,
                    nullptr, nullptr);
  CloseHandle(h);
}

// 처리되지 않은 SEH 예외(접근위반 등) — 덤프 + 심볼 스택.
LONG WINAPI kaUnhandledFilter(EXCEPTION_POINTERS* ep) {
  if (g_inCrash.exchange(true)) return EXCEPTION_CONTINUE_SEARCH;
  wchar_t stem[MAX_PATH];
  if (FILE* f = openCrashLog(stem, MAX_PATH)) {
    writeTimestamp(f);
    char head[256];
    _snprintf_s(head, _TRUNCATE, " UNHANDLED EXCEPTION code=0x%08lx addr=%p\n",
                ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0,
                ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr);
    appendUtf8(f, head);
    writeStack(f, ep ? ep->ContextRecord : nullptr);
    fclose(f);
  }
  writeMiniDump(stem, ep);
  return EXCEPTION_CONTINUE_SEARCH;  // WER(이벤트 로그)도 그대로 남긴다.
}

// C++ 예외가 어떤 catch에도 안 잡혔을 때 — 예외 타입·메시지를 남긴다.
void kaTerminateHandler() {
  if (!g_inCrash.exchange(true)) {
    wchar_t stem[MAX_PATH];
    if (FILE* f = openCrashLog(stem, MAX_PATH)) {
      writeTimestamp(f);
      appendUtf8(f, " std::terminate — 처리되지 않은 C++ 예외\n");
      if (std::exception_ptr cur = std::current_exception()) {
        try {
          std::rethrow_exception(cur);
        } catch (const std::exception& e) {
          char buf[512];
          _snprintf_s(buf, _TRUNCATE, "  exception: %s\n  what(): %s\n", typeid(e).name(),
                      e.what());
          appendUtf8(f, buf);
        } catch (...) {
          appendUtf8(f, "  exception: (std::exception 아님)\n");
        }
      } else {
        appendUtf8(f, "  (활성 예외 없음 — noexcept 위반 또는 직접 호출)\n");
      }
      appendUtf8(f, "  stack (terminate 시점):\n");
      writeStack(f, nullptr);
      fclose(f);
    }
    writeMiniDump(stem, nullptr);
  }
  std::abort();
}

// CRT 잘못된 파라미터(널 포인터 문자열 등) — 릴리스에선 인자 정보가 없다.
void kaInvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int,
                               uintptr_t) {
  if (!g_inCrash.exchange(true)) {
    wchar_t stem[MAX_PATH];
    if (FILE* f = openCrashLog(stem, MAX_PATH)) {
      writeTimestamp(f);
      appendUtf8(f, " CRT invalid parameter\n");
      writeStack(f, nullptr);
      fclose(f);
    }
    writeMiniDump(stem, nullptr);
  }
  std::abort();
}

void kaPurecallHandler() {
  if (!g_inCrash.exchange(true)) {
    wchar_t stem[MAX_PATH];
    if (FILE* f = openCrashLog(stem, MAX_PATH)) {
      writeTimestamp(f);
      appendUtf8(f, " pure virtual call\n");
      writeStack(f, nullptr);
      fclose(f);
    }
    writeMiniDump(stem, nullptr);
  }
  std::abort();
}

#endif  // Q_OS_WIN

// qWarning/qCritical을 세션 로그에도 남긴다(기존 콘솔 출력은 유지).
void kaQtMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
  if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
    const char* lv = type == QtWarningMsg ? "warn" : (type == QtCriticalMsg ? "crit" : "fatal");
    KaCrashGuard::logLine(QStringLiteral("[qt/%1] %2").arg(QLatin1String(lv), msg));
  }
  if (g_prevQtHandler) g_prevQtHandler(type, ctx, msg);
}

}  // namespace

QString KaCrashGuard::logDir() { return logDirQ(); }

void KaCrashGuard::logLine(const QString& line) {
  static QMutex mtx;
  static QString lastLine;
  static int repeat = 0;
  QMutexLocker lock(&mtx);

  QFile f(logDirQ() + QStringLiteral("/session.log"));
  // 4MB 넘으면 한 세대 물려 회전 — 경고 폭주가 디스크를 채우지 않게.
  if (f.size() > 4 * 1024 * 1024) {
    const QString oldPath = logDirQ() + QStringLiteral("/session.old.log");
    QFile::remove(oldPath);
    QFile::rename(f.fileName(), oldPath);
  }
  if (!f.open(QIODevice::Append | QIODevice::Text)) return;
  QTextStream ts(&f);

  // 같은 줄이 연달아 오면(타일 서버 오류 폭주 등) 접어서 기록한다.
  if (line == lastLine) {
    ++repeat;
    if (repeat % 50 == 0)
      ts << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
         << QStringLiteral(" (직전 메시지 %1회 반복)\n").arg(repeat);
    return;
  }
  if (repeat > 0) {
    ts << QStringLiteral("  (직전 메시지 총 %1회 반복)\n").arg(repeat + 1);
    repeat = 0;
  }
  lastLine = line;
  ts << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")) << ' '
     << line << '\n';
}

void KaCrashGuard::install() {
  QDir().mkpath(logDirQ());
#ifdef Q_OS_WIN
  const QString dirNative = QDir::toNativeSeparators(logDirQ());
  wcsncpy_s(g_logDirW, dirNative.toStdWString().c_str(), _TRUNCATE);
  SetUnhandledExceptionFilter(kaUnhandledFilter);
  std::set_terminate(kaTerminateHandler);
  _set_invalid_parameter_handler(kaInvalidParameterHandler);
  _set_purecall_handler(kaPurecallHandler);
#endif
  g_prevQtHandler = qInstallMessageHandler(kaQtMessageHandler);
  logLine(QStringLiteral("[boot] 진단 장치 시작 — 크래시 로그 폴더: %1").arg(logDirQ()));
}
