// Loads libpolyir the way another program (ninac, an FFI) would: dlopen the dylib and look the functions up by name.
#include "polyir/Std.hpp"
#include <dlfcn.h>

#include <cstdint>
#include <iostream>
#include <string>

using namespace polyir;

namespace {

  template <typename Function>
  Function symbol(void* library, const char* name) {
    return reinterpret_cast<Function>(dlsym(library, name));
  }

  using IntCall = int64_t (*)(int64_t, int32_t);
  using BeginCall = int64_t (*)(int64_t, const char*, int64_t);
  using NewCall = int64_t (*)();
  using Call3 = int64_t (*)(int64_t, int64_t, int64_t);
  using CompileCall = int32_t (*)(int64_t);
  using Call2 = int64_t (*)(int64_t, int64_t);

  // The entry points this test calls, looked up by name.
  struct Api {
    void* library;
    NewCall contextNew = symbol<NewCall>(library, "polyir_context_new");
    IntCall typeInt = symbol<IntCall>(library, "polyir_type_int");
    BeginCall functionBegin = symbol<BeginCall>(library, "polyir_function_begin");
    Call2 functionBody = symbol<Call2>(library, "polyir_function_body");
    Call3 constInt = symbol<Call3>(library, "polyir_const_int");
    Call3 ret = symbol<Call3>(library, "polyir_ret");
    CompileCall compile = symbol<CompileCall>(library, "polyir_compile");
    const char* (*output)(int64_t) = symbol<const char* (*)(int64_t)>(library, "polyir_output");
    const char* (*version)() = symbol<const char* (*)()>(library, "polyir_version");
  };

  bool complete(const Api& api);

  bool compilesAProgram(const Api& api);

  bool complete(const Api& api) {
    return api.contextNew && api.typeInt && api.functionBegin && api.functionBody && api.constInt && api.ret && api.compile && api.output && api.version;
  }

  bool compilesAProgram(const Api& api) {
    const int64_t context = api.contextNew();
    const int64_t i64 = api.typeInt(context, 64);
    const int64_t main = api.functionBegin(context, "main", i64);
    api.ret(context, api.functionBody(context, main), api.constInt(context, i64, 42));

    const bool worked = api.compile(context) == 1 && String(api.output(context)).find("ret i64") != String::npos;
    cout << (worked ? "ok   " : "FAIL ") << "dylib " << api.version() << " compiled a program from dlopen\n";

    return worked;
  }

  const char* libraryPath() {
#ifdef __APPLE__
    return "./libpolyir.dylib";
#else
    return "./libpolyir.so";
#endif
  }

  int notLoaded(const char* path) {
    cout << "FAIL could not load " << path << ": " << dlerror() << "\n";

    return 1;
  }

  int runAll(const Api& api) {
    if (!complete(api)) {
      cout << "FAIL a function is missing from the dylib\n";
      return 1;
    }

    return compilesAProgram(api) ? 0 : 1;
  }

}  // namespace

int main() {
  const char* path = libraryPath();
  void* library = dlopen(path, RTLD_NOW);

  if (library == nullptr) {
    return notLoaded(path);
  }

  Api api{library};

  return runAll(api);
}
