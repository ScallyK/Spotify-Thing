#pragma once

#include <Arduino.h>

#include "Config.h"

// HttpResponse keeps the transport result separate from parsed Spotify state.
struct HttpResponse
{
  int statusCode = -1;
  String responseBody;
};

struct ConnectDevice
{
  
  // Spotify's device identifier is used only when requesting a transfer.
  String id;
  String name;
  String type;

  bool active = false;
  bool restricted = false;

};

struct PlaybackState
{

  // These fields are the last complete player response used by the display and controls.
  bool connected = false;
  bool active = false;
  bool playing = false;
  bool canVolume = false;

  int volume = -1;
  int progressMs = 0;
  int durationMs = 0;

  String title;
  String artist;
  String device;
  String repeat = "off";

  bool shuffle = false;
};

struct DeviceList
{
  // A fixed array avoids dynamic allocation while parsing the devices response.
  ConnectDevice entries[Config::Spotify::MaxDevices];
  uint8_t count = 0;
};

enum class Screen : uint8_t
{
  NowPlaying,
  DevicePicker
};

enum class PlayerAction : uint8_t
{
  Toggle,
  Next,
  Previous,
  Volume,
  Transfer
};

struct AppState
{

  // Shared state lets the cooperative managers update one consistent UI model.

  PlaybackState playback;
  DeviceList devices;

  Screen screen = Screen::NowPlaying;
  String statusMessage = "Starting Wi-Fi...";

  uint32_t lastPollAt = 0;
  uint32_t lastDevicesAt = 0;
  uint32_t lastDrawAt = 0;
  uint32_t lastProgressDrawAt = 0;
  uint32_t lastPlaybackAt = 0;
  uint32_t lastVolumeTurnAt = 0;
  uint32_t lastActionAt = 0;

  int pendingSkip = 0;

  uint8_t selectedDevice = 0;

  bool volumeDirty = false;
  bool redraw = true;

};