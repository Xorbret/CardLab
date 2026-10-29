// Keyboard input and text drawing for the 240x135 screen.
#pragma once

#include <string>
#include <vector>

namespace cardlab {

struct Key {
  enum Type { None, Char, Enter, Backspace, Esc, Tab, Up, Down, Left, Right };
  Type type = None;
  char c = 0;
};

namespace ui {

constexpr int kCols = 30;  // 8x16 font on 240 px
constexpr int kRows = 8;   // 135 px / 16
constexpr int kBodyRows = kRows - 2;  // minus title bar and hint bar

void begin();

// Returns one event per new key press. Arrows are fn + ; . , /
// and Esc is fn + `.
Key poll();

// Blocks until any key is pressed (used after learner code draws).
void waitKey();

std::vector<std::string> wrap(const std::string& text, int cols = kCols);

void clear();
void title(const std::string& text);
void hint(const std::string& text);  // bottom row
// Body line 0..kBodyRows-1; `selected` draws it highlighted.
void line(int row, const std::string& text, bool selected = false);

}  // namespace ui
}  // namespace cardlab
