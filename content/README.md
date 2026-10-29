# Writing lessons and scenarios

Content files are Lua files that `return` a table. Copy this folder to `/cardlab` on the SD card. Files are listed in filename order, so prefix them with their track (`t1-02-...`). `pio test -e native` loads every file here, so a broken file fails CI.

## Lesson (`lessons/*.lua`)

```lua
return {
  id = "t1-02-i2c-scan",          -- unique; used for saved progress
  track = 1,                      -- 0 Orientation, 1 Build, 2 Connect, 3 Test, 4 Secure
  title = "Finding devices on I2C",
  permissions = { "i2c" },        -- modules the learner's code may use
  pages = { "Page one...", "Page two..." },
  task = "What the learner must do.",
  starter = [[ starter code ]],
  check = function(r)             -- r.ok, r.output, r.error, r.code
    return true, "feedback shown to the learner"
  end,
  quiz = {
    { q = "Question?", choices = { "A", "B", "C" }, answer = 2, why = "Explanation." },  -- answer is 1-based
  },
  reflect = "A help-desk style reflection prompt.",
}
```

`check` runs in its own sandbox with read-only hardware (`imu`, `i2c`, `sys`), so it can compare the learner's output with a live reading. Leave out `check` for a reading-and-quiz lesson.

### Modules available to learner code

| Permission | Lua API |
| --- | --- |
| `display` | `display.clear()`, `display.print(text)` |
| `gpio` | `gpio.mode(pin, "in"/"out")`, `gpio.write(pin, 0/1)`, `gpio.read(pin)` (Grove pins only) |
| `imu` | `imu.read()` returns `ax, ay, az, gx, gy, gz` |
| `i2c` | `i2c.scan()` returns a list of addresses on the Grove port |
| `wifi` | `wifi.scan()` returns `{ ssid, rssi, auth, channel }` entries (passive) |
| `files` | `files.read(path)`, `files.write(path, text)` inside `/cardlab/user` |
| `sys` | `sys.millis()`, `sys.sleep(ms)` (up to 5000) |

## Scenario (`scenarios/*.lua`)

```lua
return {
  id = "s01-office-wifi",
  title = "Trouble on the office Wi-Fi",
  briefing = "Set the scene.",
  points = 3,                     -- action points per round
  rounds = {
    {
      symptom = "What the Blue Team sees, without naming the attack.",
      damage = { uptime = 40, data = 0 },   -- applied if the response fails (half if partial)
      evidence = {
        { name = "AP event log", cost = 1, text = "What the evidence shows." },
      },
      defenses = {
        { name = "The right fix", result = "stop", text = "Why it works." },
        { name = "A half fix", result = "partial", text = "What it misses." },
        { name = "A wrong fix", result = "fail", text = "Why it does nothing." },
      },
      reveal = {
        attack = "Name of the attack",
        target = "What it goes after",
        cia = "Availability",
        technique = "MITRE ATT&CK or OWASP IoT reference",
        lesson = "lesson id to study next",
      },
    },
  },
}
```

Scoring per round: 100 for `stop`, 50 for `partial`, 0 for `fail`, plus 10 per unspent action point when the response helped.
