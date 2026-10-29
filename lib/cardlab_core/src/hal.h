// Hardware abstraction used by the Lua sandbox and lesson checks.
// The device build implements it with M5Cardputer; tests use a fake.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cardlab {

struct WifiNetwork {
  std::string ssid;
  int rssi = 0;
  std::string auth;  // "open", "wep", "wpa2", "wpa3", ...
  int channel = 0;
};

struct ImuSample {
  float ax = 0, ay = 0, az = 0;  // g
  float gx = 0, gy = 0, gz = 0;  // deg/s
};

class Hal {
 public:
  virtual ~Hal() = default;

  virtual void displayClear() = 0;
  virtual void displayPrint(const std::string& text) = 0;

  // Only pins exposed on the Grove port / EXT header are allowed, so a
  // lesson can never drive the pins wired to the LCD, keyboard or SD card.
  virtual bool gpioAllowed(int pin) = 0;
  virtual void gpioMode(int pin, bool output) = 0;
  virtual void gpioWrite(int pin, int value) = 0;
  virtual int gpioRead(int pin) = 0;

  virtual bool imuRead(ImuSample& out) = 0;
  virtual std::vector<uint8_t> i2cScan() = 0;

  // Passive scan only: lists access points, never transmits deauth or
  // other management frames.
  virtual std::vector<WifiNetwork> wifiScan() = 0;

  // Paths are relative to the learner's sandbox folder on the SD card.
  virtual bool fileRead(const std::string& path, std::string& out) = 0;
  virtual bool fileWrite(const std::string& path, const std::string& data) = 0;

  virtual uint32_t millis() = 0;
  virtual void delayMs(uint32_t ms) = 0;
};

}  // namespace cardlab
