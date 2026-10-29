#include "sandbox.h"

#include <cstdlib>
#include <cstring>

namespace cardlab {

namespace {

constexpr int kHookEvery = 1000;
constexpr size_t kMaxOutput = 4096;

struct PermName {
  const char* name;
  uint32_t bit;
};

const PermName kPermNames[] = {
    {"display", kPermDisplay}, {"gpio", kPermGpio},       {"imu", kPermImu},
    {"i2c", kPermI2c},         {"wifi", kPermWifiScan},   {"files", kPermFiles},
    {"sys", kPermSys},
};

Hal* halOf(lua_State* L) { return sandboxOf(L)->hal(); }

// Rejects absolute paths and parent-directory escapes so learner code
// stays inside its own folder on the SD card.
bool safeRelativePath(const char* p) {
  if (p[0] == '\0' || p[0] == '/' || p[0] == '\\') return false;
  return std::strstr(p, "..") == nullptr;
}

int checkPin(lua_State* L) {
  int pin = static_cast<int>(luaL_checkinteger(L, 1));
  if (!halOf(L)->gpioAllowed(pin)) {
    return luaL_error(L, "pin %d is not on the Grove/EXT header", pin);
  }
  return pin;
}

// display ---------------------------------------------------------------
int displayClear(lua_State* L) {
  halOf(L)->displayClear();
  return 0;
}
int displayPrint(lua_State* L) {
  halOf(L)->displayPrint(luaL_checkstring(L, 1));
  return 0;
}
const luaL_Reg kDisplay[] = {{"clear", displayClear}, {"print", displayPrint}, {nullptr, nullptr}};

// gpio ------------------------------------------------------------------
int gpioMode(lua_State* L) {
  int pin = checkPin(L);
  const char* mode = luaL_checkstring(L, 2);
  bool output = std::strcmp(mode, "out") == 0;
  if (!output && std::strcmp(mode, "in") != 0) return luaL_error(L, "mode must be 'in' or 'out'");
  halOf(L)->gpioMode(pin, output);
  return 0;
}
int gpioWrite(lua_State* L) {
  int pin = checkPin(L);
  // Accept 1/0 as well as true/false; in Lua 0 is truthy, so check numbers first.
  int value = lua_isnumber(L, 2) ? (lua_tointeger(L, 2) != 0) : lua_toboolean(L, 2);
  halOf(L)->gpioWrite(pin, value);
  return 0;
}
int gpioRead(lua_State* L) {
  int pin = checkPin(L);
  lua_pushinteger(L, halOf(L)->gpioRead(pin));
  return 1;
}
const luaL_Reg kGpio[] = {{"mode", gpioMode}, {"write", gpioWrite}, {"read", gpioRead}, {nullptr, nullptr}};

// imu -------------------------------------------------------------------
int imuRead(lua_State* L) {
  ImuSample s;
  if (!halOf(L)->imuRead(s)) return luaL_error(L, "IMU not available");
  lua_pushnumber(L, s.ax);
  lua_pushnumber(L, s.ay);
  lua_pushnumber(L, s.az);
  lua_pushnumber(L, s.gx);
  lua_pushnumber(L, s.gy);
  lua_pushnumber(L, s.gz);
  return 6;
}
const luaL_Reg kImu[] = {{"read", imuRead}, {nullptr, nullptr}};

// i2c -------------------------------------------------------------------
int i2cScan(lua_State* L) {
  std::vector<uint8_t> found = halOf(L)->i2cScan();
  lua_createtable(L, static_cast<int>(found.size()), 0);
  for (size_t i = 0; i < found.size(); ++i) {
    lua_pushinteger(L, found[i]);
    lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
  }
  return 1;
}
const luaL_Reg kI2c[] = {{"scan", i2cScan}, {nullptr, nullptr}};

// wifi (passive scan only) ----------------------------------------------
int wifiScan(lua_State* L) {
  std::vector<WifiNetwork> nets = halOf(L)->wifiScan();
  lua_createtable(L, static_cast<int>(nets.size()), 0);
  for (size_t i = 0; i < nets.size(); ++i) {
    lua_createtable(L, 0, 4);
    lua_pushstring(L, nets[i].ssid.c_str());
    lua_setfield(L, -2, "ssid");
    lua_pushinteger(L, nets[i].rssi);
    lua_setfield(L, -2, "rssi");
    lua_pushstring(L, nets[i].auth.c_str());
    lua_setfield(L, -2, "auth");
    lua_pushinteger(L, nets[i].channel);
    lua_setfield(L, -2, "channel");
    lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
  }
  return 1;
}
const luaL_Reg kWifi[] = {{"scan", wifiScan}, {nullptr, nullptr}};

// files -----------------------------------------------------------------
int filesRead(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  if (!safeRelativePath(path)) return luaL_error(L, "path must stay inside your folder");
  std::string data;
  if (!halOf(L)->fileRead(path, data)) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushlstring(L, data.data(), data.size());
  return 1;
}
int filesWrite(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  size_t len = 0;
  const char* data = luaL_checklstring(L, 2, &len);
  if (!safeRelativePath(path)) return luaL_error(L, "path must stay inside your folder");
  lua_pushboolean(L, halOf(L)->fileWrite(path, std::string(data, len)));
  return 1;
}
const luaL_Reg kFiles[] = {{"read", filesRead}, {"write", filesWrite}, {nullptr, nullptr}};

// sys -------------------------------------------------------------------
int sysMillis(lua_State* L) {
  lua_pushinteger(L, halOf(L)->millis());
  return 1;
}
int sysSleep(lua_State* L) {
  lua_Integer ms = luaL_checkinteger(L, 1);
  if (ms < 0 || ms > 5000) return luaL_error(L, "sleep must be 0..5000 ms");
  halOf(L)->delayMs(static_cast<uint32_t>(ms));
  return 0;
}
const luaL_Reg kSys[] = {{"millis", sysMillis}, {"sleep", sysSleep}, {nullptr, nullptr}};

}  // namespace

uint32_t permissionFromName(const std::string& name, bool& ok) {
  for (const PermName& p : kPermNames) {
    if (name == p.name) {
      ok = true;
      return p.bit;
    }
  }
  ok = false;
  return kPermNone;
}

Sandbox* sandboxOf(lua_State* L) { return *static_cast<Sandbox**>(lua_getextraspace(L)); }

Sandbox::Sandbox(Hal* hal, uint32_t permissions, SandboxLimits limits)
    : hal_(hal), permissions_(permissions), limits_(limits) {
  L_ = lua_newstate(&Sandbox::alloc, this);
  if (L_ == nullptr) return;
  *static_cast<Sandbox**>(lua_getextraspace(L_)) = this;
  openSafeLibs();
  registerModules();
  lua_sethook(L_, &Sandbox::countHook, LUA_MASKCOUNT, kHookEvery);
}

Sandbox::~Sandbox() {
  if (L_ != nullptr) lua_close(L_);
}

void* Sandbox::alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  Sandbox* self = static_cast<Sandbox*>(ud);
  // When ptr is null, osize carries a type tag rather than a size.
  size_t old = ptr != nullptr ? osize : 0;
  if (nsize == 0) {
    self->used_ -= old;
    std::free(ptr);
    return nullptr;
  }
  if (nsize > old && self->used_ - old + nsize > self->limits_.memoryBytes) return nullptr;
  void* p = std::realloc(ptr, nsize);
  if (p != nullptr) self->used_ = self->used_ - old + nsize;
  return p;
}

