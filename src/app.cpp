#include "app.h"

#include "pack.h"

#include <M5Cardputer.h>

#include <algorithm>
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
    rules->hintText = "Enter: I agree";
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

void App::showMainMenu() {
  std::vector<std::string> items = {"Lessons", "Defend the Network", "Lua console", "Tools", "Update lessons",
                                    "Rules and about"};
  auto menu = std::unique_ptr<MenuView>(new MenuView(*this, "CardLab", items, [this](int i) {
    switch (i) {
      case 0: openLessons(); break;
      case 1: openScenarios(); break;
      case 2: push(std::unique_ptr<View>(new ConsoleView(*this))); break;
      case 3: openTools(); break;
      case 4: runUpdate(); break;
      default: showError("Rules and about", kRules); break;
    }
  }));
  menu->onBack = [] {};  // the main menu is the bottom of the stack
  push(std::move(menu));
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
  std::vector<std::string> items;
  for (const std::string& p : *paths) {
    std::string src, err, label = fileStem(p);
    Lesson l(&hal_);
    if (hal_.readAbsolute(p.c_str(), src) && l.load(src, err)) {
      const LessonRecord* r = progress_.lesson(l.id());
      label = std::string(r && r->taskPassed ? "* " : "  ") + l.title();
    }
    items.push_back(label);
  }
  push(std::unique_ptr<View>(new MenuView(*this, "Lessons", items, [this, paths](int i) {
    openLesson(paths->at(i));
  })));
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
    ui::clear();
    ui::title("Running...");
    GradeResult g = lesson->grade(code);
    if (lesson->permissions() & kPermDisplay) {
      ui::hint("Any key: see result");
      ui::waitKey();
    }
    progress_.recordTask(lesson->id(), g.passed);
    saveProgress();

    std::string text = (g.passed ? "PASS: " : "NOT YET: ") + g.feedback;
    if (!g.run.output.empty()) text += "\n\nOutput:\n" + g.run.output;
    if (!g.run.ok) text += "\n\nError:\n" + g.run.error;
    bool passed = g.passed;
    push(std::unique_ptr<View>(new TextView(*this, "Result", text, [this, lesson, passed] {
      if (!passed) return pop();  // back to the editor
      pop(2);
      startQuiz(lesson);
    })));
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
    push(std::unique_ptr<View>(new TextView(*this, "Lesson done", text)));
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
      push(std::unique_ptr<View>(new TextView(*this, right ? "Correct" : "Answer", text, [this, lesson, index, score] {
        pop(2);
        askQuestion(lesson, index + 1, score);
      })));
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
  std::vector<std::string> items;
  for (const std::string& p : *paths) {
    std::string src, err, label = fileStem(p);
    Scenario s;
    if (hal_.readAbsolute(p.c_str(), src) && loadScenario(&hal_, src, s, err)) {
      int best = progress_.bestScenarioScore(s.id);
      label = s.title + (best >= 0 ? " (" + std::to_string(best) + ")" : "");
    }
    items.push_back(label);
  }
  push(std::unique_ptr<View>(new MenuView(*this, "Defend the Network", items, [this, paths](int i) {
    startGame(paths->at(i));
  })));
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
  push(std::unique_ptr<View>(new TextView(*this, game->scenario().title, text, [this, game] {
    pop();
    showRound(game);
  })));
}

void App::showRound(std::shared_ptr<Game> game) {
  if (game->finished()) {
    progress_.recordScenario(game->scenario().id, game->score());
    saveProgress();
    std::string text = "Score: " + std::to_string(game->score()) + "\nUptime: " + std::to_string(game->uptime()) +
                       "%\nData safe: " + std::to_string(game->data()) + "%";
    push(std::unique_ptr<View>(new TextView(*this, "Debrief", text)));
    return;
  }
  std::string title = "Round " + std::to_string(game->roundIndex() + 1) + "/" +
                      std::to_string(game->scenario().rounds.size());
  push(std::unique_ptr<View>(new TextView(*this, title, "SYMPTOM: " + game->round().symptom, [this, game] {
    pop();
    pushRoundMenu(game);
  })));
}

