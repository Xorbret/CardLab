#include "ui.h"

#include <M5Cardputer.h>

#include <algorithm>

namespace cardlab {
namespace ui {

namespace {

auto& lcd() { return M5Cardputer.Display; }

M5Canvas canvas(&M5Cardputer.Display);
bool haveBuffer = false;

// Battery reads go through the ADC, so refresh them at most every 10 s.
int batteryPercent() {
  static int level = -1;
  static uint32_t readAt = 0;
  uint32_t now = millis();
  if (readAt == 0 || now - readAt > 10000) {
    level = M5.Power.getBatteryLevel();
    readAt = now;
  }
  return level;
}

bool ensureBuffer() {
  if (!haveBuffer) {
    canvas.setColorDepth(16);
    haveBuffer = canvas.createSprite(kW, kH) != nullptr;
  }
  return haveBuffer;
}

void drawBattery(LovyanGFX& g, int right) {
  int level = batteryPercent();
  if (level < 0 || level > 100) return;
  // A small cell outline with a fill proportional to charge.
  int x = right - 14, y = 4;
  uint16_t c = level <= 15 ? kRed : kMuted;
  g.drawRoundRect(x, y, 12, 8, 2, c);
  g.fillRect(x + 12, y + 2, 2, 4, c);
  g.fillRect(x + 2, y + 2, (8 * level + 50) / 100, 4, level <= 15 ? kRed : kText);
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", level);
  g.setFont(fontSmall());
  g.setTextDatum(middle_right);
  g.setTextColor(kMuted);
  g.drawString(buf, x - 3, kBarH / 2);
}

}  // namespace

uint16_t toneColor(Tone t) {
  switch (t) {
    case Tone::Game: return kAmber;
    case Tone::Good: return kGreen;
    case Tone::Bad: return kRed;
    default: return kTeal;
  }
}

const lgfx::IFont* fontBody() { return &fonts::DejaVu12; }
const lgfx::IFont* fontLarge() { return &fonts::DejaVu18; }
const lgfx::IFont* fontSmall() { return &fonts::DejaVu9; }
const lgfx::IFont* fontMono() { return &fonts::AsciiFont8x16; }

void begin() {
  lcd().setRotation(1);
  lcd().setBrightness(110);
  lcd().fillScreen(kBg);
  ensureBuffer();
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

LovyanGFX& gfx() {
  if (haveBuffer) return canvas;
  return lcd();
}

void frame(const std::string& title, Tone tone) {
  ensureBuffer();
  LovyanGFX& g = gfx();
  g.fillScreen(kBg);
  g.fillRect(0, 0, kW, kBarH, kSurface);
  g.fillRect(0, kBarH - 1, kW, 1, toneColor(tone));
  g.setTextDatum(middle_left);
  g.setFont(fontBody());
  g.setTextColor(kText);
  // Leave room for the battery readout on the right.
  g.drawString(fit(title, fontBody(), kW - kPad - 52).c_str(), kPad, kBarH / 2);
  drawBattery(g, kW - 4);
}

void footer(const std::string& hints) {
  LovyanGFX& g = gfx();
  int y = kH - kFootH;
  g.fillRect(0, y, kW, kFootH, kSurface);
  g.setFont(fontSmall());
  g.setTextDatum(middle_left);
  int x = 4, cy = y + kFootH / 2;
  size_t start = 0;
  while (start < hints.size() && x < kW) {
    size_t bar = hints.find('|', start);
    std::string chip = hints.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
    start = bar == std::string::npos ? hints.size() : bar + 1;
    size_t sp = chip.find(' ');
    std::string key = chip.substr(0, sp);
    std::string action = sp == std::string::npos ? "" : chip.substr(sp + 1);
    int kw = g.textWidth(key.c_str()) + 6;
    g.fillRoundRect(x, y + 2, kw, kFootH - 4, 2, kRaised);
    g.setTextColor(kText);
    g.drawString(key.c_str(), x + 3, cy);
    x += kw + 3;
    if (!action.empty()) {
      g.setTextColor(kMuted);
      g.drawString(action.c_str(), x, cy);
      x += g.textWidth(action.c_str());
    }
    x += 8;
  }
}

void present() {
  if (haveBuffer) canvas.pushSprite(0, 0);
}

void busy(const std::string& title, const std::string& message) {
  frame(title);
  LovyanGFX& g = gfx();
  int cy = kBodyY + kBodyH / 2;
  // Three dots as a "working" mark above the message.
  for (int i = 0; i < 3; ++i) g.fillCircle(kW / 2 - 12 + i * 12, cy - 14, 3, i == 1 ? kTeal : kRule);
  g.setFont(fontBody());
  g.setTextDatum(middle_center);
  g.setTextColor(kText);
  g.drawString(fit(message, fontBody(), kW - 2 * kPad).c_str(), kW / 2, cy + 6);
  present();
}

void releaseBuffer() {
  if (!haveBuffer) return;
  present();  // leave the last frame on screen
  canvas.deleteSprite();
  haveBuffer = false;
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> out;
  size_t start = 0;
  for (;;) {
    size_t nl = text.find('\n', start);
    out.push_back(text.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return out;
}

std::vector<std::string> wrap(const std::string& text, const lgfx::IFont* font, int width) {
  LovyanGFX& g = gfx();
  g.setFont(font);
  std::vector<std::string> out;
  for (std::string para : splitLines(text)) {
    while (g.textWidth(para.c_str()) > width) {
      // Longest prefix that fits, then back up to the last space.
      size_t n = 1;
      while (n < para.size() && g.textWidth(para.substr(0, n + 1).c_str()) <= width) ++n;
      size_t cut = para.rfind(' ', n);
      if (cut == std::string::npos || cut == 0) cut = n;
      out.push_back(para.substr(0, cut));
      para.erase(0, para[cut] == ' ' ? cut + 1 : cut);
    }
    out.push_back(para);
  }
  return out;
}

std::string fit(const std::string& text, const lgfx::IFont* font, int width) {
  LovyanGFX& g = gfx();
  g.setFont(font);
  if (g.textWidth(text.c_str()) <= width) return text;
  std::string s = text;
  while (!s.empty() && g.textWidth((s + "..").c_str()) > width) s.pop_back();
  return s + "..";
}

void scrollbar(int first, int visible, int total) {
  if (total <= visible) return;
  LovyanGFX& g = gfx();
  int x = kW - 3, h = kBodyH;
  g.fillRect(x, kBodyY, 2, h, kSurface);
  int th = std::max(8, h * visible / total);
  int ty = kBodyY + (h - th) * first / std::max(1, total - visible);
  g.fillRect(x, ty, 2, th, kMuted);
}

}  // namespace ui
}  // namespace cardlab
