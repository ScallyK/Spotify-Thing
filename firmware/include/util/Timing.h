#pragma once

#include <Arduino.h>

// This comparison remains correct when the unsigned millis counter rolls over.
inline bool elapsed(uint32_t now, uint32_t since, uint32_t duration)
{
  return static_cast<uint32_t>(now - since) >= duration;
}