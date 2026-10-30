// Keyboard input and drawing for the 240x135 screen.
//
// Views draw a whole frame into an off-screen buffer and present() copies
// it to the LCD in one go, so nothing flickers between key presses.
#pragma once

#include <M5GFX.h>

#include <cstdint>
#include <string>
#include <vector>

namespace cardlab {

struct Key {
  enum Type { None, Char, Enter, Backspace, Esc, Tab, Up, Down, Left, Right };
  Type type = None;
  char c = 0;
};

namespace ui {

// Screen layout in pixels.
constexpr int kW = 240;
constexpr int kH = 135;
constexpr int kBarH = 16;              // status bar
constexpr int kFootH = 13;             // key hints
constexpr int kBodyY = kBarH + 3;      // first body pixel
constexpr int kBodyH = kH - kFootH - kBodyY - 2;
constexpr int kPad = 8;                // side margin

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Theme. Soft text on a dark blue-grey, one accent per mood.
constexpr uint16_t kBg = rgb(0x0E, 0x14, 0x1B);
constexpr uint16_t kSurface = rgb(0x18, 0x22, 0x2D);
constexpr uint16_t kRaised = rgb(0x22, 0x33, 0x42);
constexpr uint16_t kRule = rgb(0x2A, 0x39, 0x47);
constexpr uint16_t kText = rgb(0xE3, 0xEA, 0xF0);
constexpr uint16_t kMuted = rgb(0x86, 0x96, 0xA5);
constexpr uint16_t kTeal = rgb(0x2E, 0xC4, 0xB6);
constexpr uint16_t kAmber = rgb(0xF2, 0xA5, 0x41);
constexpr uint16_t kRed = rgb(0xEF, 0x5B, 0x5B);
constexpr uint16_t kGreen = rgb(0x4C, 0xC3, 0x8A);

// The colour a screen is tinted with: its status bar rule, selection bar
// and highlights.
enum class Tone { Normal, Game, Good, Bad };
uint16_t toneColor(Tone t);

// Fonts: body text, large headings and list items, small hints, and
// fixed-width code.
const lgfx::IFont* fontBody();
const lgfx::IFont* fontLarge();
const lgfx::IFont* fontSmall();
const lgfx::IFont* fontMono();
constexpr int kMonoW = 8;
constexpr int kMonoH = 16;

void begin();

// Returns one event per new key press. Arrows are fn + ; . , /
// and Esc is fn + `.
Key poll();

// Blocks until any key is pressed (used after learner code draws).
void waitKey();

// The drawing target for the current frame.
LovyanGFX& gfx();

// Starts a frame: clears the buffer and draws the status bar.
void frame(const std::string& title, Tone tone = Tone::Normal);
// Key hints along the bottom: "Enter open|` back" draws two chips, each
// with the key in a box and its action beside it.
void footer(const std::string& hints);
// Copies the finished frame to the LCD.
void present();
// Shows a one-off "please wait" screen right away (Wi-Fi, downloads).
void busy(const std::string& title, const std::string& message);

// Frees the frame buffer for memory-hungry work such as TLS downloads.
// Drawing goes straight to the LCD until the next frame() reallocates it.
void releaseBuffer();

// Splits on newlines only.
std::vector<std::string> splitLines(const std::string& text);
// Word-wraps to a pixel width in the given font.
std::vector<std::string> wrap(const std::string& text, const lgfx::IFont* font, int width);
// Shortens text with ".." to fit a pixel width.
std::string fit(const std::string& text, const lgfx::IFont* font, int width);

// A vertical scrollbar at the right edge of the body.
void scrollbar(int first, int visible, int total);

}  // namespace ui
}  // namespace cardlab
