#include "cardputer_hal.h"

#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>

#include <algorithm>

namespace cardlab {

namespace {

// microSD wiring on the Cardputer and Cardputer ADV.
constexpr int kSdSck = 40;
constexpr int kSdMiso = 39;
constexpr int kSdMosi = 14;
constexpr int kSdCs = 12;

const char* authName(wifi_auth_mode_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN: return "open";
    case WIFI_AUTH_WEP: return "wep";
    case WIFI_AUTH_WPA_PSK: return "wpa";
    case WIFI_AUTH_WPA2_PSK: return "wpa2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "wpa/wpa2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "wpa2-ent";
    case WIFI_AUTH_WPA3_PSK: return "wpa3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "wpa2/wpa3";
    default: return "other";
  }
}

}  // namespace

bool CardputerHal::begin() {
  SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
  sdReady_ = SD.begin(kSdCs, SPI, 25000000);
  if (sdReady_) {
    if (!SD.exists(kRootDir)) SD.mkdir(kRootDir);
    if (!SD.exists(kUserDir)) SD.mkdir(kUserDir);
  }
  return sdReady_;
}

bool CardputerHal::readAbsolute(const char* path, std::string& out) {
  if (!sdReady_) return false;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) return false;
  out.clear();
  out.reserve(f.size());
  char buf[256];
  while (f.available()) {
    int n = f.read(reinterpret_cast<uint8_t*>(buf), sizeof(buf));
    if (n <= 0) break;
    out.append(buf, n);
  }
  f.close();
  return true;
}

bool CardputerHal::writeAbsolute(const char* path, const std::string& data) {
  if (!sdReady_) return false;
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  size_t n = f.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
  f.close();
  return n == data.size();
}

bool CardputerHal::exists(const char* path) { return sdReady_ && SD.exists(path); }

std::vector<std::string> CardputerHal::listLua(const char* dir) {
  std::vector<std::string> out;
  if (!sdReady_) return out;
  File d = SD.open(dir);
  if (!d || !d.isDirectory()) return out;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    std::string name = f.name();
    if (!f.isDirectory() && name.size() > 4 && name.compare(name.size() - 4, 4, ".lua") == 0) {
      out.push_back(std::string(dir) + "/" + name.substr(name.find_last_of('/') + 1));
    }
    f.close();
  }
  std::sort(out.begin(), out.end());
  return out;
}

void CardputerHal::displayClear() {
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setCursor(0, 0);
}

void CardputerHal::displayPrint(const std::string& text) {
  M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5Cardputer.Display.print(text.c_str());
}

bool CardputerHal::gpioAllowed(int pin) {
  return pin == M5.getPin(m5::pin_name_t::port_a_pin1) || pin == M5.getPin(m5::pin_name_t::port_a_pin2);
}

void CardputerHal::gpioMode(int pin, bool output) { pinMode(pin, output ? OUTPUT : INPUT); }
void CardputerHal::gpioWrite(int pin, int value) { digitalWrite(pin, value ? HIGH : LOW); }
int CardputerHal::gpioRead(int pin) { return digitalRead(pin); }

bool CardputerHal::imuRead(ImuSample& out) {
  if (!M5.Imu.isEnabled()) return false;
  M5.Imu.update();
  return M5.Imu.getAccel(&out.ax, &out.ay, &out.az) && M5.Imu.getGyro(&out.gx, &out.gy, &out.gz);
}

std::vector<uint8_t> CardputerHal::i2cScan() {
  std::vector<uint8_t> found;
  if (!M5.Ex_I2C.isEnabled()) M5.Ex_I2C.begin();
  bool result[0x80] = {};
  M5.Ex_I2C.scanID(result);
  for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
    if (result[addr]) found.push_back(addr);
  }
  return found;
}

std::vector<WifiNetwork> CardputerHal::wifiScan() {
  std::vector<WifiNetwork> nets;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    WifiNetwork w;
    w.ssid = WiFi.SSID(i).c_str();
    if (w.ssid.empty()) w.ssid = "(hidden)";
    w.rssi = WiFi.RSSI(i);
    w.auth = authName(WiFi.encryptionType(i));
    w.channel = WiFi.channel(i);
    nets.push_back(w);
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  return nets;
}

bool CardputerHal::fileRead(const std::string& path, std::string& out) {
  return readAbsolute((std::string(kUserDir) + "/" + path).c_str(), out);
}

bool CardputerHal::fileWrite(const std::string& path, const std::string& data) {
  return writeAbsolute((std::string(kUserDir) + "/" + path).c_str(), data);
}

uint32_t CardputerHal::millis() { return ::millis(); }
void CardputerHal::delayMs(uint32_t ms) { ::delay(ms); }

}  // namespace cardlab
