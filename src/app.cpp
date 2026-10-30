#include "app.h"

#include "pack.h"

#include <M5Cardputer.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace cardlab {

namespace {

const char* kRules =
    "CardLab teaches how IoT devices work, how they are attacked, and how to defend them.\n\n"
    "Rules:\n"
    "- Only test hardware and networks you own or have written permission to test.\n"
    "- Wi-Fi tools here only listen. CardLab never sends deauth, jamming or other attack traffic.\n"
    "- Attacks are taught in the Defend the Network game, which is fully simulated.\n"
    "- Unauthorized access is a crime in most countries (e.g. the US Computer Fraud and Abuse Act).\n\n"
    "Press Enter to agree and continue.";

bool isBack(const Key& k) { return k.type == Key::Esc || (k.type == Key::Char && k.c == '`'); }

std::string fileStem(const std::string& path) {
  size_t slash = path.find_last_of('/');
  std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
  return name.substr(0, name.rfind('.'));
}

// Uptime and data-safety meters plus action-point pips for a game round.
void drawMeter(LovyanGFX& g, int x, int y, const char* label, int pct, uint16_t color) {
  g.setFont(ui::fontSmall());
  g.setTextDatum(middle_left);
  g.setTextColor(ui::kMuted);
  g.drawString(label, x, y + 6);
  int bx = x + g.textWidth(label) + 4, bw = 44;
  g.fillRoundRect(bx, y + 3, bw, 6, 2, ui::kSurface);
  int fill = bw * std::max(0, std::min(100, pct)) / 100;
  if (fill > 0) g.fillRoundRect(bx, y + 3, fill, 6, 2, pct < 50 ? ui::kRed : color);
}

void drawGameHud(LovyanGFX& g, int y, const Game& game) {
  drawMeter(g, ui::kPad, y + 1, "UP", game.uptime(), ui::kGreen);
  drawMeter(g, ui::kPad + 66, y + 1, "DATA", game.data(), ui::kTeal);
  int total = game.scenario().pointsPerRound;
  int x = ui::kW - ui::kPad - total * 9;
  g.setTextDatum(middle_right);
  g.setTextColor(ui::kMuted);
  g.drawString("AP", x - 3, y + 7);
  for (int i = 0; i < total; ++i) {
    if (i < game.pointsLeft()) g.fillCircle(x + 4 + i * 9, y + 7, 3, ui::kAmber);
    else g.drawCircle(x + 4 + i * 9, y + 7, 3, ui::kRule);
  }
  g.fillRect(ui::kPad, y + 15, ui::kW - 2 * ui::kPad, 1, ui::kRule);
}

}  // namespace

// App ----------------------------------------------------------------------

void App::begin() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  ui::begin();
  hal_.begin();

  std::string saved;
  if (hal_.readAbsolute(kProgressFile, saved)) progress_.parse(saved);

  showMainMenu();
  if (!hal_.exists(kAgreedFile)) {
    auto rules = std::unique_ptr<TextView>(new TextView(*this, "Before you start", kRules, [this] {
      hal_.writeAbsolute(kAgreedFile, "agreed\n");
      pop();
    }));
    rules->hintText = "Enter I agree";
    push(std::move(rules));
  }
}

void App::loop() {
  M5Cardputer.update();
  Key k = ui::poll();
  if (k.type != Key::None && !stack_.empty()) {
    stack_.back()->key(k);
    dirty_ = true;
  }
  if (pendingPop_ > 0 || !pendingPush_.empty()) {
    while (pendingPop_ > 0 && stack_.size() > 1) {
      stack_.pop_back();
      --pendingPop_;
    }
    pendingPop_ = 0;
    for (auto& v : pendingPush_) stack_.push_back(std::move(v));
    pendingPush_.clear();
    dirty_ = true;
  }
  if (dirty_ && !stack_.empty()) {
    stack_.back()->draw();
    dirty_ = false;
  }
  delay(10);
}

void App::push(std::unique_ptr<View> v) { pendingPush_.push_back(std::move(v)); }
void App::pop(int count) { pendingPop_ += count; }

void App::saveProgress() { hal_.writeAbsolute(kProgressFile, progress_.serialize()); }

void App::showError(const std::string& title, const std::string& message) {
  push(std::unique_ptr<View>(new TextView(*this, title, message)));
}

void App::showText(const std::string& title, const std::string& text, ui::Tone tone) {
  auto v = std::unique_ptr<TextView>(new TextView(*this, title, text));
  v->tone = tone;
  push(std::move(v));
}

void App::showMainMenu() {
  std::vector<HomeView::Item> items = {
      {kIconBook, "Lessons", "Lua, sensors, radios", ui::Tone::Normal},
      {kIconShield, "Defend the Network", "Turn-based blue team game", ui::Tone::Game},
      {kIconTerminal, "Lua console", "Try code on the device", ui::Tone::Normal},
      {kIconGear, "Tools", "I2C and Wi-Fi scans, setup", ui::Tone::Normal},
      {kIconDownload, "Update lessons", "Download new content", ui::Tone::Normal},
      {kIconInfo, "Rules and about", "Test only what you own", ui::Tone::Normal},
  };
  push(std::unique_ptr<View>(new HomeView(*this, items, [this](int i) {
    switch (i) {
      case 0: openLessons(); break;
      case 1: openScenarios(); break;
      case 2: push(std::unique_ptr<View>(new ConsoleView(*this))); break;
      case 3: openTools(); break;
      case 4: runUpdate(); break;
      default: showError("Rules and about", kRules); break;
    }
  })));
}

// Lessons -------------------------------------------------------------------

