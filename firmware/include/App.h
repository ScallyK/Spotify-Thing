#pragma once

#include <Adafruit_ST7735.h>
#include <SPI.h>

#include "Models.h"
#include "input/Button.h"
#include "input/QuadratureEncoder.h"
#include "network/ClockManager.h"
#include "network/WifiManager.h"
#include "spotify/SpotifyApi.h"
#include "spotify/SpotifyController.h"
#include "ui/Display.h"

// App owns every runtime component and runs them from one cooperative loop.
class App
{
  
public:

  App();
  void begin();
  void update();

private:

  void processControls();

  // Polling and drawing are separate so input always runs before network work.
  void pollSpotify();
  void updateDisplay();

  AppState appState;

  // Managers share appState but retain responsibility for their own protocol details.
  WifiManager wifiManager;
  ClockManager clockManager;

  SpotifyApi spotifyApi;
  SpotifyController spotifyController;

  SPIClass tftSpi;

  Adafruit_ST7735 tftDisplay;

  Display display;

  QuadratureEncoder volumeEncoder;
  QuadratureEncoder navigationEncoder;

  Button playPauseButton;
  Button devicePickerButton;

};