# CardLab

Small firmware designed to teach Users about coding and cybersecurity in a handheld form factor.

CardLab turns an M5Stack Cardputer ADV into a pocket lab for moving from IT Help Desk toward a Cyber Security Trainee role. Every lesson follows the same loop: **Learn, Build, Test, Break, Defend**. You read a short page, write Lua on the device's keyboard, and the device grades your work.

## What's in it

- **Lessons** on the built-in hardware: networking basics, the motion sensor, the I2C bus, GPIO pins on the Grove port, and passive Wi-Fi recon.
- **Defend the Network**: a turn-based Blue Team game. Red Team attacks a simulated office; you investigate symptoms, spend action points on evidence, and choose a response. Each round reveals the attack, what it targeted, and why your defense worked. Nothing is transmitted.
- **Lua console** with access to the hardware modules.
- **Tools**: I2C scan, passive Wi-Fi scan, Wi-Fi setup, and progress export to JSON for a portfolio.
- **Update lessons**: downloads the latest lessons and scenarios over Wi-Fi (HTTPS, checked against the built-in root certificates). Files that fail to load are skipped.

## Getting started

1. Install [PlatformIO](https://platformio.org/install).
2. Build and flash: `pio run -e cardputer-adv -t upload`
3. Copy the `content/` folder to a microSD card as `/cardlab` (so you have `/cardlab/lessons` and `/cardlab/scenarios`), insert it, and restart.

Prebuilt files are on the [dev-build release](https://github.com/Xorbret/CardLab/releases/tag/dev-build), updated by CI on every build:

- `cardlab-app.bin`: app image for M5Launcher.
- `cardlab.bin`: full image to flash at `0x0` with M5Burner or esptool.
- `cardlab-sd.zip`: unzip to the SD card root.

After the first install, set up Wi-Fi under **Tools** and use **Update lessons** to pull new content without touching the SD card. To use your own content source, put an `https://` URL to a pack (built with `tools/make_pack.py`) on the first line of `/cardlab/update-url.txt`.

### Keys

| Key | Action |
| --- | --- |
| `;` / `.` | Move up / down in menus and text |
| `fn` + `;` `.` `,` `/` | Arrow keys (editor) |
| `Enter` | Select / continue |
| `` ` `` or `fn` + `` ` `` | Back (in the editor and console, only `fn` + `` ` ``) |
| `Tab` | Run your code in the editor |

## Project layout

| Path | What it is |
| --- | --- |
| `lib/cardlab_core` | Platform-independent core: Lua sandbox, lesson engine, game engine, progress. Unit tested on the host. |
| `lib/lua` | Lua 5.4.7, vendored (core only; no `io`, `os` or `package`). |
| `src` | Device code: Cardputer hardware layer and UI. |
| `content` | Lessons and game scenarios, written as Lua tables. See [content/README.md](content/README.md). |
| `test/test_core` | Host tests: `pio test -e native` |

## Safety and ethics

CardLab only teaches attacks against equipment you own, and the firmware enforces that: Wi-Fi tools only listen, learner code runs in a sandbox that can reach only the hardware a lesson allows, and attacks such as deauthentication or evil twins are taught in the simulated game instead of being performed. See [docs/ETHICS.md](docs/ETHICS.md).