void App::pushRoundMenu(std::shared_ptr<Game> game) {
  const Round& r = game->round();
  std::vector<std::string> items = {"Re-read symptom"};
  for (size_t i = 0; i < r.evidence.size(); ++i) {
    items.push_back(game->seen(i) ? "[seen] " + r.evidence[i].name
                                  : "(" + std::to_string(r.evidence[i].cost) + ") " + r.evidence[i].name);
  }
  items.push_back("> Respond");
  std::string title = "AP " + std::to_string(game->pointsLeft()) + "  Up " + std::to_string(game->uptime()) +
                      "%  Data " + std::to_string(game->data()) + "%";

  push(std::unique_ptr<View>(new MenuView(*this, title, items, [this, game](int i) {
    const Round& r = game->round();
    if (i == 0) return showError("Symptom", r.symptom);
    size_t e = static_cast<size_t>(i - 1);
    if (e < r.evidence.size()) {
      if (!game->investigate(e)) return showError("No points", "Not enough action points. Time to respond.");
      push(std::unique_ptr<View>(new TextView(*this, r.evidence[e].name, r.evidence[e].text, [this, game] {
        pop(2);  // evidence and the stale round menu
        pushRoundMenu(game);
      })));
      return;
    }
    std::vector<std::string> names;
    for (const Defense& d : r.defenses) names.push_back(d.name);
    push(std::unique_ptr<View>(new MenuView(*this, "Respond", names, [this, game](int j) {
      Outcome o = game->defend(static_cast<size_t>(j));
      const Round& done = *game->lastRound();
      const Reveal& rv = done.reveal;
      std::string text = std::string(outcomeName(o)) + "\n" + game->lastDefense()->text + "\n\nATTACK: " + rv.attack +
                         "\nTARGET: " + rv.target + "\nCIA: " + rv.cia;
      if (!rv.technique.empty()) text += "\nREF: " + rv.technique;
      if (!rv.lesson.empty()) text += "\nSTUDY: lesson " + rv.lesson;
      push(std::unique_ptr<View>(new TextView(*this, "Reveal", text, [this, game] {
        pop(3);  // reveal, respond menu, round menu
        showRound(game);
      })));
    })));
  })));
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
      ui::clear();
      ui::title("Scanning...");
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
  ui::clear();
  ui::title("Scanning...");
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
    ui::clear();
    ui::title("Connecting...");
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

  ui::clear();
  ui::title("Connecting...");
  if (!hal_.wifiConnect(ssid, password, 15000)) {
    return showError("Update", "Could not connect to " + ssid + ". Run Tools > Wi-Fi setup again.");
  }
  ui::title("Downloading...");
  std::string data, err;
  bool ok = hal_.httpsGet(url, data, 128 * 1024, err);
  hal_.wifiOff();
  if (!ok) return showError("Update failed", err);

  ui::title("Checking...");
  Pack pack;
  if (!parsePack(data, pack, err)) return showError("Update failed", err);
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
  showError("Update done", text);
}

// InputView -----------------------------------------------------------------

InputView::InputView(App& app, std::string title, std::string prompt, bool masked,
                     std::function<void(const std::string&)> onDone)
    : app_(app), title_(std::move(title)), prompt_(std::move(prompt)), masked_(masked), onDone_(std::move(onDone)) {}

void InputView::draw() {
  ui::clear();
  ui::title(title_);
  ui::line(0, prompt_);
  std::string shown = masked_ ? std::string(text_.size(), '*') : text_;
  if (static_cast<int>(shown.size()) > ui::kCols - 1) shown = shown.substr(shown.size() - (ui::kCols - 1));
  ui::line(1, shown + "_", true);
  ui::hint("Enter OK  Tab show  fn+` back");
}

void InputView::key(const Key& k) {
  if (k.type == Key::Esc) return app_.pop();
  if (k.type == Key::Enter) return onDone_(text_);
  if (k.type == Key::Tab) masked_ = !masked_;
  else if (k.type == Key::Backspace && !text_.empty()) text_.pop_back();
  else if (k.type == Key::Char && text_.size() < 64) text_ += k.c;
}

// MenuView ------------------------------------------------------------------

MenuView::MenuView(App& app, std::string title, std::vector<std::string> items, std::function<void(int)> onSelect)
    : app_(app), title_(std::move(title)), items_(std::move(items)), onSelect_(std::move(onSelect)) {}

void MenuView::draw() {
  ui::clear();
  ui::title(title_);
  for (int row = 0; row < ui::kBodyRows; ++row) {
    int i = top_ + row;
    if (i >= static_cast<int>(items_.size())) break;
    ui::line(row, items_[i], i == sel_);
  }
  ui::hint("; . move  Enter  ` back");
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
  if (sel_ >= top_ + ui::kBodyRows) top_ = sel_ - ui::kBodyRows + 1;
}

