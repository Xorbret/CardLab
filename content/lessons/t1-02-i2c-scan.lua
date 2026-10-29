return {
  id = "t1-02-i2c-scan",
  track = 1,
  title = "Finding devices on I2C",
  permissions = { "i2c" },
  pages = {
    "I2C is a two-wire bus (SDA and SCL). Each device has a 7-bit address, so the CPU can talk to many sensors on the same two wires.",
    "The Grove port on the Cardputer is an I2C port. Plug in a Grove sensor and it shows up at its address, for example 0x44 for an SHT3x.",
    "Hardware testers scan buses to learn what is inside a device. You will do the same on hardware you own.",
  },
  task = "Scan the Grove I2C bus and print how many devices were found (0 is fine if nothing is plugged in).",
  starter = [[local found = i2c.scan()
for _, addr in ipairs(found) do
  print(string.format("0x%02X", addr))
end]],
  check = function(r)
    if not r.ok then return false, r.error end
    local expected = #i2c.scan()
    local last
    for n in r.output:gmatch("%d+") do last = tonumber(n) end
    if last == expected then
      return true, "Correct: " .. expected .. " device(s) on the bus."
    end
    return false, "The check sees " .. expected .. " device(s). Print #found at the end."
  end,
  quiz = {
    { q = "Why can many sensors share one I2C bus?", choices = { "Each has its own address", "They take turns by time", "They use different voltages" },
      answer = 1, why = "The address in each message picks which device answers." },
  },
  reflect = "If you found an unknown device on a bus inside a product, what would you look up first?",
}
