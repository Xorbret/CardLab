// A lesson: reading pages, a code task graded by the lesson's own check(),
// a short quiz and a reflection prompt. See content/README.md for the format.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sandbox.h"

namespace cardlab {

struct QuizItem {
  std::string question;
  std::vector<std::string> choices;
  int answer = 0;  // 0-based index into choices
  std::string why;
};

struct GradeResult {
  bool passed = false;
  std::string feedback;
  RunResult run;  // the learner's run, for showing output and errors
};

class Lesson {
 public:
  explicit Lesson(Hal* hal) : hal_(hal) {}

  bool load(const std::string& source, std::string& err);

  // Runs the learner's code in a fresh sandbox with only the permissions
  // this lesson asked for, then passes the result to the lesson's check().
  GradeResult grade(const std::string& learnerCode);

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  int track() const { return track_; }
  const std::vector<std::string>& pages() const { return pages_; }
  const std::string& task() const { return task_; }
  const std::string& starter() const { return starter_; }
  const std::vector<QuizItem>& quiz() const { return quiz_; }
  const std::string& reflect() const { return reflect_; }
  uint32_t permissions() const { return permissions_; }
  bool hasTask() const { return checkRef_ != LUA_NOREF; }

 private:
  Hal* hal_;
  std::unique_ptr<Sandbox> content_;
  int checkRef_ = LUA_NOREF;
  std::string id_, title_, task_, starter_, reflect_;
  int track_ = 0;
  uint32_t permissions_ = kPermNone;
  std::vector<std::string> pages_;
  std::vector<QuizItem> quiz_;
};

}  // namespace cardlab