void App::openLessons() {
  if (!hal_.sdReady()) {
    showError("No SD card", "Copy the repo's content folder to /cardlab on a microSD card and restart.");
    return;
  }
  auto paths = std::make_shared<std::vector<std::string>>(hal_.listLua(kLessonDir));
  if (paths->empty()) {
    showError("No lessons", std::string("No .lua files in ") + kLessonDir + ". Use Update lessons on the main menu.");
    return;
  }
  std::vector<std::string> items, details;
  int done = 0;
  for (const std::string& p : *paths) {
    std::string src, err, label = fileStem(p), detail;
    Lesson l(&hal_);
    if (hal_.readAbsolute(p.c_str(), src) && l.load(src, err)) {
      const LessonRecord* r = progress_.lesson(l.id());
      label = l.title();
      if (r && r->taskPassed) {
        detail = "done";
        ++done;
      }
    }
    items.push_back(label);
    details.push_back(detail);
  }
  std::string title = "Lessons  " + std::to_string(done) + "/" + std::to_string(items.size());
  auto menu = std::unique_ptr<MenuView>(new MenuView(*this, title, items, [this, paths](int i) {
    openLesson(paths->at(i));
  }));
  menu->details = details;
  push(std::move(menu));
}

void App::openLesson(const std::string& path) {
  std::string src, err;
  if (!hal_.readAbsolute(path.c_str(), src)) return showError("Error", "Could not read " + path);
  auto lesson = std::make_shared<Lesson>(&hal_);
  if (!lesson->load(src, err)) return showError("Lesson error", err);

  std::string text;
  for (const std::string& page : lesson->pages()) text += page + "\n\n";
  if (lesson->hasTask()) text += "TASK: " + lesson->task();
  push(std::unique_ptr<View>(new TextView(*this, lesson->title(), text, [this, lesson] {
    pop();
    startTask(lesson);
  })));
}

void App::startTask(std::shared_ptr<Lesson> lesson) {
  if (!lesson->hasTask()) return startQuiz(lesson);
  push(std::unique_ptr<View>(new EditorView(*this, lesson->title(), lesson->starter(), [this, lesson](const std::string& code) {
    ui::busy("Running", "Running your code...");
    GradeResult g = lesson->grade(code);
    if (lesson->permissions() & kPermDisplay) ui::waitKey();  // let them see what it drew
    progress_.recordTask(lesson->id(), g.passed);
    saveProgress();

    std::string text = (g.passed ? "PASS: " : "NOT YET: ") + g.feedback;
    if (!g.run.output.empty()) text += "\n\nOutput:\n" + g.run.output;
    if (!g.run.ok) text += "\n\nError:\n" + g.run.error;
    bool passed = g.passed;
    auto result = std::unique_ptr<TextView>(new TextView(*this, passed ? "Passed" : "Not yet", text, [this, lesson, passed] {
      if (!passed) return pop();  // back to the editor
      pop(2);
      startQuiz(lesson);
    }));
    result->tone = passed ? ui::Tone::Good : ui::Tone::Bad;
    result->hintText = passed ? "Enter quiz" : "Enter edit";
    push(std::move(result));
  })));
}

void App::startQuiz(std::shared_ptr<Lesson> lesson) {
  askQuestion(lesson, 0, std::make_shared<int>(0));
}

void App::askQuestion(std::shared_ptr<Lesson> lesson, size_t index, std::shared_ptr<int> score) {
  const auto& quiz = lesson->quiz();
  if (index >= quiz.size()) {
    if (!quiz.empty()) {
      progress_.recordQuiz(lesson->id(), *score, static_cast<int>(quiz.size()));
      saveProgress();
    }
    std::string text = "Quiz: " + std::to_string(*score) + "/" + std::to_string(quiz.size()) + "\n\n";
    if (!lesson->reflect().empty()) text += "Reflect: " + lesson->reflect();
    showText("Lesson done", text, ui::Tone::Good);
    return;
  }
  const QuizItem& q = quiz[index];
  char head[24];
  std::snprintf(head, sizeof(head), "Question %u/%u", unsigned(index + 1), unsigned(quiz.size()));
  std::string title = head;
  push(std::unique_ptr<View>(new TextView(*this, title, q.question, [this, lesson, index, score, title] {
    pop();
    const QuizItem& q = lesson->quiz()[index];
    push(std::unique_ptr<View>(new MenuView(*this, title, q.choices, [this, lesson, index, score](int choice) {
      const QuizItem& q = lesson->quiz()[index];
      bool right = choice == q.answer;
      if (right) ++*score;
      std::string text = std::string(right ? "Correct. " : "Not quite. ") + q.why;
      auto answer = std::unique_ptr<TextView>(new TextView(*this, right ? "Correct" : "Answer", text, [this, lesson, index, score] {
        pop(2);
        askQuestion(lesson, index + 1, score);
      }));
      answer->tone = right ? ui::Tone::Good : ui::Tone::Bad;
      push(std::move(answer));
    })));
  })));
}

// Defend the Network --------------------------------------------------------

void App::openScenarios() {
  if (!hal_.sdReady()) {
    showError("No SD card", "Copy the repo's content folder to /cardlab on a microSD card and restart.");
    return;
  }
  auto paths = std::make_shared<std::vector<std::string>>(hal_.listLua(kScenarioDir));
  if (paths->empty()) return showError("No scenarios", std::string("No .lua files in ") + kScenarioDir + ". Use Update lessons on the main menu.");
  std::vector<std::string> items, details;
  for (const std::string& p : *paths) {
    std::string src, err, label = fileStem(p), detail;
    Scenario s;
    if (hal_.readAbsolute(p.c_str(), src) && loadScenario(&hal_, src, s, err)) {
      int best = progress_.bestScenarioScore(s.id);
      label = s.title;
      if (best >= 0) detail = "best " + std::to_string(best);
    }
    items.push_back(label);
    details.push_back(detail);
  }
  auto menu = std::unique_ptr<MenuView>(new MenuView(*this, "Defend the Network", items, [this, paths](int i) {
    startGame(paths->at(i));
  }));
  menu->details = details;
  menu->tone = ui::Tone::Game;
  push(std::move(menu));
}

