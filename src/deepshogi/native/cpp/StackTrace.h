#pragma once

#include <cstdio>
#include <cstdlib>

#if defined(__GNUC__)
#include <cxxabi.h>    // __cxa_demangle
#include <dlfcn.h>     // dladdr
#include <execinfo.h>  // backtrace, backtrace_symbols

void printStackTrace(FILE* fp = stderr) {
  // スタックフレームを最大 128 個まで取得
  const int MAX_FRAMES = 128;
  void* callstack[MAX_FRAMES];
  int frames = ::backtrace(callstack, MAX_FRAMES);

  // 取得したスタックフレームを文字列情報に変換
  char** symbols = ::backtrace_symbols(callstack, frames);
  if (!symbols) {
    std::fprintf(fp, "Error: backtrace_symbols() returned nullptr.\n");
    return;
  }

  for (int i = 0; i < frames; i++) {
    Dl_info info;
    if (dladdr(callstack[i], &info) && info.dli_sname) {
      // C++シンボルのデマングル
      int status = 0;
      char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);

      // シンボルがデマングルできた場合はデマングル後の名前を利用
      const char* symbolName = (status == 0 && demangled) ? demangled : info.dli_sname;

      // アドレスとシンボル名、ベースアドレスとの差を表示
      std::fprintf(
          fp,
          "%2d: %p  %s + %zd\n",
          i,
          callstack[i],
          symbolName,
          (char*)callstack[i] - (char*)info.dli_saddr);

      std::free(demangled);
    } else {
      // dladdr で情報が取れなかった場合は backtrace_symbols の結果をそのまま表示
      std::fprintf(fp, "%2d: %p  %s\n", i, callstack[i], symbols[i]);
    }
  }

  std::free(symbols);
}
#else
void printStackTrace(FILE* fp = stderr) {
  std::fprintf(fp, "Stack trace is not supported on this platform.\n");
}
#endif
