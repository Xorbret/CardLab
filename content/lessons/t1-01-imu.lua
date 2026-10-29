return {
  id = "t1-01-imu",
  track = 1,
  title = "Reading the motion sensor",
  permissions = { "imu", "display" },
  pages = {
    "The Cardputer has a 6-axis IMU: 3 axes of acceleration (in g) and 3 of rotation (degrees per second). It talks to the CPU over I2C.",
    "Lying flat, gravity pulls about 1 g on one axis and about 0 on the other two. That is how phones know which way is up.",
    "imu.read() returns ax, ay, az, gx, gy, gz. Lua functions can return several values at once.",
  },
  task = "Lay the Cardputer flat on a table, then print the largest of the three acceleration values (absolute value).",
  starter = [[local ax, ay, az = imu.read()
-- find the biggest of math.abs(ax), math.abs(ay), math.abs(az)
print(az)]],
  check = function(r)
    if not r.ok then return false, r.error end
    local v = tonumber(r.output:match("[-%d%.]+"))
    if not v then return false, "Print a number." end
    if v > 0.8 and v < 1.2 then return true, "About 1 g: gravity. Your sensor reading checks out." end
    return false, "Expected about 1.0 when flat. Got " .. v .. ". Did you take the largest absolute value?"
  end,
  quiz = {
    { q = "A sensor says a parked car is accelerating at 3 g. What should firmware do?",
      choices = { "Trust it", "Treat it as suspect and sanity-check it", "Reboot" }, answer = 2,
      why = "Out-of-range values can mean a fault or spoofing. Range checks are a basic defense." },
  },
  reflect = "How could a bad sensor reading turn into a security problem in a smart lock?",
}