void App::startGame(const std::string& path) {
  std::string src, err;
  Scenario s;
  if (!hal_.readAbsolute(path.c_str(), src)) return showError("Error", "Could not read " + path);
  if (!loadScenario(&hal_, src, s, err)) return showError("Scenario error", err);
  auto game = std::make_shared<Game>(std::move(s));
  std::string text = game->scenario().briefing +
                     "\n\nEach round shows a symptom. Spend action points on evidence, then pick a "
                     "response. You keep unspent points as bonus score.";
  auto brief = std::unique_ptr<TextView>(new TextView(*this, game->scenario().title, text, [this, game] {
    pop();
    showRound(game);
  }));
  brief->tone = ui::Tone::Game;
  brief->hintText = "Enter start";
  push(std::move(brief));
}

void App::showRound(std::shared_ptr<Game> game) {
  if (game->finished()) {
    progress_.recordScenario(game->scenario().id, game->score());
    saveProgress();
    std::string text = "SCORE: " + std::to_string(game->score()) + "\nUPTIME: " + std::to_string(game->uptime()) +
                       "%\nDATA SAFE: " + std::to_string(game->data()) + "%";
    showText("Debrief", text, ui::Tone::Game);
    return;
  }
  std::string title = "Round " + std::to_string(game->roundIndex() + 1) + "/" +
                      std::to_string(game->scenario().rounds.size());
  auto symptom = std::unique_ptr<TextView>(new TextView(*this, title, "SYMPTOM: " + game->round().symptom, [this, game] {
    pop();
    pushRoundMenu(game);
  }));
  symptom->tone = ui::Tone::Game;
  symptom->hintText = "Enter investigate";
  push(std::move(symptom));
}

void App::pushRoundMenu(std::shared_ptr<Game> game) {
  const Round& r = game->round();
  std::vector<std::string> items = {"Re-read symptom"}, details = {""};
  for (size_t i = 0; i < r.evidence.size(); ++i) {
    items.push_back(r.evidence[i].name);
    details.push_back(game->seen(i) ? "seen" : std::to_string(r.evidence[i].cost) + " AP");
  }
  items.push_back("Respond now");
  details.push_back("");
  std::string title = "Round " + std::to_string(game->roundIndex() + 1) + "/" +
                      std::to_string(game->scenario().rounds.size());

  auto menu = std::unique_ptr<MenuView>(new MenuView(*this, title, items, [this, game](int i) {
    const Round& r = game->round();
    if (i == 0) return showError("Symptom", r.symptom);
    size_t e = static_cast<size_t>(i - 1);
    if (e < r.evidence.size()) {
      if (!game->investigate(e)) return showError("No points", "Not enough action points. Time to respond.");
      auto ev = std::unique_ptr<TextView>(new TextView(*this, r.evidence[e].name, r.evidence[e].text, [this, game] {
        pop(2);  // evidence and the stale round menu
        pushRoundMenu(game);
      }));
      ev->tone = ui::Tone::Game;
      push(std::move(ev));
      return;
    }
    std::vector<std::string> names;
    for (const Defense& d : r.defenses) names.push_back(d.name);
    auto respond = std::unique_ptr<MenuView>(new MenuView(*this, "Pick a response", names, [this, game](int j) {
      Outcome o = game->defend(static_cast<size_t>(j));
      const Round& done = *game->lastRound();
      const Reveal& rv = done.reveal;
      std::string text = std::string(outcomeName(o)) + "\n" + game->lastDefense()->text + "\n\nATTACK: " + rv.attack +
                         "\nTARGET: " + rv.target + "\nCIA: " + rv.cia;
      if (!rv.technique.empty()) text += "\nREF: " + rv.technique;
      if (!rv.lesson.empty()) text += "\nSTUDY: lesson " + rv.lesson;
      auto reveal = std::unique_ptr<TextView>(new TextView(*this, "Reveal", text, [this, game] {
        pop(3);  // reveal, respond menu, round menu
        showRound(game);
      }));
      reveal->tone = o == Outcome::Stopped ? ui::Tone::Good : o == Outcome::Partial ? ui::Tone::Game : ui::Tone::Bad;
      push(std::move(reveal));
    }));
    respond->tone = ui::Tone::Game;
    push(std::move(respond));
  }));
  menu->details = details;
  menu->tone = ui::Tone::Game;
  menu->hud = [game](LovyanGFX& g, int y) { drawGameHud(g, y, *game); };
  push(std::move(menu));
}

// Tools ---------------------------------------------------------------------

