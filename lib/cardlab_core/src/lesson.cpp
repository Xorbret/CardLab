#include "lesson.h"

#include "content.h"

namespace cardlab {

bool Lesson::load(const std::string& source, std::string& err) {
  content_.reset(new Sandbox(hal_, kPermReadOnly));
  lua_State* L = content_->state();
  if (!loadContentTable(*content_, source, "=lesson", err)) return false;

  id_ = fieldString(L, -1, "id");
  title_ = fieldString(L, -1, "title", id_);
  if (id_.empty()) {
    lua_pop(L, 1);
    err = "lesson needs an id";
    return false;
  }
  track_ = static_cast<int>(fieldInt(L, -1, "track"));
  task_ = fieldString(L, -1, "task");
  starter_ = fieldString(L, -1, "starter");
  reflect_ = fieldString(L, -1, "reflect");
  pages_ = fieldStringList(L, -1, "pages");

  permissions_ = kPermNone;
  for (const std::string& name : fieldStringList(L, -1, "permissions")) {
    bool ok = false;
    uint32_t bit = permissionFromName(name, ok);
    if (!ok) {
      lua_pop(L, 1);
      err = "unknown permission: " + name;
      return false;
    }
    permissions_ |= bit;
  }

  quiz_.clear();
  forEachItem(L, -1, "quiz", [&](size_t) {
    QuizItem q;
    q.question = fieldString(L, -1, "q");
    q.choices = fieldStringList(L, -1, "choices");
    q.answer = static_cast<int>(fieldInt(L, -1, "answer", 1)) - 1;  // 1-based in Lua
    q.why = fieldString(L, -1, "why");
    if (!q.question.empty() && q.answer >= 0 && q.answer < static_cast<int>(q.choices.size())) {
      quiz_.push_back(q);
    }
  });

  if (lua_getfield(L, -1, "check") == LUA_TFUNCTION) {
    checkRef_ = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    lua_pop(L, 1);
    checkRef_ = LUA_NOREF;
  }
  lua_pop(L, 1);  // lesson table
  return true;
}

GradeResult Lesson::grade(const std::string& learnerCode) {
  GradeResult g;
  {
    Sandbox learner(hal_, permissions_);
    g.run = learner.run(learnerCode);
  }
  if (checkRef_ == LUA_NOREF || !content_) {
    g.passed = g.run.ok;
    g.feedback = g.run.ok ? "Ran without errors." : g.run.error;
    return g;
  }

  lua_State* L = content_->state();
  content_->beginCall();
  lua_rawgeti(L, LUA_REGISTRYINDEX, checkRef_);
  lua_createtable(L, 0, 3);
  lua_pushboolean(L, g.run.ok);
  lua_setfield(L, -2, "ok");
  lua_pushlstring(L, g.run.output.data(), g.run.output.size());
  lua_setfield(L, -2, "output");
  lua_pushlstring(L, g.run.error.data(), g.run.error.size());
  lua_setfield(L, -2, "error");
  lua_pushlstring(L, learnerCode.data(), learnerCode.size());
  lua_setfield(L, -2, "code");

  int status = lua_pcall(L, 1, 2, 0);
  if (status != LUA_OK) {
    g.passed = false;
    g.feedback = "Lesson check failed: " + content_->endCall(status).error;
    return g;
  }
  g.passed = lua_toboolean(L, -2);
  const char* msg = lua_tostring(L, -1);
  g.feedback = msg != nullptr ? msg : (g.passed ? "Passed." : "Not yet. Try again.");
  lua_pop(L, 2);
  return g;
}

}  // namespace cardlab
