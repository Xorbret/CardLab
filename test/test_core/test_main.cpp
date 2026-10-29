// Native tests for the platform-independent core: `pio test -e native`.
#include <unity.h>

#include <dirent.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

#include "game.h"
#include "lesson.h"
#include "progress.h"
#include "sandbox.h"

using namespace cardlab;

namespace {

class FakeHal : public Hal {
 public:
  std::string screen;
  std::map<int, int> pins;
  std::map<std::string, std::string> files;
  std::vector<uint8_t> i2c = {0x44, 0x68};
  ImuSample imu;

  FakeHal() { imu.az = 1.0f; }

  void displayClear() override { screen.clear(); }
  void displayPrint(const std::string& t) override { screen += t; }
  bool gpioAllowed(int pin) override { return pin == 1 || pin == 2; }
  void gpioMode(int, bool) override {}
  void gpioWrite(int pin, int v) override { pins[pin] = v; }
  int gpioRead(int pin) override { return pins[pin]; }
  bool imuRead(ImuSample& out) override {
    out = imu;
    return true;
  }
  std::vector<uint8_t> i2cScan() override { return i2c; }
  std::vector<WifiNetwork> wifiScan() override {
    return {{"Office", -50, "wpa2", 6}, {"CafeFree", -70, "open", 1}};
  }
  bool fileRead(const std::string& p, std::string& out) override {
    auto it = files.find(p);
    if (it == files.end()) return false;
    out = it->second;
    return true;
  }
  bool fileWrite(const std::string& p, const std::string& d) override {
    files[p] = d;
    return true;
  }
  uint32_t millis() override { return 1234; }
  void delayMs(uint32_t) override {}
};

// Content paths are relative to the project root. Fall back to this file's
// location in case the test runner starts somewhere else.
std::string projectPath(const std::string& rel) {
  if (access(rel.c_str(), F_OK) == 0) return rel;
  std::string self = __FILE__;
  for (int i = 0; i < 3; ++i) self = self.substr(0, self.find_last_of('/'));
  return self + "/" + rel;
}

std::string readFile(const std::string& path) {
  std::ifstream in(projectPath(path));
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<std::string> listLua(const std::string& dir) {
  std::vector<std::string> out;
  if (DIR* d = opendir(projectPath(dir).c_str())) {
    while (dirent* e = readdir(d)) {
      std::string n = e->d_name;
      if (n.size() > 4 && n.substr(n.size() - 4) == ".lua") out.push_back(dir + "/" + n);
    }
    closedir(d);
  }
  std::sort(out.begin(), out.end());
  return out;
}

FakeHal* hal;

}  // namespace

void setUp() { hal = new FakeHal(); }
void tearDown() { delete hal; }

// Sandbox ----------------------------------------------------------------

void test_sandbox_captures_print() {
  Sandbox box(hal, kPermNone);
  RunResult r = box.run("print('a', 1) print(true)");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("a\t1\ntrue\n", r.output.c_str());
}

void test_sandbox_reports_errors() {
  Sandbox box(hal, kPermNone);
  RunResult r = box.run("error('boom')");
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, r.error.find("boom"));
}

void test_sandbox_stops_infinite_loop() {
  Sandbox box(hal, kPermNone);
  RunResult r = box.run("while true do end");
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, r.error.find("time limit"));
}

void test_sandbox_limits_memory() {
  Sandbox box(hal, kPermNone);
  RunResult r = box.run("local t = {} for i = 1, 1e6 do t[i] = ('x'):rep(100) .. i end");
  TEST_ASSERT_FALSE(r.ok);
}

void test_sandbox_has_no_filesystem_or_load() {
  Sandbox box(hal, kPermAll);
  RunResult r = box.run("print(io, os, dofile, loadfile, load, require)");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("nil\tnil\tnil\tnil\tnil\tnil\n", r.output.c_str());
}

void test_sandbox_modules_follow_permissions() {
  Sandbox none(hal, kPermNone);
  TEST_ASSERT_FALSE(none.run("imu.read()").ok);
  Sandbox imu(hal, kPermImu);
  RunResult r = imu.run("local _, _, z = imu.read() print(z)");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("1.0\n", r.output.c_str());
}

