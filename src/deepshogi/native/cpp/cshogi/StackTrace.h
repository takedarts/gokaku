#pragma once

#include <cstdio>
#include <cstdlib>

#if defined(__GNUC__)
#include <cxxabi.h>    // __cxa_demangle
#include <dlfcn.h>     // dladdr
#include <execinfo.h>  // backtrace, backtrace_symbols

void printStackTrace(FILE* fp = stderr) {
  // Get up to 128 stack frames
  const int MAX_FRAMES = 128;
  void* callstack[MAX_FRAMES];
  int frames = ::backtrace(callstack, MAX_FRAMES);

  // Convert obtained stack frames to string information
  char** symbols = ::backtrace_symbols(callstack, frames);
  if (!symbols) {
    std::fprintf(fp, "Error: backtrace_symbols() returned nullptr.\n");
    return;
  }

  for (int i = 0; i < frames; i++) {
    Dl_info info;
    if (dladdr(callstack[i], &info) && info.dli_sname) {
      // Demangle C++ symbols
      int status = 0;
      char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);

      // If the symbol was successfully demangled, use the demangled name
      const char* symbolName = (status == 0 && demangled) ? demangled : info.dli_sname;

      // Display address, symbol name, and offset from base address
      std::fprintf(
          fp,
          "%2d: %p  %s + %zd\n",
          i,
          callstack[i],
          symbolName,
          (char*)callstack[i] - (char*)info.dli_saddr);

      std::free(demangled);
    } else {
      // If dladdr could not retrieve information, display the result from backtrace_symbols as is
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