void App::openTools() {
  std::vector<std::string> items = {"I2C scan (Grove)", "Wi-Fi scan (passive)", "Export progress JSON",
                                    "Wi-Fi setup (for updates)"};
  push(std::unique_ptr<View>(new MenuView(*this, "Tools", items, [this](int i) {
    if (i == 0) {
      std::string text;
      for (uint8_t a : hal_.i2cScan()) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "0x%02X\n", a);
        text += buf;
      }
      showError("I2C devices", text.empty() ? "None found. Plug a Grove sensor into the port." : text);
    } else if (i == 1) {
      ui::busy("Wi-Fi scan", "Listening for networks...");
      std::string text;
      for (const WifiNetwork& n : hal_.wifiScan()) {
        text += n.ssid + "\n  " + n.auth + " ch" + std::to_string(n.channel) + " " + std::to_string(n.rssi) + "dBm\n";
      }
      showError("Wi-Fi networks", text.empty() ? "No networks found." : text);
    } else if (i == 3) {
      setupWifi();
    } else {
      bool ok = hal_.writeAbsolute(kExportFile, progress_.toJson());
      showError("Export", ok ? std::string("Saved to ") + kExportFile : "Could not write to the SD card.");
    }
  })));
}

// Wi-Fi setup and content updates -------------------------------------------

void App::setupWifi() {
  if (!hal_.sdReady()) return showError("No SD card", "Wi-Fi settings are saved on the SD card.");
  ui::busy("Wi-Fi setup", "Looking for networks...");
  auto names = std::make_shared<std::vector<std::string>>();
  for (const WifiNetwork& n : hal_.wifiScan()) {
    if (n.ssid != "(hidden)") names->push_back(n.ssid);
  }
  std::vector<std::string> items = *names;
  items.push_back("> Type a network name");
  push(std::unique_ptr<View>(new MenuView(*this, "Pick your Wi-Fi", items, [this, names](int i) {
    if (i < static_cast<int>(names->size())) return askWifiPassword(names->at(i));
    push(std::unique_ptr<View>(new InputView(*this, "Wi-Fi setup", "Network name:", false,
                                             [this](const std::string& ssid) {
                                               pop();
                                               if (!ssid.empty()) askWifiPassword(ssid);
                                             })));
  })));
}

void App::askWifiPassword(const std::string& ssid) {
  push(std::unique_ptr<View>(new InputView(*this, ssid, "Password (blank if open):", true,
                                           [this, ssid](const std::string& password) {
    pop(2);  // password entry and the network list
    ui::busy("Wi-Fi setup", "Connecting to " + ssid + "...");
    bool ok = hal_.wifiConnect(ssid, password, 15000);
    hal_.wifiOff();
    if (!ok) return showError("Wi-Fi", "Could not connect to " + ssid + ". Check the password and try again.");
    hal_.writeAbsolute(kWifiFile, ssid + "\n" + password + "\n");
    showError("Wi-Fi", "Connected and saved. Use Update lessons from the main menu.");
  })));
}

void App::runUpdate() {
  if (!hal_.sdReady()) return showError("No SD card", "Updates are saved on the SD card.");
  std::string creds;
  if (!hal_.readAbsolute(kWifiFile, creds)) {
    return showError("Update", "Set up Wi-Fi first: Tools > Wi-Fi setup.");
  }
  size_t nl = creds.find('\n');
  std::string ssid = creds.substr(0, nl);
  std::string password = nl == std::string::npos ? "" : creds.substr(nl + 1);
  if (!password.empty() && password.back() == '\n') password.pop_back();

  std::string url = kDefaultPackUrl, custom;
  if (hal_.readAbsolute(kUpdateUrlFile, custom)) {
    custom = custom.substr(0, custom.find_first_of("\r\n"));
    if (custom.compare(0, 8, "https://") == 0) url = custom;
  }

  ui::busy("Update lessons", "Connecting to " + ssid + "...");
  if (!hal_.wifiConnect(ssid, password, 15000)) {
    return showError("Update", "Could not connect to " + ssid + ". Run Tools > Wi-Fi setup again.");
  }
  ui::busy("Update lessons", "Downloading...");
  // TLS and the download need the RAM the frame buffer uses.
  ui::releaseBuffer();
  std::string data, err;
  bool ok = hal_.httpsGet(url, data, 128 * 1024, err);
  hal_.wifiOff();
  if (!ok) return showText("Update failed", err, ui::Tone::Bad);

  ui::busy("Update lessons", "Checking files...");
  Pack pack;
  if (!parsePack(data, pack, err)) return showText("Update failed", err, ui::Tone::Bad);
  data.clear();
  data.shrink_to_fit();

  std::string current;
  hal_.readAbsolute(kContentVersionFile, current);
  int written = 0;
  std::string skipped;
  for (const PackFile& f : pack.files) {
    // Only install files that load, so a bad file can't break the menus.
    bool valid;
    if (f.path.compare(0, 8, "lessons/") == 0) {
      Lesson l(&hal_);
      valid = l.load(f.data, err);
    } else {
      Scenario sc;
      valid = loadScenario(&hal_, f.data, sc, err);
    }
    std::string dest = std::string(kRootDir) + "/" + f.path;
    if (valid && hal_.writeAbsolute(dest.c_str(), f.data)) {
      ++written;
    } else {
      skipped += "\n- " + f.path;
    }
  }
  hal_.writeAbsolute(kContentVersionFile, pack.version + "\n");

  std::string text = "Installed " + std::to_string(written) + " files.\nVersion: " + pack.version.substr(0, 12);
  if (current.compare(0, pack.version.size(), pack.version) == 0) text += "\n(Already up to date.)";
  if (!skipped.empty()) text += "\n\nSkipped (did not load):" + skipped;
  showText("Update done", text, skipped.empty() ? ui::Tone::Good : ui::Tone::Game);
}


// HomeView ------------------------------------------------------------------