void Sandbox::countHook(lua_State* L, lua_Debug*) {
  Sandbox* self = sandboxOf(L);
  self->executed_ += kHookEvery;
  if (self->executed_ > self->limits_.instructionBudget) {
    luaL_error(L, "time limit reached (infinite loop?)");
  }
}

int Sandbox::luaPrint(lua_State* L) {
  Sandbox* self = sandboxOf(L);
  int n = lua_gettop(L);
  for (int i = 1; i <= n; ++i) {
    size_t len = 0;
    const char* s = luaL_tolstring(L, i, &len);
    if (i > 1) self->output_ += '\t';
    self->output_.append(s, len);
    lua_pop(L, 1);
  }
  self->output_ += '\n';
  if (self->output_.size() > kMaxOutput) {
    self->output_.erase(0, self->output_.size() - kMaxOutput);
  }
  return 0;
}

void Sandbox::openSafeLibs() {
  luaL_requiref(L_, LUA_GNAME, luaopen_base, 1);
  luaL_requiref(L_, LUA_STRLIBNAME, luaopen_string, 1);
  luaL_requiref(L_, LUA_TABLIBNAME, luaopen_table, 1);
  luaL_requiref(L_, LUA_MATHLIBNAME, luaopen_math, 1);
  luaL_requiref(L_, LUA_UTF8LIBNAME, luaopen_utf8, 1);
  luaL_requiref(L_, LUA_COLIBNAME, luaopen_coroutine, 1);
  lua_pop(L_, 6);

  // Remove anything that reaches the filesystem or loads bytecode.
  for (const char* name : {"dofile", "loadfile", "load", "collectgarbage"}) {
    lua_pushnil(L_);
    lua_setglobal(L_, name);
  }
  lua_pushcfunction(L_, &Sandbox::luaPrint);
  lua_setglobal(L_, "print");
}

