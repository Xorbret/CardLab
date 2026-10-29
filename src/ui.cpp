#include "ui.h"

#include <M5Cardputer.h>

namespace cardlab {
namespace ui {

namespace {

constexpr int kLineH = 16;
constexpr uint16_t kAccent = 0x2D7F;  // blue
auto& lcd() { return M5Cardputer.Display; }

std::string fit(const std::string& s) {
  return s.size() > static_cast<size_t>(kCols) ? s.substr(0, kCols) : s;
}

}  // namespace

void begin() {
  lcd().setRotation(1);
  lcd().setFont(&fonts::AsciiFont8x16);
  lcd().setTextSize(1);
  lcd().setTextWrap(false);
  clear();
}

Key poll() {
  Key k;
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return k;
  const auto& s = M5Cardputer.Keyboard.keysState();
  char c = s.word.empty() ? 0 : s.word.front();
  if (s.fn) {
    // M5Cardputer 1.1.1 reports fn combos as fn + the plain character.
    switch (c) {
      case ';': k.type = Key::Up; break;
      case '.': k.type = Key::Down; break;
      case ',': k.type = Key::Left; break;
      case '/': k.type = Key::Right; break;
      case '`': k.type = Key::Esc; break;
      default: break;
    }
    if (k.type == Key::None && s.del) k.type = Key::Backspace;
    return k;
  }
  if (s.enter) k.type = Key::Enter;
  else if (s.del) k.type = Key::Backspace;  // the backspace key
  else if (s.tab) k.type = Key::Tab;
  else if (c != 0) {
    k.type = Key::Char;
    k.c = c;
  }
  return k;
}

void waitKey() {
  for (;;) {
    M5Cardputer.update();
    if (poll().type != Key::None) return;
    delay(10);
  }
}

std::vector<std::string> wrap(const std::string& text, int cols) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= text.size()) {
    size_t nl = text.find('\n', start);
    std::string para = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    while (static_cast<int>(para.size()) > cols) {
      size_t cut = para.rfind(' ', cols);
      if (cut == std::string::npos || cut == 0) cut = cols;
      out.push_back(para.substr(0, cut));
      para.erase(0, para[cut] == ' ' ? cut + 1 : cut);
    }
    out.push_back(para);
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return out;
}

void clear() { lcd().fillScreen(TFT_BLACK); }

void title(const std::string& text) {
  lcd().fillRect(0, 0, lcd().width(), kLineH, kAccent);
  lcd().setTextColor(TFT_WHITE, kAccent);
  lcd().drawString(fit(text).c_str(), 0, 0);
}

void hint(const std::string& text) {
  int y = (kRows - 1) * kLineH;
  lcd().fillRect(0, y, lcd().width(), lcd().height() - y, TFT_DARKGREY);
  lcd().setTextColor(TFT_BLACK, TFT_DARKGREY);
  lcd().drawString(fit(text).c_str(), 0, y);
}

void line(int row, const std::string& text, bool selected) {
  int y = (row + 1) * kLineH;
  uint16_t bg = selected ? TFT_WHITE : TFT_BLACK;
  lcd().fillRect(0, y, lcd().width(), kLineH, bg);
  lcd().setTextColor(selected ? TFT_BLACK : TFT_WHITE, bg);
  lcd().drawString(fit(text).c_str(), 0, y);
}

}  // namespace ui
}  // namespace cardlab
