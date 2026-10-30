// Learner progress, saved to SD as one line per lesson and exportable as
// JSON for a portfolio.
#pragma once

#include <map>
#include <string>

namespace cardlab {

struct LessonRecord {
  bool taskPassed = false;
  int quizScore = 0;
  int quizTotal = 0;
};

class Progress {
 public:
  void recordTask(const std::string& lessonId, bool passed);
  void recordQuiz(const std::string& lessonId, int score, int total);
  void recordScenario(const std::string& scenarioId, int score);

  const LessonRecord* lesson(const std::string& lessonId) const;
  int bestScenarioScore(const std::string& scenarioId) const;  // -1 if never played
  size_t lessonsPassed() const;

  // Line format: "L|id|passed|score|total" and "S|id|score".
  std::string serialize() const;
  void parse(const std::string& text);
  std::string toJson() const;

 private:
  std::map<std::string, LessonRecord> lessons_;
  std::map<std::string, int> scenarios_;
};

}  // namespace cardlab