void test_sandbox_blocks_unsafe_pins() {
  Sandbox box(hal, kPermGpio);
  TEST_ASSERT_TRUE(box.run("gpio.mode(2, 'out') gpio.write(2, 1)").ok);
  TEST_ASSERT_EQUAL(1, hal->pins[2]);
  TEST_ASSERT_TRUE(box.run("gpio.write(2, 0)").ok);
  TEST_ASSERT_EQUAL(0, hal->pins[2]);
  RunResult r = box.run("gpio.write(40, 1)");
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL(0u, hal->pins.count(40));
}

void test_sandbox_blocks_path_escape() {
  Sandbox box(hal, kPermFiles);
  TEST_ASSERT_TRUE(box.run("files.write('notes.txt', 'hi')").ok);
  TEST_ASSERT_EQUAL_STRING("hi", hal->files["notes.txt"].c_str());
  TEST_ASSERT_FALSE(box.run("files.read('../progress.txt')").ok);
  TEST_ASSERT_FALSE(box.run("files.write('/cardlab/x', 'y')").ok);
}

void test_sandbox_refuses_bytecode() {
  Sandbox box(hal, kPermNone);
  std::string bytecode("\x1bLua", 4);
  TEST_ASSERT_FALSE(box.run(bytecode).ok);
}

void test_sandbox_eval_prints_expressions() {
  Sandbox box(hal, kPermNone);
  TEST_ASSERT_EQUAL_STRING("2\n", box.eval("1+1").output.c_str());
  TEST_ASSERT_TRUE(box.eval("x = 5").ok);
  TEST_ASSERT_EQUAL_STRING("5\t6\n", box.eval("x, x + 1").output.c_str());
  // A failing expression must run once, not again as a statement.
  box.run("calls = 0 function f() calls = calls + 1 error('boom') end");
  TEST_ASSERT_FALSE(box.eval("f()").ok);
  TEST_ASSERT_EQUAL_STRING("1\n", box.eval("calls").output.c_str());
}

// Lessons ----------------------------------------------------------------

void test_every_lesson_loads() {
  std::vector<std::string> files = listLua("content/lessons");
  TEST_ASSERT_TRUE(files.size() >= 5);
  for (const std::string& f : files) {
    Lesson l(hal);
    std::string err;
    TEST_ASSERT_TRUE_MESSAGE(l.load(readFile(f), err), (f + ": " + err).c_str());
    TEST_ASSERT_FALSE_MESSAGE(l.pages().empty(), f.c_str());
  }
}

void test_lesson_grades_hello() {
  Lesson l(hal);
  std::string err;
  TEST_ASSERT_TRUE(l.load(readFile("content/lessons/t0-01-hello-lua.lua"), err));
  TEST_ASSERT_FALSE(l.grade(l.starter()).passed);
  GradeResult g = l.grade("print('hello, cardlab')");
  TEST_ASSERT_TRUE_MESSAGE(g.passed, g.feedback.c_str());
  TEST_ASSERT_EQUAL(1, (int)l.quiz().size());
  TEST_ASSERT_EQUAL(1, l.quiz()[0].answer);
}

void test_lesson_check_uses_hardware() {
  Lesson l(hal);
  std::string err;
  TEST_ASSERT_TRUE(l.load(readFile("content/lessons/t1-02-i2c-scan.lua"), err));
  TEST_ASSERT_TRUE(l.grade("local f = i2c.scan() print(#f)").passed);
  TEST_ASSERT_FALSE(l.grade("print(5)").passed);
}

void test_lesson_learner_gets_only_lesson_permissions() {
  Lesson l(hal);
  std::string err;
  TEST_ASSERT_TRUE(l.load(readFile("content/lessons/t0-01-hello-lua.lua"), err));
  GradeResult g = l.grade("gpio.write(2, 1)");
  TEST_ASSERT_FALSE(g.run.ok);
  TEST_ASSERT_EQUAL(0u, hal->pins.count(2));
}

void test_lesson_rejects_unknown_permission() {
  Lesson l(hal);
  std::string err;
  TEST_ASSERT_FALSE(l.load("return { id = 'x', permissions = { 'deauth' } }", err));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, err.find("deauth"));
}

// Game -------------------------------------------------------------------

