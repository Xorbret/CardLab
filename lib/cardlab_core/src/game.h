// "Defend the Network": a turn-based Blue Team game. Every attack is
// simulated on screen; nothing is transmitted. See content/README.md.
#pragma once

#include <string>
#include <vector>

#include "hal.h"

namespace cardlab {

enum class Outcome { Stopped, Partial, Failed };

struct Evidence {
  std::string name;
  int cost = 1;
  std::string text;
};

struct Defense {
  std::string name;
  Outcome result = Outcome::Failed;
  std::string text;  // why it worked or did not
};

struct Reveal {
  std::string attack;     // e.g. "Deauthentication attack"
  std::string target;     // what it goes after, e.g. "802.11 management frames"
  std::string cia;        // Confidentiality / Integrity / Availability
  std::string technique;  // optional framework reference
  std::string lesson;     // lesson id to study next
};

struct Round {
  std::string symptom;
  int uptimeDamage = 0;
  int dataDamage = 0;
  std::vector<Evidence> evidence;
  std::vector<Defense> defenses;
  Reveal reveal;
};

struct Scenario {
  std::string id;
  std::string title;
  std::string briefing;
  int pointsPerRound = 3;
  std::vector<Round> rounds;
};

// Parses a scenario file (a Lua file returning a table).
bool loadScenario(Hal* hal, const std::string& source, Scenario& out, std::string& err);

class Game {
 public:
  explicit Game(Scenario scenario);

  const Scenario& scenario() const { return scenario_; }
  bool finished() const { return round_ >= scenario_.rounds.size(); }
  size_t roundIndex() const { return round_; }
  const Round& round() const { return scenario_.rounds[round_]; }

  int uptime() const { return uptime_; }
  int data() const { return data_; }
  int pointsLeft() const { return points_; }
  int score() const { return score_; }

  bool seen(size_t evidenceIndex) const;

  // Spends action points to look at a piece of evidence. Re-reading is
  // free. Returns false if the index is bad or points run out.
  bool investigate(size_t evidenceIndex);

  // Chooses a response, applies damage, scores the round and moves on.
  // The finished round stays readable through lastRound() for the reveal.
  Outcome defend(size_t defenseIndex);
  const Round* lastRound() const { return last_; }
  const Defense* lastDefense() const { return lastDefense_; }

 private:
  void startRound();

  Scenario scenario_;
  size_t round_ = 0;
  int uptime_ = 100;
  int data_ = 100;
  int points_ = 0;
  int score_ = 0;
  std::vector<bool> seen_;
  const Round* last_ = nullptr;
  const Defense* lastDefense_ = nullptr;
};

const char* outcomeName(Outcome o);

}  // namespace cardlab
