#include "input/QuadratureEncoder.h"

// Each pin change is decoded through the previous and current two-bit encoder state.
QuadratureEncoder::QuadratureEncoder(uint8_t clockGpioPin, uint8_t dataGpioPin) : clockPin(clockGpioPin), dataPin(dataGpioPin) {}

void QuadratureEncoder::begin()
{
  // Both phases use pull-ups because the encoder shorts each phase to ground.
  pinMode(clockPin, INPUT_PULLUP);
  pinMode(dataPin, INPUT_PULLUP);
  previousPinState = (digitalRead(clockPin) << 1) | digitalRead(dataPin);
  attachInterruptArg(digitalPinToInterrupt(clockPin), isr, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(dataPin), isr, this, CHANGE);
}

// One physical detent produces four valid quadrature transitions
int16_t QuadratureEncoder::takeDetents()
{
  noInterrupts();

  const int16_t detents = quadratureTransitions / 4;

  // Leave an incomplete rotation in the accumulator for the next loop pass.
  quadratureTransitions -= detents * 4;

  interrupts();

  return detents;
}

// This ISR stays in normal flash because it does not run while the flash cache is disabled.
void QuadratureEncoder::isr(void *argument)
{
  static_cast<QuadratureEncoder *>(argument)->handleTransition();
}

void QuadratureEncoder::handleTransition()
{
  
  // Combine the two pin levels into the same state encoding used by the lookup table.
  const uint8_t currentPinState = (digitalRead(clockPin) << 1) | digitalRead(dataPin);

  // Invalid jumps land on zero, which also helps reject switch bounce.
  static constexpr int8_t transitionTable[16] = {
      0,
      -1,
      1,
      0,
      1,
      0,
      0,
      -1,
      -1,
      0,
      0,
      1,
      0,
      1,
      -1,
      0,
  };

  quadratureTransitions += transitionTable[(previousPinState << 2) | currentPinState];
  previousPinState = currentPinState;

}