void test_every_scenario_loads() {
  std::vector<std::string> files = listLua("content/scenarios");
  TEST_ASSERT_TRUE(files.size() >= 2);
  for (const std::string& f : files) {
    Scenario s;
    std::string err;
    TEST_ASSERT_TRUE_MESSAGE(loadScenario(hal, readFile(f), s, err), (f + ": " + err).c_str());
  }
}

void test_game_round_flow() {
  Scenario s;
  std::string err;
  TEST_ASSERT_TRUE(loadScenario(hal, readFile("content/scenarios/s01-office-wifi.lua"), s, err));
  Game g(s);
  TEST_ASSERT_EQUAL(3, g.pointsLeft());
  TEST_ASSERT_TRUE(g.investigate(0));
  TEST_ASSERT_EQUAL(2, g.pointsLeft());
  TEST_ASSERT_TRUE(g.investigate(0));  // re-reading is free
  TEST_ASSERT_EQUAL(2, g.pointsLeft());
  TEST_ASSERT_TRUE(g.investigate(1));
  TEST_ASSERT_FALSE(g.investigate(2));  // out of points
  TEST_ASSERT_EQUAL((int)Outcome::Stopped, (int)g.defend(0));
  TEST_ASSERT_EQUAL(100, g.uptime());
  TEST_ASSERT_EQUAL(100, g.score());
  TEST_ASSERT_EQUAL_STRING("Deauthentication attack", g.lastRound()->reveal.attack.c_str());

  TEST_ASSERT_EQUAL(1u, g.roundIndex());
  TEST_ASSERT_EQUAL(3, g.pointsLeft());
  TEST_ASSERT_EQUAL((int)Outcome::Failed, (int)g.defend(1));
  TEST_ASSERT_EQUAL(50, g.data());
  TEST_ASSERT_EQUAL(100, g.score());  // no bonus for a failed response
  TEST_ASSERT_TRUE(g.finished());
}

void test_scenario_rejects_bad_result() {
  Scenario s;
  std::string err;
  const char* src = "return { id='x', rounds = { { symptom='s', defenses = { { name='d', result='maybe' } } } } }";
  TEST_ASSERT_FALSE(loadScenario(hal, src, s, err));
}

// Progress ---------------------------------------------------------------

void test_progress_round_trip() {
  Progress p;
  p.recordTask("a", true);
  p.recordTask("a", false);  // a later failure does not erase a pass
  p.recordQuiz("a", 1, 2);
  p.recordQuiz("a", 0, 2);  // keeps the best score
  p.recordScenario("s1", 150);
  p.recordScenario("s1", 90);

  Progress q;
  q.parse(p.serialize());
  TEST_ASSERT_TRUE(q.lesson("a")->taskPassed);
  TEST_ASSERT_EQUAL(1, q.lesson("a")->quizScore);
  TEST_ASSERT_EQUAL(150, q.bestScenarioScore("s1"));
  TEST_ASSERT_EQUAL(-1, q.bestScenarioScore("nope"));
  TEST_ASSERT_EQUAL_STRING(
      "{\"lessons\":{\"a\":{\"taskPassed\":true,\"quizScore\":1,\"quizTotal\":2}},\"scenarios\":{\"s1\":150}}",
      q.toJson().c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_sandbox_captures_print);
  RUN_TEST(test_sandbox_reports_errors);
  RUN_TEST(test_sandbox_stops_infinite_loop);
  RUN_TEST(test_sandbox_limits_memory);
  RUN_TEST(test_sandbox_has_no_filesystem_or_load);
  RUN_TEST(test_sandbox_modules_follow_permissions);
  RUN_TEST(test_sandbox_blocks_unsafe_pins);
  RUN_TEST(test_sandbox_blocks_path_escape);
  RUN_TEST(test_sandbox_refuses_bytecode);
  RUN_TEST(test_sandbox_eval_prints_expressions);
  RUN_TEST(test_every_lesson_loads);
  RUN_TEST(test_lesson_grades_hello);
  RUN_TEST(test_lesson_check_uses_hardware);
  RUN_TEST(test_lesson_learner_gets_only_lesson_permissions);
  RUN_TEST(test_lesson_rejects_unknown_permission);
  RUN_TEST(test_every_scenario_loads);
  RUN_TEST(test_game_round_flow);
  RUN_TEST(test_scenario_rejects_bad_result);
  RUN_TEST(test_progress_round_trip);
  return UNITY_END();
}
