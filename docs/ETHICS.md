# Safety and ethics

CardLab exists to train defenders. These rules are part of the design, not just a disclaimer.

## What the firmware will do

- **Listen passively.** The Wi-Fi scan reads beacons that access points broadcast to everyone, the same as a phone's network list.
- **Run learner code in a sandbox.** Lua runs with no file system, OS or bytecode access, a memory cap and a time limit. A lesson's code only sees the hardware modules that lesson lists in `permissions`.
- **Restrict pins.** GPIO is limited to the Grove port pins, so code cannot drive the pins wired to the screen, keyboard or SD card.
- **Teach attacks by simulation.** Defend the Network shows the symptoms, evidence, target and fix for attacks like deauthentication, evil twins, exposed MQTT brokers and default credentials, without sending a single packet.

## What it will not do

CardLab will not transmit deauthentication, disassociation or beacon floods, jam radios, clone or emulate other people's devices, or attack networks. Those features belong in authorized penetration-testing tools used under a written scope, not in a learning device.

## Your responsibilities

- Only test hardware and networks you own or have written permission to test.
- Unauthorized access is a crime in most countries, for example under the US Computer Fraud and Abuse Act and the UK Computer Misuse Act.
- Follow local radio rules. Do not transmit outside licensed bands or beyond duty-cycle limits (relevant for future LoRa lessons).
- Treat findings like a professional: keep notes, report through the right channel, and never use what you find.