void drawIcon(LovyanGFX& g, int icon, int cx, int cy, int r, uint16_t c, uint16_t bg) {
  auto s = [r](float f) { return static_cast<int>(f * r + 0.5f); };
  switch (icon) {
    case kIconBook:  // an open book
      g.fillRoundRect(cx - r, cy - s(0.75f), r - 1, s(1.5f), 2, c);
      g.fillRoundRect(cx + 1, cy - s(0.75f), r - 1, s(1.5f), 2, c);
      if (r >= 12) {
        for (int i = 0; i < 3; ++i) {
          int y = cy - s(0.4f) + i * s(0.3f);
          g.fillRect(cx - s(0.8f), y, s(0.6f), 2, bg);
          g.fillRect(cx + s(0.2f), y, s(0.6f), 2, bg);
        }
      }
      break;
    case kIconShield:
      g.fillRoundRect(cx - s(0.8f), cy - r, s(1.6f), s(1.1f), 3, c);
      g.fillTriangle(cx - s(0.8f), cy, cx + s(0.8f), cy, cx, cy + r, c);
      if (r >= 12) {  // a check mark
        g.drawWideLine(cx - s(0.35f), cy - s(0.1f), cx - s(0.05f), cy + s(0.2f), 1.5f, bg);
        g.drawWideLine(cx - s(0.05f), cy + s(0.2f), cx + s(0.4f), cy - s(0.35f), 1.5f, bg);
      }
      break;
    case kIconTerminal:
      g.fillRoundRect(cx - r, cy - s(0.75f), 2 * r, s(1.5f), 3, c);
      g.fillRoundRect(cx - r + 2, cy - s(0.75f) + 2, 2 * r - 4, s(1.5f) - 4, 2, bg);
      g.drawWideLine(cx - s(0.6f), cy - s(0.3f), cx - s(0.25f), cy, r >= 12 ? 1.5f : 1.0f, c);
      g.drawWideLine(cx - s(0.25f), cy, cx - s(0.6f), cy + s(0.3f), r >= 12 ? 1.5f : 1.0f, c);
      g.fillRect(cx, cy + s(0.25f), s(0.5f), 2, c);
      break;
    case kIconGear:
      for (int i = 0; i < 4; ++i) {  // eight teeth as four bars through the middle
        float a = i * 0.785398f;
        int dx = static_cast<int>(cosf(a) * r * 0.9f), dy = static_cast<int>(sinf(a) * r * 0.9f);
        g.drawWideLine(cx - dx, cy - dy, cx + dx, cy + dy, r * 0.18f, c);
      }
      g.fillCircle(cx, cy, s(0.68f), c);
      g.fillCircle(cx, cy, s(0.28f), bg);
      break;
    case kIconDownload:
      g.fillRect(cx - s(0.15f), cy - r, s(0.3f), s(0.9f), c);
      g.fillTriangle(cx - s(0.55f), cy - s(0.15f), cx + s(0.55f), cy - s(0.15f), cx, cy + s(0.45f), c);
      g.fillRect(cx - r, cy + s(0.75f), 2 * r, s(0.2f) + 1, c);
      g.fillRect(cx - r, cy + s(0.4f), s(0.18f) + 1, s(0.5f), c);
      g.fillRect(cx + r - s(0.18f) - 1, cy + s(0.4f), s(0.18f) + 1, s(0.5f), c);
      break;
    default:  // info
      g.fillCircle(cx, cy, r, c);
      g.fillCircle(cx, cy, r - (r >= 12 ? 3 : 2), bg);
      g.fillCircle(cx, cy - s(0.45f), s(0.12f) + 1, c);
      g.fillRect(cx - s(0.1f), cy - s(0.15f), s(0.2f) + 1, s(0.65f), c);
      break;
  }
}

HomeView::HomeView(App& app, std::vector<Item> items, std::function<void(int)> onSelect)
    : app_(app), items_(std::move(items)), onSelect_(std::move(onSelect)) {}

void HomeView::draw() {
  const Item& it = items_[sel_];
  ui::frame("CardLab", it.tone);
  LovyanGFX& g = ui::gfx();
  int n = static_cast<int>(items_.size());
  uint16_t accent = ui::toneColor(it.tone);
  int cy = ui::kBodyY + 26;

  // Neighbours peek in from the sides so it reads as a row of cards.
  const Item& prev = items_[(sel_ + n - 1) % n];
  const Item& next = items_[(sel_ + 1) % n];
  drawIcon(g, prev.icon, 30, cy, 9, ui::kRule, ui::kBg);
  drawIcon(g, next.icon, ui::kW - 30, cy, 9, ui::kRule, ui::kBg);
  g.fillTriangle(52, cy, 57, cy - 5, 57, cy + 5, ui::kMuted);
  g.fillTriangle(ui::kW - 52, cy, ui::kW - 57, cy - 5, ui::kW - 57, cy + 5, ui::kMuted);

  g.fillRoundRect(ui::kW / 2 - 30, cy - 24, 60, 48, 8, ui::kSurface);
  drawIcon(g, it.icon, ui::kW / 2, cy, 16, accent, ui::kSurface);

  g.setTextDatum(top_center);
  g.setFont(ui::fontLarge());
  g.setTextColor(ui::kText);
  g.drawString(it.label.c_str(), ui::kW / 2, cy + 30);
  g.setFont(ui::fontBody());
  g.setTextColor(ui::kMuted);
  g.drawString(ui::fit(it.blurb, ui::fontBody(), ui::kW - 2 * ui::kPad).c_str(), ui::kW / 2, cy + 50);

  int dotsW = n * 8;
  for (int i = 0; i < n; ++i) {
    int x = ui::kW / 2 - dotsW / 2 + i * 8 + 4;
    if (i == sel_) g.fillRoundRect(x - 3, ui::kH - ui::kFootH - 6, 7, 3, 1, accent);
    else g.fillRect(x - 1, ui::kH - ui::kFootH - 6, 3, 3, ui::kRule);
  }
  ui::footer(", / browse|Enter open|1-6 jump");
}

