#include <Arduino.h>
#include "App.h"

// A single App instance owns the firmware lifetime.
App app;

void setup()
{
  // Arduino invokes setup once after reset.
  app.begin();
}

void loop()
{
  // App keeps the cooperative loop ordering identical to the original firmware.
  app.update();
}