// Helpers for reading lesson and scenario files. Both are Lua files that
// `return { ... }` a table, so content needs no separate parser.
#pragma once

#include <string>
#include <vector>

#include "sandbox.h"

namespace cardlab {

// Runs a content chunk and leaves the returned table on top of the stack.
// Returns false (stack unchanged) and fills err on failure.
bool loadContentTable(Sandbox& box, const std::string& source, const char* chunkName, std::string& err);

// Field readers for the table at `idx`. Missing fields give the default.
std::string fieldString(lua_State* L, int idx, const char* key, const std::string& def = "");
long fieldInt(lua_State* L, int idx, const char* key, long def = 0);
std::vector<std::string> fieldStringList(lua_State* L, int idx, const char* key);

// Calls fn(i) with the i-th element (0-based) of array field `key` pushed
// on top of the stack. The element is popped after each call.
template <typename Fn>
void forEachItem(lua_State* L, int idx, const char* key, Fn fn) {
  idx = lua_absindex(L, idx);
  if (lua_getfield(L, idx, key) != LUA_TTABLE) {
    lua_pop(L, 1);
    return;
  }
  lua_Integer n = luaL_len(L, -1);
  for (lua_Integer i = 1; i <= n; ++i) {
    lua_rawgeti(L, -1, i);
    fn(static_cast<size_t>(i - 1));
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
}

}  // namespace cardlab