// TextView ------------------------------------------------------------------

TextView::TextView(App& app, std::string title, const std::string& text, std::function<void()> onDone)
    : app_(app), title_(std::move(title)), lines_(ui::wrap(text)), onDone_(std::move(onDone)) {}

void TextView::draw() {
  ui::clear();
  ui::title(title_);
  for (int row = 0; row < ui::kBodyRows; ++row) {
    int i = top_ + row;
    if (i >= static_cast<int>(lines_.size())) break;
    ui::line(row, lines_[i]);
  }
  bool more = top_ + ui::kBodyRows < static_cast<int>(lines_.size());
  ui::hint(more ? "; . scroll (more below)" : hintText);
}

void TextView::key(const Key& k) {
  int maxTop = std::max(0, static_cast<int>(lines_.size()) - ui::kBodyRows);
  if (k.type == Key::Up || (k.type == Key::Char && k.c == ';')) top_ = std::max(0, top_ - 1);
  else if (k.type == Key::Down || (k.type == Key::Char && k.c == '.')) top_ = std::min(maxTop, top_ + 1);
  else if (k.type == Key::Char && k.c == ' ') top_ = std::min(maxTop, top_ + ui::kBodyRows);
  else if (k.type == Key::Enter || isBack(k)) {
    if (onDone_) onDone_();
    else app_.pop();
  }
}

// EditorView ----------------------------------------------------------------

EditorView::EditorView(App& app, std::string title, const std::string& code,
                       std::function<void(const std::string&)> onRun)
    : app_(app), title_(std::move(title)), onRun_(std::move(onRun)) {
  lines_ = ui::wrap(code, 1 << 20);  // split on newlines only
  if (lines_.empty()) lines_.push_back("");
  row_ = static_cast<int>(lines_.size()) - 1;
  col_ = static_cast<int>(lines_[row_].size());
}

std::string EditorView::text() const {
  std::string out;
  for (size_t i = 0; i < lines_.size(); ++i) out += lines_[i] + (i + 1 < lines_.size() ? "\n" : "");
  return out;
}

void EditorView::draw() {
  ui::clear();
  ui::title(title_);
  for (int r = 0; r < ui::kBodyRows; ++r) {
    int i = top_ + r;
    if (i >= static_cast<int>(lines_.size())) break;
    std::string s = lines_[i];
    // Scroll long lines so the cursor stays visible, and mark the cursor.
    int start = 0;
    if (i == row_) {
      s.insert(col_, "|");
      start = std::max(0, col_ - (ui::kCols - 2));
    }
    ui::line(r, s.substr(start));
  }
  ui::hint("Tab run  fn+arrows  fn+` exit");
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
  if (row_ >= top_ + ui::kBodyRows) top_ = row_ - ui::kBodyRows + 1;
}

// ConsoleView ---------------------------------------------------------------

ConsoleView::ConsoleView(App& app) : app_(app), box_(&app.hal(), kPermAll) {
  lines_.push_back("Lua 5.4. Try: print(1+1)");
  lines_.push_back("Modules: display gpio imu i2c");
  lines_.push_back("wifi files sys");
}

void ConsoleView::draw() {
  ui::clear();
  ui::title("Lua console");
  int body = ui::kBodyRows - 1;  // last body row is the prompt
  int first = std::max(0, static_cast<int>(lines_.size()) - body);
  for (int r = 0; r < body && first + r < static_cast<int>(lines_.size()); ++r) ui::line(r, lines_[first + r]);
  std::string prompt = "> " + input_;
  if (static_cast<int>(prompt.size()) > ui::kCols - 1) prompt = prompt.substr(prompt.size() - (ui::kCols - 1));
  ui::line(body, prompt + "_", true);
  ui::hint("Enter run  fn+` exit");
}

void ConsoleView::key(const Key& k) {
  if (k.type == Key::Esc) return app_.pop();
  if (k.type == Key::Backspace && !input_.empty()) input_.pop_back();
  else if (k.type == Key::Char) input_ += k.c;
  else if (k.type == Key::Enter && !input_.empty()) {
    lines_.push_back("> " + input_);
    RunResult r = box_.eval(input_);
    std::string text = r.ok ? r.output : r.error;
    for (const std::string& l : ui::wrap(text)) {
      if (!l.empty()) lines_.push_back(l);
    }
    if (lines_.size() > 64) lines_.erase(lines_.begin(), lines_.begin() + (lines_.size() - 64));
    input_.clear();
  }
}

}  // namespace cardlab