void HomeView::key(const Key& k) {
  int n = static_cast<int>(items_.size());
  bool prev = k.type == Key::Left || k.type == Key::Up || (k.type == Key::Char && (k.c == ',' || k.c == ';'));
  bool next = k.type == Key::Right || k.type == Key::Down || (k.type == Key::Char && (k.c == '/' || k.c == '.'));
  if (prev) sel_ = (sel_ + n - 1) % n;
  else if (next) sel_ = (sel_ + 1) % n;
  else if (k.type == Key::Char && k.c >= '1' && k.c < '1' + n) {
    sel_ = k.c - '1';
    onSelect_(sel_);
  } else if (k.type == Key::Enter) {
    onSelect_(sel_);
  }
}

// InputView -----------------------------------------------------------------

InputView::InputView(App& app, std::string title, std::string prompt, bool masked,
                     std::function<void(const std::string&)> onDone)
    : app_(app), title_(std::move(title)), prompt_(std::move(prompt)), masked_(masked), onDone_(std::move(onDone)) {}

void InputView::draw() {
  ui::frame(title_);
  LovyanGFX& g = ui::gfx();
  g.setTextDatum(top_left);
  g.setFont(ui::fontBody());
  g.setTextColor(ui::kMuted);
  g.drawString(prompt_.c_str(), ui::kPad, ui::kBodyY + 8);

  int fx = ui::kPad, fy = ui::kBodyY + 28, fw = ui::kW - 2 * ui::kPad, fh = 26;
  g.fillRoundRect(fx, fy, fw, fh, 4, ui::kSurface);
  g.drawRoundRect(fx, fy, fw, fh, 4, ui::kTeal);
  std::string shown = masked_ ? std::string(text_.size(), '*') : text_;
  g.setFont(ui::fontLarge());
  // Keep the end of long text (where the cursor is) in view.
  int room = fw - 16;
  while (!shown.empty() && g.textWidth(shown.c_str()) > room) shown.erase(0, 1);
  g.setTextDatum(middle_left);
  g.setTextColor(ui::kText);
  g.drawString(shown.c_str(), fx + 6, fy + fh / 2);
  int cx = fx + 7 + g.textWidth(shown.c_str());
  g.fillRect(cx, fy + 6, 2, fh - 12, ui::kTeal);

  g.setFont(ui::fontSmall());
  g.setTextDatum(top_right);
  g.setTextColor(ui::kMuted);
  g.drawString((std::to_string(text_.size()) + " chars").c_str(), fx + fw, fy + fh + 5);
  ui::footer(masked_ ? "Enter OK|Tab show|fn+` back" : "Enter OK|Tab hide|fn+` back");
}

void InputView::key(const Key& k) {
  if (k.type == Key::Esc) return app_.pop();
  if (k.type == Key::Enter) return onDone_(text_);
  if (k.type == Key::Tab) masked_ = !masked_;
  else if (k.type == Key::Backspace && !text_.empty()) text_.pop_back();
  else if (k.type == Key::Char && text_.size() < 64) text_ += k.c;
}

// MenuView ------------------------------------------------------------------

namespace {
constexpr int kRowH = 20;
constexpr int kHudH = 18;
}  // namespace

MenuView::MenuView(App& app, std::string title, std::vector<std::string> items, std::function<void(int)> onSelect)
    : app_(app), title_(std::move(title)), items_(std::move(items)), onSelect_(std::move(onSelect)) {}

int MenuView::rows() const { return (ui::kBodyH - (hud ? kHudH : 0)) / kRowH; }

void MenuView::draw() {
  ui::frame(title_, tone);
  LovyanGFX& g = ui::gfx();
  int y0 = ui::kBodyY;
  if (hud) {
    hud(g, y0);
    y0 += kHudH;
  }
  uint16_t accent = ui::toneColor(tone);
  int n = static_cast<int>(items_.size());
  int right = ui::kW - ui::kPad;
  for (int row = 0; row < rows() && top_ + row < n; ++row) {
    int i = top_ + row;
    int y = y0 + row * kRowH;
    bool sel = i == sel_;
    if (sel) {
      g.fillRoundRect(4, y + 1, ui::kW - 12, kRowH - 2, 4, ui::kRaised);
      g.fillRoundRect(4, y + 4, 3, kRowH - 8, 1, accent);
    }
    std::string detail = i < static_cast<int>(details.size()) ? details[i] : "";
    int detailW = 0;
    if (!detail.empty()) {
      g.setFont(ui::fontSmall());
      g.setTextDatum(middle_right);
      g.setTextColor(sel ? accent : ui::kMuted);
      g.drawString(detail.c_str(), right - 4, y + kRowH / 2);
      detailW = g.textWidth(detail.c_str()) + 8;
    }
    g.setTextDatum(middle_left);
    g.setTextColor(sel ? ui::kText : ui::kMuted);
    std::string label = ui::fit(items_[i], ui::fontLarge(), right - 14 - detailW - 4);
    g.drawString(label.c_str(), 14, y + kRowH / 2);
  }
  ui::scrollbar(top_, rows(), n);
  ui::footer(onBack ? "; . move|Enter open" : "; . move|Enter open|` back");
}

