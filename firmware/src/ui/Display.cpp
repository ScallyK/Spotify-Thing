#include "ui/Display.h"

#include <Adafruit_GFX.h>

#include "Config.h"
#include "util/TextUtils.h"

// Display owns screen layout while TextUtils handles font-safe text formatting.
Display::Display(Adafruit_ST7735 &tftDisplay, AppState &appState) : tftDisplay(tftDisplay), appState(appState) {}

int Display::displayedProgress() const
{

  // Advance only locally while playing so the progress bar stays smooth between polls.

  int progress = appState.playback.progressMs;

  if (appState.playback.playing && appState.playback.active){
    progress += millis() - appState.lastPlaybackAt;
  }

  return appState.playback.durationMs > 0 ? min(progress, appState.playback.durationMs) : progress;

}

void Display::text(uint16_t x, uint16_t y, const String &textValue, uint16_t colour,
                   uint8_t textSize) const
{

  // Sanitize immediately before drawing because Spotify metadata can contain unsupported glyphs.
  tftDisplay.setTextSize(textSize);
  tftDisplay.setTextColor(colour);
  tftDisplay.setCursor(x, y);
  tftDisplay.print(TextUtils::sanitizeForBuiltinFont(textValue));

}

void Display::drawHeader(const String &label, bool wifiHasIp)
{

  // The status strip is redrawn with every full screen to avoid stale connection text.
  tftDisplay.fillRect(0, 0, Config::Display::Width, 15, ST77XX_GREEN);

  text(4, 4, label, ST77XX_BLACK);

  const String wifiLabel = wifiHasIp ? "WiFi" : "NO WiFi";

  text(Config::Display::Width - 4 - TextUtils::measureTextWidth(tftDisplay, wifiLabel), 4,
       wifiLabel, ST77XX_BLACK);

}

void Display::drawNowPlaying(bool wifiHasIp)
{

  // Keep the offline and inactive states readable without requiring Spotify metadata.
  const PlaybackState &playback = appState.playback;

  tftDisplay.fillScreen(ST77XX_BLACK);

  drawHeader("NOW PLAYING", wifiHasIp);

  if (!playback.connected || !playback.active){

    const uint16_t colour = playback.connected ? ST77XX_YELLOW : ST77XX_RED;

    text(10, 28, playback.connected ? "NO ACTIVE" : "SPOTIFY", colour, 2);
    text(10, 50, playback.connected ? "DEVICE" : "OFFLINE", colour, 2);
    text(10, 76, TextUtils::clipToWidth(tftDisplay, appState.statusMessage, 140), ST77XX_WHITE);
    text(10, 94, playback.connected ? "Start Spotify, then" : "Check Wi-Fi, clock,", ST77XX_CYAN);
    text(10, 108, playback.connected ? "turn right dial" : "and pairing", ST77XX_CYAN);

    return;

  }

  text(6, 22, TextUtils::clipToWidth(tftDisplay, playback.title, 148), ST77XX_WHITE);
  text(6, 37, TextUtils::clipToWidth(tftDisplay, playback.artist, 148), ST77XX_CYAN);
  text(6, 52, TextUtils::clipToWidth(tftDisplay, playback.device, 148), ST77XX_YELLOW);

  const int progress = displayedProgress();

  text(6, 69,
       TextUtils::formatPlaybackTime(progress) + " / " +
           TextUtils::formatPlaybackTime(playback.durationMs),
       ST77XX_WHITE);

  if (playback.playing){
    tftDisplay.fillRect(137, 67, 4, 9, ST77XX_GREEN);
    tftDisplay.fillRect(145, 67, 4, 9, ST77XX_GREEN);
  }

  else{
    tftDisplay.fillTriangle(138, 66, 138, 77, 149, 71, ST77XX_YELLOW);
  }

  tftDisplay.drawRect(6, 84, 148, 8, ST77XX_WHITE);

  if (playback.durationMs > 0){
    // Clamp the calculated fill width to the inner border of the progress bar.
    tftDisplay.fillRect(7, 85, min(146L, (146L * progress) / playback.durationMs), 6, ST77XX_GREEN);
  }

  if (appState.statusMessage.length()){
    text(6, 102, TextUtils::clipToWidth(tftDisplay, appState.statusMessage, 148), ST77XX_RED);
  }

  else{

    text(6, 102,
         playback.canVolume && playback.volume >= 0 ? "VOL " + String(playback.volume) + "%"
                                                    : "VOL --",
         ST77XX_WHITE);

    text(70, 102,
         TextUtils::clipToWidth(
             tftDisplay, String(playback.shuffle ? "SHUF" : "    ") + "  R:" + playback.repeat, 78),
         ST77XX_CYAN);

  }

}

