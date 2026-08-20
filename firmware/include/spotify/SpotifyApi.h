#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "Models.h"

class WifiManager;
class ClockManager;

// SpotifyApi owns token storage and authenticated HTTPS requests.
class SpotifyApi

{
  
public:

  SpotifyApi(WifiManager &wifiManager, ClockManager &clockManager, AppState &appState);

  // Load a stored refresh-token rotation when it belongs to the configured credential.
  void begin();

  // Send an authenticated request to Spotify's player API.
  HttpResponse playerRequest(const char *httpMethod, const String &apiPath,
                             const String &requestBody = "");

private:

  // Renew the access token only when the cached token cannot safely cover another request.
  bool ensureToken();

  HttpResponse rawRequest(const char *httpMethod, const String &requestUrl,
                          const String &requestBody, const char *contentType, bool usesBearerToken);

  static String tokenFingerprint(const String &source);
  static String formEncode(const String &source);

  WifiManager &wifiManager;
  ClockManager &clockManager;

  AppState &appState;

  String accessToken;
  String configuredRefreshToken;
  String refreshToken;

  time_t accessExpiresAt = 0;
  Preferences tokenStore;

  // Preferences may be unavailable, so networking must work without persistent token storage.
  bool tokenStoreReady = false;
  
};