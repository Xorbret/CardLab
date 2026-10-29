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
  void showError(const std::string& title, const std::string& message);

  CardputerHal hal_;
  Progress progress_;
  std::vector<std::unique_ptr<View>> stack_;
  std::vector<std::unique_ptr<View>> pendingPush_;
  int pendingPop_ = 0;
  bool dirty_ = true;
};

// A scrollable list. Enter calls onSelect(index); Esc pops unless onBack
// is set.
class MenuView : public View {
 public:
  MenuView(App& app, std::string title, std::vector<std::string> items, std::function<void(int)> onSelect);
  void draw() override;
  void key(const Key& k) override;
  std::function<void()> onBack;

 private:
  App& app_;
  std::string title_;
  std::vector<std::string> items_;
  std::function<void(int)> onSelect_;
  int sel_ = 0, top_ = 0;
};

// Read-only text with scrolling. Enter or Esc calls onDone (default: pop).
class TextView : public View {
 public:
  TextView(App& app, std::string title, const std::string& text, std::function<void()> onDone = nullptr);
  void draw() override;
  void key(const Key& k) override;
  std::string hintText = "; . scroll  Enter: next";

 private:
  App& app_;
  std::string title_;
  std::vector<std::string> lines_;
  std::function<void()> onDone_;
  int top_ = 0;
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
  int row_ = 0, col_ = 0, top_ = 0;
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
  std::vector<std::string> lines_;
};

}  // namespace cardlab
