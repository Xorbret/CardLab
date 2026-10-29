// A restricted Lua state for running learner code and lesson content.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include "hal.h"

namespace cardlab {

// Hardware modules a sandbox may expose. Lessons ask for these by name.
enum Permission : uint32_t {
  kPermNone = 0,
  kPermDisplay = 1u << 0,
  kPermGpio = 1u << 1,
  kPermImu = 1u << 2,
  kPermI2c = 1u << 3,
  kPermWifiScan = 1u << 4,
  kPermFiles = 1u << 5,
  kPermSys = 1u << 6,
  kPermAll = 0xFFFFFFFFu,
};

// Read-only modules that lesson checks may use to compare against the
// learner's output. Nothing here changes hardware state.
constexpr uint32_t kPermReadOnly = kPermImu | kPermI2c | kPermSys;

// Returns kPermNone and sets ok=false for an unknown name.
uint32_t permissionFromName(const std::string& name, bool& ok);

struct RunResult {
  bool ok = false;
  std::string output;  // everything passed to print()
  std::string error;   // Lua error message when ok is false
};

struct SandboxLimits {
  size_t memoryBytes = 48 * 1024;
  uint32_t instructionBudget = 2000000;
};

class Sandbox {
 public:
  Sandbox(Hal* hal, uint32_t permissions, SandboxLimits limits = {});
  ~Sandbox();
  Sandbox(const Sandbox&) = delete;
  Sandbox& operator=(const Sandbox&) = delete;

  // Compiles and runs text source (bytecode is refused).
  RunResult run(const std::string& code, const char* chunkName = "=learner");

  // Console mode: an expression such as "1+1" prints its value; anything
  // else runs as a statement.
  RunResult eval(const std::string& line);

  lua_State* state() { return L_; }
  Hal* hal() { return hal_; }
  uint32_t permissions() const { return permissions_; }

  // Used while calling back into Lua from C++ (e.g. a lesson's check()).
  void beginCall();
  RunResult endCall(int status);

 private:
  static void* alloc(void* ud, void* ptr, size_t osize, size_t nsize);
  static void countHook(lua_State* L, lua_Debug* ar);
  static int luaPrint(lua_State* L);
  void openSafeLibs();
  void registerModules();

  lua_State* L_ = nullptr;
  Hal* hal_;
  uint32_t permissions_;
  SandboxLimits limits_;
  size_t used_ = 0;
  uint32_t executed_ = 0;
  std::string output_;
};

// Fetches the Sandbox that owns a lua_State (stored in its extra space).
Sandbox* sandboxOf(lua_State* L);

}  // namespace cardlab
