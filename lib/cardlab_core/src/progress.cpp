#include "progress.h"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace cardlab {

namespace {

std::vector<std::string> split(const std::string& line, char sep) {
  std::vector<std::string> parts;
  std::string cur;
  for (char c : line) {
    if (c == sep) {
      parts.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  parts.push_back(cur);
  return parts;
}

std::string jsonEscape(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      default:
        if (static_cast<unsigned char>(c) >= 0x20) out += c;
    }
  }
  return out;
}

}  // namespace

void Progress::recordTask(const std::string& lessonId, bool passed) {
  LessonRecord& r = lessons_[lessonId];
  r.taskPassed = r.taskPassed || passed;
}

void Progress::recordQuiz(const std::string& lessonId, int score, int total) {
  LessonRecord& r = lessons_[lessonId];
  if (r.quizTotal == 0 || score * r.quizTotal > r.quizScore * total) {
    r.quizScore = score;
    r.quizTotal = total;
  }
}

void Progress::recordScenario(const std::string& scenarioId, int score) {
  auto it = scenarios_.find(scenarioId);
  if (it == scenarios_.end() || score > it->second) scenarios_[scenarioId] = score;
}

const LessonRecord* Progress::lesson(const std::string& lessonId) const {
  auto it = lessons_.find(lessonId);
  return it == lessons_.end() ? nullptr : &it->second;
}

int Progress::bestScenarioScore(const std::string& scenarioId) const {
  auto it = scenarios_.find(scenarioId);
  return it == scenarios_.end() ? -1 : it->second;
}

size_t Progress::lessonsPassed() const {
  size_t n = 0;
  for (const auto& kv : lessons_) n += kv.second.taskPassed ? 1 : 0;
  return n;
}

std::string Progress::serialize() const {
  std::ostringstream out;
  for (const auto& kv : lessons_) {
    out << "L|" << kv.first << '|' << (kv.second.taskPassed ? 1 : 0) << '|' << kv.second.quizScore << '|'
        << kv.second.quizTotal << '\n';
  }
  for (const auto& kv : scenarios_) out << "S|" << kv.first << '|' << kv.second << '\n';
  return out.str();
}

void Progress::parse(const std::string& text) {
  lessons_.clear();
  scenarios_.clear();
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    std::vector<std::string> p = split(line, '|');
    if (p.size() == 5 && p[0] == "L" && !p[1].empty()) {
      LessonRecord& r = lessons_[p[1]];
      r.taskPassed = p[2] == "1";
      r.quizScore = std::atoi(p[3].c_str());
      r.quizTotal = std::atoi(p[4].c_str());
    } else if (p.size() == 3 && p[0] == "S" && !p[1].empty()) {
      scenarios_[p[1]] = std::atoi(p[2].c_str());
    }
  }
}

std::string Progress::toJson() const {
  std::ostringstream out;
  out << "{\"lessons\":{";
  bool first = true;
  for (const auto& kv : lessons_) {
    out << (first ? "" : ",") << '"' << jsonEscape(kv.first) << "\":{\"taskPassed\":"
        << (kv.second.taskPassed ? "true" : "false") << ",\"quizScore\":" << kv.second.quizScore
        << ",\"quizTotal\":" << kv.second.quizTotal << '}';
    first = false;
  }
  out << "},\"scenarios\":{";
  first = true;
  for (const auto& kv : scenarios_) {
    out << (first ? "" : ",") << '"' << jsonEscape(kv.first) << "\":" << kv.second;
    first = false;
  }
  out << "}}";
  return out.str();
}

}  // namespace cardlab
