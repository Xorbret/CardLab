// Screen stack and the views that make up the CardLab UI.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cardputer_hal.h"
#include "game.h"
#include "lesson.h"
#include "progress.h"
#include "ui.h"

namespace cardlab {

class App;

class View {
 public:
  virtual ~View() = default;
  virtual void draw() = 0;
  virtual void key(const Key& k) = 0;
};

class App {
 public:
  void begin();
  void loop();

  // Stack changes are applied after the current key handler returns, so a
  // view may safely pop itself.
  void push(std::unique_ptr<View> v);
  void pop(int count = 1);

  CardputerHal& hal() { return hal_; }
  Progress& progress() { return progress_; }
  void saveProgress();

 private:
  void showMainMenu();
  void openLessons();
  void openLesson(const std::string& path);
  void startTask(std::shared_ptr<Lesson> lesson);
  void startQuiz(std::shared_ptr<Lesson> lesson);
  void askQuestion(std::shared_ptr<Lesson> lesson, size_t index, std::shared_ptr<int> score);
  void openScenarios();
  void startGame(const std::string& path);
  void showRound(std::shared_ptr<Game> game);
  void pushRoundMenu(std::shared_ptr<Game> game);
  void openTools();
  void setupWifi();
  void askWifiPassword(const std::string& ssid);
  void runUpdate();
  void showError(const std::string& title, const std::string& message);
  void showText(const std::string& title, const std::string& text, ui::Tone tone);

  CardputerHal hal_;
  Progress progress_;
  std::vector<std::unique_ptr<View>> stack_;
  std::vector<std::unique_ptr<View>> pendingPush_;
  int pendingPop_ = 0;
  bool dirty_ = true;
};

// The home screen: one large card per section, moved through with , and /.
class HomeView : public View {
 public:
  struct Item {
    int icon;
    std::string label, blurb;
    ui::Tone tone;
  };
  HomeView(App& app, std::vector<Item> items, std::function<void(int)> onSelect);
  void draw() override;
  void key(const Key& k) override;

 private:
  App& app_;
  std::vector<Item> items_;
  std::function<void(int)> onSelect_;
  int sel_ = 0;
};

// Icons for the home screen, drawn from shapes so they cost no flash.
enum Icon { kIconBook, kIconShield, kIconTerminal, kIconGear, kIconDownload, kIconInfo };
void drawIcon(LovyanGFX& g, int icon, int cx, int cy, int r, uint16_t color, uint16_t bg);

// A scrollable list. Enter calls onSelect(index); Esc pops unless onBack
// is set. `details` adds short right-aligned notes (a score, "done").
class MenuView : public View {
 public:
  MenuView(App& app, std::string title, std::vector<std::string> items, std::function<void(int)> onSelect);
  void draw() override;
  void key(const Key& k) override;
  std::function<void()> onBack;
  std::vector<std::string> details;
  ui::Tone tone = ui::Tone::Normal;
  // Optional strip drawn above the list (the game's status readout).
  std::function<void(LovyanGFX&, int y)> hud;

 private:
  int rows() const;
  App& app_;
  std::string title_;
  std::vector<std::string> items_;
  std::function<void(int)> onSelect_;
  int sel_ = 0, top_ = 0;
};

// Read-only text with scrolling. Enter or Esc calls onDone (default: pop).
// Tab switches between normal and large text for every reader.
class TextView : public View {
 public:
  TextView(App& app, std::string title, std::string text, std::function<void()> onDone = nullptr);
  void draw() override;
  void key(const Key& k) override;
  std::string hintText = "Enter next";
  ui::Tone tone = ui::Tone::Normal;

 private:
  void layout();
  App& app_;
  std::string title_, text_;
  std::vector<std::string> lines_;
  std::function<void()> onDone_;
  int top_ = 0;
  bool large_ = false;  // the layout that lines_ was wrapped for
};

// Line-based code editor. Tab calls onRun(code); Esc pops.
class EditorView : public View {
 public:
  EditorView(App& app, std::string title, const std::string& code, std::function<void(const std::string&)> onRun);
  void draw() override;
  void key(const Key& k) override;

 private:
  std::string text() const;
  App& app_;
  std::string title_;
  std::vector<std::string> lines_;
  std::function<void(const std::string&)> onRun_;
  int row_ = 0, col_ = 0, top_ = 0, left_ = 0;
};

// Single-line text entry. Enter calls onDone(text); Esc pops. Masked input
// shows dots; Tab toggles showing the text.
class InputView : public View {
 public:
  InputView(App& app, std::string title, std::string prompt, bool masked,
            std::function<void(const std::string&)> onDone);
  void draw() override;
  void key(const Key& k) override;

 private:
  App& app_;
  std::string title_, prompt_, text_;
  bool masked_;
  std::function<void(const std::string&)> onDone_;
};

// One-line Lua prompt with scrolling output. Enter runs the line.
class ConsoleView : public View {
 public:
  explicit ConsoleView(App& app);
  void draw() override;
  void key(const Key& k) override;

 private:
  App& app_;
  Sandbox box_;
  std::string input_;
  // Output lines; a leading '>' marks an echoed command, '!' an error.
  std::vector<std::string> lines_;
};

}  // namespace cardlab