void Sandbox::registerModules() {
  struct Module {
    uint32_t bit;
    const char* name;
    const luaL_Reg* fns;
  };
  const Module modules[] = {
      {kPermDisplay, "display", kDisplay}, {kPermGpio, "gpio", kGpio},     {kPermImu, "imu", kImu},
      {kPermI2c, "i2c", kI2c},             {kPermWifiScan, "wifi", kWifi}, {kPermFiles, "files", kFiles},
      {kPermSys, "sys", kSys},
  };
  for (const Module& m : modules) {
    if ((permissions_ & m.bit) == 0) continue;
    lua_newtable(L_);
    luaL_setfuncs(L_, m.fns, 0);
    lua_setglobal(L_, m.name);
  }
}

void Sandbox::beginCall() {
  output_.clear();
  executed_ = 0;
}

RunResult Sandbox::endCall(int status) {
  RunResult r;
  r.ok = status == LUA_OK;
  r.output = output_;
  if (!r.ok) {
    const char* msg = lua_tostring(L_, -1);
    r.error = msg != nullptr ? msg : (status == LUA_ERRMEM ? "out of memory" : "error");
    lua_pop(L_, 1);
  }
  return r;
}

RunResult Sandbox::run(const std::string& code, const char* chunkName) {
  if (L_ == nullptr) {
    RunResult r;
    r.error = "sandbox failed to start";
    return r;
  }
  beginCall();
  int status = luaL_loadbufferx(L_, code.data(), code.size(), chunkName, "t");
  if (status == LUA_OK) status = lua_pcall(L_, 0, 0, 0);
  return endCall(status);
}

RunResult Sandbox::eval(const std::string& line) {
  if (L_ == nullptr) return run(line);
  std::string expr = "return " + line;
  if (luaL_loadbufferx(L_, expr.data(), expr.size(), "=console", "t") != LUA_OK) {
    lua_pop(L_, 1);
    return run(line, "=console");
  }
  beginCall();
  int base = lua_gettop(L_) - 1;
  int status = lua_pcall(L_, 0, LUA_MULTRET, 0);
  if (status != LUA_OK) return endCall(status);
  int n = lua_gettop(L_) - base;
  if (n > 0) {
    lua_pushcfunction(L_, &Sandbox::luaPrint);
    lua_insert(L_, base + 1);
    status = lua_pcall(L_, n, 0, 0);
  }
  return endCall(status);
}

}  // namespace cardlab
