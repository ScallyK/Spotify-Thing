#include "input/Button.h"

// The switch is wired to ground, so HIGH means released.
Button::Button(uint8_t buttonPin) : buttonPin(buttonPin) {}

void Button::begin()
{
  // Capture the idle level before the first debounce sample is taken.
  pinMode(buttonPin, INPUT_PULLUP);
  stablePinState = sampledPinState = digitalRead(buttonPin);
}

// Buttons use pull ups, so a click is reported when the button is released
bool Button::clicked()
{
  
  const bool currentPinState = digitalRead(buttonPin);

  if (currentPinState != sampledPinState){
    // A new electrical level starts a fresh debounce interval.
    sampledPinState = currentPinState;
    lastSampleChangeAt = millis();
  }

  if (currentPinState != stablePinState && millis() - lastSampleChangeAt > 25){
    // Report only the stable release edge so a held button cannot repeat.
    stablePinState = currentPinState;
    return stablePinState == HIGH;
  }

  return false;

}