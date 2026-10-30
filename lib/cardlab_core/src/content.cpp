#include "content.h"

namespace cardlab {

bool loadContentTable(Sandbox& box, const std::string& source, const char* chunkName, std::string& err) {
  lua_State* L = box.state();
  if (L == nullptr) {
    err = "sandbox failed to start";
    return false;
  }
  box.beginCall();
  int status = luaL_loadbufferx(L, source.data(), source.size(), chunkName, "t");
  if (status == LUA_OK) status = lua_pcall(L, 0, 1, 0);
  if (status != LUA_OK) {
    err = box.endCall(status).error;
    return false;
  }
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    err = "content file must return a table";
    return false;
  }
  return true;
}

std::string fieldString(lua_State* L, int idx, const char* key, const std::string& def) {
  std::string out = def;
  if (lua_getfield(L, idx, key) == LUA_TSTRING) {
    size_t len = 0;
    const char* s = lua_tolstring(L, -1, &len);
    out.assign(s, len);
  }
  lua_pop(L, 1);
  return out;
}

long fieldInt(lua_State* L, int idx, const char* key, long def) {
  long out = def;
  if (lua_getfield(L, idx, key) == LUA_TNUMBER) out = static_cast<long>(lua_tointeger(L, -1));
  lua_pop(L, 1);
  return out;
}

std::vector<std::string> fieldStringList(lua_State* L, int idx, const char* key) {
  std::vector<std::string> out;
  forEachItem(L, idx, key, [&](size_t) {
    if (lua_type(L, -1) == LUA_TSTRING) out.emplace_back(lua_tostring(L, -1));
  });
  return out;
}

}  // namespace cardlab