void Display::drawDevicePicker(bool wifiHasIp)
{

  // Device selection is clamped at render time to match the original screen behavior.
  tftDisplay.fillScreen(ST77XX_BLACK);

  drawHeader("SELECT CONNECT DEVICE", wifiHasIp);

  if (!appState.devices.count){

    text(8, 38, "NO DEVICES FOUND", ST77XX_YELLOW, 2);
    text(8, 70, "Start Spotify on a", ST77XX_WHITE);
    text(8, 84, "phone, PC, or speaker", ST77XX_WHITE);

    return;

  }

  if (appState.selectedDevice >= appState.devices.count){
    appState.selectedDevice = appState.devices.count - 1;
  }

  const uint8_t selected = appState.selectedDevice;
  const uint8_t first = (selected / 5) * 5;

  // Each page contains five two-line device entries on the 160 by 128 display.
  for (uint8_t i = first; i < appState.devices.count && i < first + 5; ++i)
  {

    const uint16_t y = 20 + (i - first) * 20;
    const ConnectDevice &device = appState.devices.entries[i];
    const bool isSelected = i == selected;

    if (isSelected){
      tftDisplay.fillRect(2, y - 2, 156, 18, ST77XX_GREEN);
    }

    const uint16_t foreground = isSelected ? ST77XX_BLACK : ST77XX_WHITE;

    text(5, y,
         TextUtils::clipToWidth(tftDisplay, String(device.active ? "> " : "  ") + device.name, 148),
         foreground);

    text(5, y + 9,
         TextUtils::clipToWidth(tftDisplay, device.type + (device.restricted ? " (locked)" : ""),
                                148),
         isSelected ? ST77XX_BLACK : ST77XX_CYAN);

  }

  if (appState.statusMessage.length()){
    text(4, 112, TextUtils::clipToWidth(tftDisplay, appState.statusMessage, 148), ST77XX_RED);
  }

}

void Display::draw(bool wifiHasIp)
{

  if (appState.screen == Screen::NowPlaying){
    drawNowPlaying(wifiHasIp);
  }

  else{
    drawDevicePicker(wifiHasIp);
  }

}

void Display::drawProgress()
{

  // This inexpensive path updates only live playback fields between complete redraws.
  const PlaybackState &playback = appState.playback;

  if (appState.screen != Screen::NowPlaying || !playback.connected || !playback.active ||
      !playback.playing){
    return;
  }

  const int progress = displayedProgress();

  tftDisplay.fillRect(6, 67, 126, 10, ST77XX_BLACK);

  text(6, 69,
       TextUtils::formatPlaybackTime(progress) + " / " +
           TextUtils::formatPlaybackTime(playback.durationMs),
       ST77XX_WHITE);

  tftDisplay.fillRect(6, 84, 148, 8, ST77XX_BLACK);
  tftDisplay.drawRect(6, 84, 148, 8, ST77XX_WHITE);
  
  if (playback.durationMs > 0){
    tftDisplay.fillRect(7, 85, min(146L, (146L * progress) / playback.durationMs), 6, ST77XX_GREEN);
  }
  
}