void MenuView::key(const Key& k) {
  int n = static_cast<int>(items_.size());
  if (isBack(k)) {
    if (onBack) onBack();
    else app_.pop();
    return;
  }
  if (n == 0) return;
  if (k.type == Key::Up || (k.type == Key::Char && k.c == ';')) sel_ = (sel_ + n - 1) % n;
  else if (k.type == Key::Down || (k.type == Key::Char && k.c == '.')) sel_ = (sel_ + 1) % n;
  else if (k.type == Key::Enter) return onSelect_(sel_);
  if (sel_ < top_) top_ = sel_;
  if (sel_ >= top_ + rows()) top_ = sel_ - rows() + 1;
}

// TextView ------------------------------------------------------------------

namespace {
bool gLargeText = false;  // shared by every reader, toggled with Tab

const lgfx::IFont* readerFont() { return gLargeText ? ui::fontLarge() : ui::fontBody(); }
int readerLineH() { return gLargeText ? 19 : 15; }
int readerRows() { return ui::kBodyH / readerLineH(); }

// "ATTACK: ..." style lines get their label drawn in the accent colour.
size_t labelLength(const std::string& line) {
  size_t colon = line.find(':');
  if (colon < 2 || colon > 12 || colon == std::string::npos) return 0;
  for (size_t i = 0; i < colon; ++i) {
    if (!(std::isupper(static_cast<unsigned char>(line[i])) || line[i] == ' ')) return 0;
  }
  return colon + 1;
}
}  // namespace

TextView::TextView(App& app, std::string title, std::string text, std::function<void()> onDone)
    : app_(app), title_(std::move(title)), text_(std::move(text)), onDone_(std::move(onDone)) {}

void TextView::layout() {
  if (!lines_.empty() && large_ == gLargeText) return;
  large_ = gLargeText;
  lines_ = ui::wrap(text_, readerFont(), ui::kW - 2 * ui::kPad - 4);
  top_ = std::min(top_, std::max(0, static_cast<int>(lines_.size()) - readerRows()));
}

void TextView::draw() {
  layout();
  ui::frame(title_, tone);
  LovyanGFX& g = ui::gfx();
  g.setFont(readerFont());
  g.setTextDatum(top_left);
  uint16_t accent = ui::toneColor(tone);
  int n = static_cast<int>(lines_.size());
  for (int row = 0; row < readerRows() && top_ + row < n; ++row) {
    const std::string& l = lines_[top_ + row];
    int y = ui::kBodyY + 2 + row * readerLineH();
    size_t label = labelLength(l);
    int x = ui::kPad;
    if (label > 0) {
      g.setTextColor(accent);
      std::string head = l.substr(0, label);
      g.drawString(head.c_str(), x, y);
      x += g.textWidth(head.c_str());
    }
    g.setTextColor(ui::kText);
    g.drawString(l.c_str() + label, x, y);
  }
  ui::scrollbar(top_, readerRows(), n);
  bool more = top_ + readerRows() < n;
  ui::footer(std::string(more ? "; . scroll|" : "") + hintText + "|Tab size");
}

void TextView::key(const Key& k) {
  int maxTop = std::max(0, static_cast<int>(lines_.size()) - readerRows());
  if (k.type == Key::Up || (k.type == Key::Char && k.c == ';')) top_ = std::max(0, top_ - 1);
  else if (k.type == Key::Down || (k.type == Key::Char && k.c == '.')) top_ = std::min(maxTop, top_ + 1);
  else if (k.type == Key::Char && k.c == ' ') top_ = std::min(maxTop, top_ + readerRows() - 1);
  else if (k.type == Key::Tab) gLargeText = !gLargeText;
  else if (k.type == Key::Enter || isBack(k)) {
    if (onDone_) onDone_();
    else app_.pop();
  }
}

// EditorView ----------------------------------------------------------------

namespace {
constexpr int kGutterW = 20;
constexpr int kCodeRows = ui::kBodyH / ui::kMonoH;
constexpr int kCodeCols = (ui::kW - kGutterW - 6) / ui::kMonoW;
}  // namespace

EditorView::EditorView(App& app, std::string title, const std::string& code,
                       std::function<void(const std::string&)> onRun)
    : app_(app), title_(std::move(title)), onRun_(std::move(onRun)) {
  lines_ = ui::splitLines(code);
  row_ = static_cast<int>(lines_.size()) - 1;
  col_ = static_cast<int>(lines_[row_].size());
  top_ = std::max(0, row_ - kCodeRows + 1);
  left_ = std::max(0, col_ - kCodeCols + 1);
}

std::string EditorView::text() const {
  std::string out;
  for (size_t i = 0; i < lines_.size(); ++i) out += lines_[i] + (i + 1 < lines_.size() ? "\n" : "");
  return out;
}

void EditorView::draw() {
  ui::frame(title_);
  LovyanGFX& g = ui::gfx();
  g.fillRect(0, ui::kBodyY - 3, kGutterW, ui::kBodyH + 5, ui::kSurface);
  int x0 = kGutterW + 4;
  for (int r = 0; r < kCodeRows; ++r) {
    int i = top_ + r;
    if (i >= static_cast<int>(lines_.size())) break;
    int y = ui::kBodyY + r * ui::kMonoH;
    bool cur = i == row_;
    if (cur) g.fillRect(kGutterW, y, ui::kW - kGutterW, ui::kMonoH, ui::kRaised);

    g.setFont(ui::fontSmall());
    g.setTextDatum(middle_right);
    g.setTextColor(cur ? ui::kTeal : ui::kMuted);
    g.drawString(std::to_string(i + 1).c_str(), kGutterW - 3, y + ui::kMonoH / 2);

    g.setFont(ui::fontMono());
    g.setTextDatum(top_left);
    const std::string& s = lines_[i];
    // Comments are dimmed so the code stands out.
    size_t comment = s.find("--");
    std::string shown = static_cast<int>(s.size()) > left_ ? s.substr(left_, kCodeCols) : "";
    for (size_t c = 0; c < shown.size(); ++c) {
      bool dim = comment != std::string::npos && left_ + c >= comment;
      g.setTextColor(dim ? ui::kMuted : ui::kText);
      char ch[2] = {shown[c], 0};
      g.drawString(ch, x0 + static_cast<int>(c) * ui::kMonoW, y);
    }
    if (left_ > 0 && !s.empty()) g.fillRect(x0 - 3, y + 3, 1, ui::kMonoH - 6, ui::kAmber);
    if (cur) g.fillRect(x0 + (col_ - left_) * ui::kMonoW - 1, y + 1, 2, ui::kMonoH - 2, ui::kTeal);
  }
  ui::scrollbar(top_, kCodeRows, static_cast<int>(lines_.size()));
  ui::footer("Tab run|fn+;.,/ move|fn+` exit");
}

