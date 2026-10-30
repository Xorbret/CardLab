#include "game.h"

#include <algorithm>

#include "content.h"

namespace cardlab {

namespace {

bool parseOutcome(const std::string& s, Outcome& out) {
  if (s == "stop") out = Outcome::Stopped;
  else if (s == "partial") out = Outcome::Partial;
  else if (s == "fail") out = Outcome::Failed;
  else return false;
  return true;
}

}  // namespace

const char* outcomeName(Outcome o) {
  switch (o) {
    case Outcome::Stopped: return "Attack stopped";
    case Outcome::Partial: return "Partly contained";
    default: return "Attack succeeded";
  }
}

bool loadScenario(Hal* hal, const std::string& source, Scenario& out, std::string& err) {
  // Scenario files only describe data, so they get no hardware access.
  Sandbox box(hal, kPermNone);
  lua_State* L = box.state();
  if (!loadContentTable(box, source, "=scenario", err)) return false;

  out = Scenario();
  out.id = fieldString(L, -1, "id");
  out.title = fieldString(L, -1, "title", out.id);
  out.briefing = fieldString(L, -1, "briefing");
  out.pointsPerRound = static_cast<int>(fieldInt(L, -1, "points", 3));

  forEachItem(L, -1, "rounds", [&](size_t i) {
    Round r;
    r.symptom = fieldString(L, -1, "symptom");
    if (lua_getfield(L, -1, "damage") == LUA_TTABLE) {
      r.uptimeDamage = static_cast<int>(fieldInt(L, -1, "uptime"));
      r.dataDamage = static_cast<int>(fieldInt(L, -1, "data"));
    }
    lua_pop(L, 1);
    forEachItem(L, -1, "evidence", [&](size_t) {
      Evidence e;
      e.name = fieldString(L, -1, "name");
      e.cost = static_cast<int>(fieldInt(L, -1, "cost", 1));
      e.text = fieldString(L, -1, "text");
      r.evidence.push_back(e);
    });
    forEachItem(L, -1, "defenses", [&](size_t j) {
      Defense d;
      d.name = fieldString(L, -1, "name");
      d.text = fieldString(L, -1, "text");
      if (!parseOutcome(fieldString(L, -1, "result"), d.result) && err.empty()) {
        err = "round " + std::to_string(i + 1) + " defense " + std::to_string(j + 1) +
              ": result must be stop, partial or fail";
      }
      r.defenses.push_back(d);
    });
    if (lua_getfield(L, -1, "reveal") == LUA_TTABLE) {
      r.reveal.attack = fieldString(L, -1, "attack");
      r.reveal.target = fieldString(L, -1, "target");
      r.reveal.cia = fieldString(L, -1, "cia");
      r.reveal.technique = fieldString(L, -1, "technique");
      r.reveal.lesson = fieldString(L, -1, "lesson");
    }
    lua_pop(L, 1);
    if (r.defenses.empty() && err.empty()) err = "round " + std::to_string(i + 1) + " has no defenses";
    out.rounds.push_back(r);
  });
  lua_pop(L, 1);

  if (err.empty() && out.id.empty()) err = "scenario needs an id";
  if (err.empty() && out.rounds.empty()) err = "scenario needs at least one round";
  return err.empty();
}

Game::Game(Scenario scenario) : scenario_(std::move(scenario)) { startRound(); }

void Game::startRound() {
  points_ = scenario_.pointsPerRound;
  seen_.assign(finished() ? 0 : round().evidence.size(), false);
}

bool Game::seen(size_t evidenceIndex) const { return evidenceIndex < seen_.size() && seen_[evidenceIndex]; }

bool Game::investigate(size_t evidenceIndex) {
  if (finished() || evidenceIndex >= round().evidence.size()) return false;
  if (seen_[evidenceIndex]) return true;
  int cost = round().evidence[evidenceIndex].cost;
  if (cost > points_) return false;
  points_ -= cost;
  seen_[evidenceIndex] = true;
  return true;
}

Outcome Game::defend(size_t defenseIndex) {
  if (finished() || defenseIndex >= round().defenses.size()) return Outcome::Failed;
  const Round& r = round();
  const Defense& d = r.defenses[defenseIndex];

  int base = 0;
  switch (d.result) {
    case Outcome::Stopped:
      base = 100;
      break;
    case Outcome::Partial:
      base = 50;
      uptime_ -= r.uptimeDamage / 2;
      data_ -= r.dataDamage / 2;
      break;
    case Outcome::Failed:
      uptime_ -= r.uptimeDamage;
      data_ -= r.dataDamage;
      break;
  }
  uptime_ = std::max(uptime_, 0);
  data_ = std::max(data_, 0);
  // Unspent points reward solving a round with less investigation, but
  // only when the response actually helped.
  score_ += base + (d.result == Outcome::Failed ? 0 : 10 * points_);

  last_ = &r;
  lastDefense_ = &d;
  ++round_;
  startRound();
  return d.result;
}

}  // namespace cardlab
