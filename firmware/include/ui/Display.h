#pragma once

#include <Adafruit_ST7735.h>

#include "Models.h"

// Display draws the current screen and preserves the original render-time device-selection clamp.
class Display

{
  
public:

  Display(Adafruit_ST7735 &tftDisplay, AppState &appState);

  // Redraw all screen content after a state change.
  void draw(bool wifiHasIp);

  // Redraw only moving playback details between full screen updates.
  void drawProgress();

private:

  // Estimate current progress from the last successful Spotify response and elapsed local time.
  int displayedProgress() const;

  void text(uint16_t x, uint16_t y, const String &textValue, uint16_t colour,
            uint8_t textSize = 1) const;

  void drawHeader(const String &label, bool wifiHasIp);
  void drawNowPlaying(bool wifiHasIp);
  void drawDevicePicker(bool wifiHasIp);

  Adafruit_ST7735 &tftDisplay;
  AppState &appState;

};