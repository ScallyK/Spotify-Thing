#pragma once

#include <Arduino.h>

// QuadratureEncoder counts transitions in an interrupt and returns complete detents to the loop.
class QuadratureEncoder
{
  
 public:

  QuadratureEncoder(uint8_t clockGpioPin, uint8_t dataGpioPin);

  // Attach interrupts to both phases so every valid transition is counted.
  void begin();

  // Return whole detents while leaving a partial turn for a later loop iteration.
  int16_t takeDetents();

 private:

  uint8_t clockPin;
  uint8_t dataPin;

  volatile int16_t quadratureTransitions = 0;

  // This is written in the interrupt handler and read atomically by takeDetents.
  volatile uint8_t previousPinState = 0;

  static void isr(void* argument);

  void handleTransition();

};