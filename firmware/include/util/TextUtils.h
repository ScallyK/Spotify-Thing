#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>

// TextUtils adapts Spotify text to the built-in display font and available screen width.
namespace TextUtils
{

    String sanitizeForBuiltinFont(const String &textValue);

    // Measure text using the same driver state that will render it.
    uint16_t measureTextWidth(Adafruit_GFX &displayDriver, const String &textValue,
                              uint8_t textSize = 1);

    String clipToWidth(Adafruit_GFX &displayDriver, const String &textValue, uint16_t maximumWidth,
                       uint8_t textSize = 1);

    // Format nonnegative elapsed playback time as minutes and seconds.
    String formatPlaybackTime(int milliseconds);

} // namespace TextUtils