void EditorView::key(const Key& k) {
  std::string& cur = lines_[row_];
  switch (k.type) {
    case Key::Esc: app_.pop(); return;
    case Key::Tab: onRun_(text()); return;
    case Key::Enter: {
      // Keep the current line's indentation.
      size_t indent = cur.find_first_not_of(' ');
      if (indent == std::string::npos) indent = cur.size();
      std::string rest = std::string(indent, ' ') + cur.substr(col_);
      cur.erase(col_);
      lines_.insert(lines_.begin() + row_ + 1, rest);
      ++row_;
      col_ = static_cast<int>(indent);
      break;
    }
    case Key::Backspace:
      if (col_ > 0) {
        cur.erase(col_ - 1, 1);
        --col_;
      } else if (row_ > 0) {
        col_ = static_cast<int>(lines_[row_ - 1].size());
        lines_[row_ - 1] += cur;
        lines_.erase(lines_.begin() + row_);
        --row_;
      }
      break;
    case Key::Up: if (row_ > 0) --row_; break;
    case Key::Down: if (row_ + 1 < static_cast<int>(lines_.size())) ++row_; break;
    case Key::Left: if (col_ > 0) --col_; break;
    case Key::Right: if (col_ < static_cast<int>(cur.size())) ++col_; break;
    case Key::Char:
      cur.insert(cur.begin() + col_, k.c);
      ++col_;
      break;
    default: break;
  }
  col_ = std::min(col_, static_cast<int>(lines_[row_].size()));
  if (row_ < top_) top_ = row_;
  if (row_ >= top_ + kCodeRows) top_ = row_ - kCodeRows + 1;
  if (col_ < left_) left_ = col_;
  if (col_ >= left_ + kCodeCols) left_ = col_ - kCodeCols + 1;
}

// ConsoleView ---------------------------------------------------------------

namespace {
constexpr int kConsoleCols = (ui::kW - 2 * 6) / ui::kMonoW;
}

ConsoleView::ConsoleView(App& app) : app_(app), box_(&app.hal(), kPermAll) {
  lines_.push_back("Lua 5.4. Try: print(1+1)");
  lines_.push_back("Modules: display gpio imu i2c");
  lines_.push_back("         wifi files sys");
}

void ConsoleView::draw() {
  ui::frame("Lua console");
  LovyanGFX& g = ui::gfx();
  g.setFont(ui::fontMono());
  g.setTextDatum(top_left);
  int body = kCodeRows - 1;  // the last row is the prompt
  int n = static_cast<int>(lines_.size());
  int first = std::max(0, n - body);
  for (int r = 0; r < body && first + r < n; ++r) {
    const std::string& l = lines_[first + r];
    char tag = l.empty() ? ' ' : l[0];
    g.setTextColor(tag == '>' ? ui::kTeal : tag == '!' ? ui::kRed : ui::kText);
    g.drawString(tag == '!' ? l.c_str() + 1 : l.c_str(), 6, ui::kBodyY + r * ui::kMonoH);
  }
  int y = ui::kBodyY + body * ui::kMonoH;
  g.fillRect(0, y - 1, ui::kW, ui::kMonoH + 2, ui::kSurface);
  std::string prompt = input_;
  int room = kConsoleCols - 3;
  if (static_cast<int>(prompt.size()) > room) prompt = prompt.substr(prompt.size() - room);
  g.setTextColor(ui::kTeal);
  g.drawString(">", 6, y);
  g.setTextColor(ui::kText);
  g.drawString(prompt.c_str(), 6 + 2 * ui::kMonoW, y);
  g.fillRect(6 + (2 + static_cast<int>(prompt.size())) * ui::kMonoW, y + 1, 2, ui::kMonoH - 2, ui::kTeal);
  ui::footer("Enter run|fn+` exit");
}

void ConsoleView::key(const Key& k) {
  if (k.type == Key::Esc) return app_.pop();
  if (k.type == Key::Backspace && !input_.empty()) input_.pop_back();
  else if (k.type == Key::Char) input_ += k.c;
  else if (k.type == Key::Enter && !input_.empty()) {
    for (const std::string& l : ui::wrap("> " + input_, ui::fontMono(), kConsoleCols * ui::kMonoW)) lines_.push_back(l);
    RunResult r = box_.eval(input_);
    std::string text = r.ok ? r.output : r.error;
    for (const std::string& l : ui::wrap(text, ui::fontMono(), kConsoleCols * ui::kMonoW)) {
      if (!l.empty()) lines_.push_back(r.ok ? l : "!" + l);
    }
    if (lines_.size() > 64) lines_.erase(lines_.begin(), lines_.begin() + (lines_.size() - 64));
    input_.clear();
  }
}

}  // namespace cardlab
