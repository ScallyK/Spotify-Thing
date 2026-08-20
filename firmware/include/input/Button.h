#pragma once

#include <Arduino.h>

// Button debounces a pull-up switch and reports one click when the switch is released.
class Button
{
 public:
  explicit Button(uint8_t buttonPin);
  // Configure the switch input and capture its initial released or pressed state.
  void begin();
  // Return true once after a debounced release edge.
  bool clicked();

 private:
  uint8_t buttonPin;
  // sampledPinState tracks the newest electrical level while stablePinState is debounced.
  bool stablePinState = HIGH;
  bool sampledPinState = HIGH;
  uint32_t lastSampleChangeAt = 0;
};
