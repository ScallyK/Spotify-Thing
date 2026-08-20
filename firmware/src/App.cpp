#include "App.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <SPI.h>
#include <time.h>

#include "Config.h"
#include "secrets.h"
#include "util/Timing.h"

// App keeps initialization and loop ordering in one place to preserve device timing.
App::App()
    : wifiManager(appState),
      clockManager(appState),
      spotifyApi(wifiManager, clockManager, appState),
      spotifyController(spotifyApi, appState),
      tftSpi(HSPI),
      tftDisplay(&tftSpi, Config::Pins::TftCs, Config::Pins::TftDc, Config::Pins::TftRst),
      display(tftDisplay, appState),
      volumeEncoder(Config::Pins::LeftClk, Config::Pins::LeftDt),
      navigationEncoder(Config::Pins::RightClk, Config::Pins::RightDt),
      playPauseButton(Config::Pins::LeftSw),
      devicePickerButton(Config::Pins::RightSw)
{}

void App::begin()
{

  // Set UTC before seeding the clock so build and HTTP times are interpreted consistently.
  Serial.begin(115200);

  setenv("TZ", "UTC0", 1);
  tzset();

  clockManager.begin();

  // Draw a startup screen before Wi-Fi work so the remote responds immediately after boot.
  tftSpi.begin();

  tftDisplay.initR(INITR_BLACKTAB);
  tftDisplay.setRotation(1);
  tftDisplay.setTextWrap(false);
  tftDisplay.fillScreen(ST77XX_BLACK);
  tftDisplay.setTextColor(ST77XX_GREEN);
  tftDisplay.setTextSize(2);
  tftDisplay.setCursor(22, 43);
  tftDisplay.print("NOW PLAYING");
  tftDisplay.setTextColor(ST77XX_WHITE);
  tftDisplay.setCursor(30, 72);
  tftDisplay.print("DIRECT MODE");

  volumeEncoder.begin();
  navigationEncoder.begin();
  playPauseButton.begin();
  devicePickerButton.begin();

  // Reset the station before joining so stale radio state cannot affect the first DHCP attempt.
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);

  delay(150);

  WiFi.setHostname("now-playing-remote");
  wifiManager.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  spotifyApi.begin();

}

void App::processControls()
{

  // Read both encoders once per loop so quick turns become accumulated actions.
  const int16_t volumeEncoderSteps = volumeEncoder.takeDetents();
  const int16_t navigationEncoderSteps = navigationEncoder.takeDetents();

  PlaybackState &playbackState = appState.playback;

  if (volumeEncoderSteps && appState.screen == Screen::NowPlaying && playbackState.canVolume &&
      playbackState.volume >= 0){
    // Delay the network request until rotation stops so several detents become one volume update.
    playbackState.volume = constrain(playbackState.volume + volumeEncoderSteps * 2, 0, 100);
    appState.volumeDirty = true;
    appState.lastVolumeTurnAt = millis();
    appState.redraw = true;
  }

  if (navigationEncoderSteps){

    // The right encoder queues track changes or moves through the device picker.
    if (appState.screen == Screen::NowPlaying){
      appState.pendingSkip = constrain(appState.pendingSkip + navigationEncoderSteps, -8, 8);
    }

    else if (appState.devices.count){
      appState.selectedDevice = constrain(int(appState.selectedDevice) + navigationEncoderSteps, 0,
                                          appState.devices.count - 1);
    }

    appState.redraw = true;
    appState.lastPollAt = 0;
  }

  if (playPauseButton.clicked() && appState.screen == Screen::NowPlaying){
    spotifyController.sendAction(PlayerAction::Toggle);
  }

  if (devicePickerButton.clicked()){

    // Refresh devices only when opening the picker so normal playback polling stays lightweight.
    if (appState.screen == Screen::NowPlaying){

      appState.screen = Screen::DevicePicker;
      spotifyController.refreshDevices(true);

      for (uint8_t i = 0; i < appState.devices.count; ++i)
      {
        if (appState.devices.entries[i].active){
          appState.selectedDevice = i;
        }
      }
      
    }

    else if (appState.devices.count){

      const ConnectDevice &choice = appState.devices.entries[appState.selectedDevice];

      if (choice.restricted){
        appState.statusMessage = "That device is locked";
      }

      else if (spotifyController.sendAction(PlayerAction::Transfer, -1, choice.id)){
        appState.screen = Screen::NowPlaying;
      }

    }

    appState.redraw = true;

  }

  if (appState.volumeDirty &&
      elapsed(millis(), appState.lastVolumeTurnAt, Config::Timing::VolumeDelay)){
    spotifyController.sendAction(PlayerAction::Volume, playbackState.volume);

    appState.volumeDirty = false;
  }

  if (appState.pendingSkip &&
      elapsed(millis(), appState.lastActionAt, Config::Timing::ActionDelay)){
    // Spotify receives queued skips one at a time to preserve request ordering.
    spotifyController.sendAction(appState.pendingSkip > 0 ? PlayerAction::Next
                                                          : PlayerAction::Previous);
    appState.pendingSkip += appState.pendingSkip > 0 ? -1 : 1;
  }

}

void App::pollSpotify()
{

  // HTTPS requests wait for both a DHCP address and a clock suitable for certificate validation.
  if (wifiManager.hasIpAddress() && clockManager.ready() &&
      elapsed(millis(), appState.lastPollAt, Config::Timing::Poll)){
    appState.lastPollAt = millis();
    spotifyController.refreshPlayback();
  }

}

void App::updateDisplay()
{

  // Full redraws handle state changes while progress redraws only update the moving playback details.
  if (appState.redraw){
    display.draw(wifiManager.hasIpAddress());
    appState.redraw = false;
    appState.lastDrawAt = millis();
    appState.lastProgressDrawAt = appState.lastDrawAt;
  }

  else if (elapsed(millis(), appState.lastProgressDrawAt, Config::Timing::ProgressRedraw)){
    display.drawProgress();
    appState.lastProgressDrawAt = millis();
  }

}

void App::update()
{

  // Keep this order aligned with the original single-file loop.
  wifiManager.update();
  clockManager.update(wifiManager.isAssociated(), wifiManager.hasIpAddress());
  processControls();
  pollSpotify();
  updateDisplay();
  
}