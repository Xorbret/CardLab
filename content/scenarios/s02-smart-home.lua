return {
  id = "s02-smart-home",
  title = "The smart home that turned on itself",
  briefing = "A small business owner calls the help desk: their office smart devices are acting up. They use a cheap MQTT broker on a Raspberry Pi to tie it all together.",
  points = 3,
  rounds = {
    {
      symptom = "The office lights and a heater switch on at 3 AM. Nobody was there and no schedule is set.",
      damage = { uptime = 20, data = 20 },
      evidence = {
        { name = "Broker config", cost = 1,
          text = "MQTT on port 1883, allow_anonymous true, and the port is forwarded on the router." },
        { name = "Broker log", cost = 1,
          text = "At 03:02 a client from an overseas IP published to office/heater/set and office/lights/set." },
        { name = "Device firmware version", cost = 1, text = "Plugs are on current firmware." },
      },
      defenses = {
        { name = "Remove the port forward, require auth and TLS (8883)", result = "stop",
          text = "The broker is no longer reachable from the internet, and clients must log in over an encrypted link." },
        { name = "Update the smart plug firmware", result = "fail",
          text = "The plugs were fine. They obeyed commands from a broker that let anyone publish." },
        { name = "Turn on authentication only", result = "partial",
          text = "Blocks anonymous publishes, but passwords still cross the internet in plain text on 1883." },
      },
      reveal = {
        attack = "Unauthenticated MQTT broker exposed to the internet",
        target = "The message broker that every device trusts for commands",
        cia = "Integrity",
        technique = "OWASP IoT Top 10: I2 Insecure Network Services",
        lesson = "t0-02-network-refresher",
      },
    },
    {
      symptom = "A week later, the camera feed shows a stranger's 'hello' message overlaid on the image.",
      damage = { uptime = 10, data = 60 },
      evidence = {
        { name = "Camera login page", cost = 1, text = "Username admin, password admin still works." },
        { name = "Router device list", cost = 1, text = "Camera has a UPnP port mapping to the internet the owner never created." },
        { name = "Camera vendor advisory", cost = 2, text = "Vendor advises changing default credentials and disabling UPnP." },
      },
      defenses = {
        { name = "Change default password, disable UPnP, put cameras on an IoT VLAN", result = "stop",
          text = "Removes the open door, stops devices opening their own holes, and limits what a compromised camera can reach." },
        { name = "Factory reset the camera", result = "fail",
          text = "A reset brings back admin/admin, and UPnP opens the port again." },
        { name = "Change default password only", result = "partial",
          text = "Closes this door, but the camera is still exposed to the internet through UPnP." },
      },
      reveal = {
        attack = "Default credentials on an internet-exposed camera",
        target = "Factory logins and automatic port mapping (UPnP)",
        cia = "Confidentiality",
        technique = "OWASP IoT Top 10: I1 Weak, Guessable, or Hardcoded Passwords",
        lesson = "t0-02-network-refresher",
      },
    },
  },
}
