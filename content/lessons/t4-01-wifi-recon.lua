return {
  id = "t4-01-wifi-recon",
  track = 4,
  title = "Wi-Fi recon (passive)",
  permissions = { "wifi" },
  pages = {
    "Recon is the first step for attackers and defenders alike: what is out there and how is it protected?",
    "A passive scan only listens for beacons that access points broadcast anyway. It is legal and is what every phone does. Connecting to or attacking networks you do not own is not.",
    "Security types, weakest first: open, WEP (broken), WPA/WPA2-PSK (depends on the password), WPA3 (strongest). Open or WEP on a home or office network is a finding.",
  },
  task = "Scan and print each network as: ssid, auth. Then print how many are open or WEP.",
  starter = [[local nets = wifi.scan()
local weak = 0
for _, n in ipairs(nets) do
  print(n.ssid .. ", " .. n.auth)
  -- count weak ones here
end
print(weak)]],
  check = function(r)
    if not r.ok then return false, r.error end
    if not r.code:find("wifi.scan", 1, true) then return false, "Use wifi.scan()." end
    local last
    for n in r.output:gmatch("%d+") do last = tonumber(n) end
    if last == nil then return false, "Print the weak count on the last line." end
    return true, "Recon done. In the Defend the Network game you will see what attackers do next, and how to stop it."
  end,
  quiz = {
    { q = "Which of these is OK without permission?", choices = { "Passively listing nearby networks", "Guessing a neighbor's Wi-Fi password", "Kicking devices off a cafe network" },
      answer = 1, why = "Listening to public beacons is fine. Accessing or disrupting networks you do not own is illegal in most places." },
  },
  reflect = "Your office guest Wi-Fi shows as 'open'. Write the ticket you would raise and who it goes to.",
}
