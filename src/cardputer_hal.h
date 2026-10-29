// Hal implementation for the M5Stack Cardputer / Cardputer ADV.
#pragma once

#include "hal.h"

namespace cardlab {

// SD card layout (copy the repo's content/ folder to /cardlab on the card).
constexpr const char* kRootDir = "/cardlab";
constexpr const char* kLessonDir = "/cardlab/lessons";
constexpr const char* kScenarioDir = "/cardlab/scenarios";
constexpr const char* kUserDir = "/cardlab/user";
constexpr const char* kProgressFile = "/cardlab/progress.txt";
constexpr const char* kExportFile = "/cardlab/progress.json";
constexpr const char* kAgreedFile = "/cardlab/agreed.txt";

class CardputerHal : public Hal {
 public:
  bool begin();  // mounts the SD card; false if no card
  bool sdReady() const { return sdReady_; }

  // Whole-path helpers for the firmware itself (not exposed to Lua).
  bool readAbsolute(const char* path, std::string& out);
  bool writeAbsolute(const char* path, const std::string& data);
  bool exists(const char* path);
  std::vector<std::string> listLua(const char* dir);  // sorted full paths

  void displayClear() override;
  void displayPrint(const std::string& text) override;
  bool gpioAllowed(int pin) override;
  void gpioMode(int pin, bool output) override;
  void gpioWrite(int pin, int value) override;
  int gpioRead(int pin) override;
  bool imuRead(ImuSample& out) override;
  std::vector<uint8_t> i2cScan() override;
  std::vector<WifiNetwork> wifiScan() override;
  bool fileRead(const std::string& path, std::string& out) override;
  bool fileWrite(const std::string& path, const std::string& data) override;
  uint32_t millis() override;
  void delayMs(uint32_t ms) override;

 private:
  bool sdReady_ = false;
};

}  // namespace cardlab
