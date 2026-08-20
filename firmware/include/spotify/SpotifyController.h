#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Models.h"

class SpotifyApi;

// SpotifyController maps Spotify responses and user actions into AppState updates.
class SpotifyController

{

 public:

  SpotifyController(SpotifyApi& spotifyApi, AppState& appState);

  // Replace the playback model with the latest player response.
  bool refreshPlayback();

  // Refresh the device list unless a still-valid cache is acceptable.
  bool refreshDevices(bool force = false);
  bool sendAction(PlayerAction action, int value = -1, const String& deviceId = "");

 private:

  // Convert transport and Spotify JSON failures into a concise display message.
  String apiError(const HttpResponse& response) const;
  
  static String jsonString(JsonVariantConst value, const char* fallback = "");

  SpotifyApi& spotifyApi;
  AppState& appState;

};