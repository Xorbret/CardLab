// CardLab: a pocket lab for learning to build, test and defend IoT devices
// on the M5Stack Cardputer ADV.
#include <Arduino.h>

#include "app.h"

static cardlab::App app;

void setup() { app.begin(); }

void loop() { app.loop(); }
