#include "spotify/SpotifyApi.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

#include <cstring>

#include "network/ClockManager.h"
#include "network/WifiManager.h"
#include "secrets.h"
#include "spotify/SpotifyCertificate.h"

// Token storage accepts a saved rotation only when it belongs to the flashed refresh token.
SpotifyApi::SpotifyApi(WifiManager &wifiManager, ClockManager &clockManager, AppState &appState) : wifiManager(wifiManager), clockManager(clockManager), appState(appState) {}

void SpotifyApi::begin()
{

  // Start from the flashed credential and accept persisted rotations only from that source.
  configuredRefreshToken = String(SPOTIFY_REFRESH_TOKEN);
  refreshToken = configuredRefreshToken;

  if (!tokenStore.begin("spotify", false)){
    return;
  }

  tokenStoreReady = true;

  const String configuredId = tokenFingerprint(configuredRefreshToken);
  const String storedId = tokenStore.getString("source_id", "");
  const String storedRefresh = tokenStore.getString("refresh", "");

  if (storedId == configuredId && storedRefresh.length()){
    refreshToken = storedRefresh;
  }

  else{
    tokenStore.putString("source_id", configuredId);
    tokenStore.remove("refresh");
  }

}

bool SpotifyApi::ensureToken()
{

  // HTTPS must not start until DHCP and the certificate-validation clock are available.
  if (!wifiManager.hasIpAddress()){
    appState.statusMessage = "Connecting Wi-Fi...";
    return false;
  }

  if (!clockManager.ready()){
    appState.statusMessage = "Waiting for NTP clock...";
    return false;
  }

  // Refresh early enough that a request cannot begin with an expiring token.
  if (accessToken.length() && time(nullptr) < accessExpiresAt - 60){
    return true;
  }

  const String tokenRequestForm = String("grant_type=refresh_token&refresh_token=") +
                                  formEncode(refreshToken) +
                                  "&client_id=" + formEncode(SPOTIFY_CLIENT_ID);
  const HttpResponse tokenResponse =
      rawRequest("POST", "https://accounts.spotify.com/api/token", tokenRequestForm,
                 "application/x-www-form-urlencoded", false);

  if (tokenResponse.statusCode <= 0){
    return false;
  }

  if (tokenResponse.statusCode != HTTP_CODE_OK){

    // Do not keep a token known to be rejected by Spotify.
    accessToken = "";

    const bool missingCredentials =
        String(SPOTIFY_CLIENT_ID).length() == 0 ||
        String(SPOTIFY_CLIENT_ID) == "replace-with-your-spotify-client-id" ||
        refreshToken.length() == 0 || refreshToken == "replace-with-your-refresh-token";

    if (tokenResponse.statusCode == HTTP_CODE_BAD_REQUEST && !missingCredentials){
      DynamicJsonDocument errorDoc(768);

      if (!deserializeJson(errorDoc, tokenResponse.responseBody)){

        const char *code = errorDoc["error"];

        if (code && String(code).length()){
          appState.statusMessage = "Token: " + String(code);
          return false;
        }
      }

      appState.statusMessage = "Token: refresh rejected";

    }

    else{
      appState.statusMessage =
          missingCredentials ? "Check Spotify credentials" : "Spotify sign-in failed";
    }

    return false;

  }

  DynamicJsonDocument tokenDocument(2048);

  if (deserializeJson(tokenDocument, tokenResponse.responseBody)){
    appState.statusMessage = "Bad Spotify token reply";
    return false;
  }

  const char *accessTokenValue = tokenDocument["access_token"];

  if (!accessTokenValue){
    appState.statusMessage = "No Spotify access token";
    return false;
  }

  accessToken = accessTokenValue;
  accessExpiresAt = time(nullptr) + (tokenDocument["expires_in"] | 3600);

  const char *replacementRefreshToken = tokenDocument["refresh_token"];

  if (replacementRefreshToken && String(replacementRefreshToken).length()){

    // Spotify may rotate the refresh token, so persist the replacement for the next boot.
    refreshToken = replacementRefreshToken;

    if (tokenStoreReady){
      tokenStore.putString("refresh", refreshToken);
    }

  }

  return true;
}

HttpResponse SpotifyApi::playerRequest(const char *httpMethod, const String &apiPath,
                                       const String &requestBody)
{
  if (!ensureToken()){
    return HttpResponse{};
  }

  HttpResponse playerResponse =
      rawRequest(httpMethod, String("https://api.spotify.com/v1") + apiPath, requestBody,
                 "application/json", true);
  if (playerResponse.statusCode == HTTP_CODE_UNAUTHORIZED){

    // One retry with a fresh token handles access-token expiry during a player request.
    accessToken = "";

    if (ensureToken()){
      playerResponse = rawRequest(httpMethod, String("https://api.spotify.com/v1") + apiPath,
                                  requestBody, "application/json", true);
    }

  }

  return playerResponse;
}

HttpResponse SpotifyApi::rawRequest(const char *httpMethod, const String &requestUrl,
                                    const String &requestBody, const char *contentType,
                                    bool usesBearerToken)
{

  // Every request creates a fresh TLS client so credentials and connection state do not leak.
  if (!wifiManager.hasIpAddress()){
    appState.statusMessage = "Wi-Fi disconnected";
    return HttpResponse{};
  }

  WiFiClientSecure secureClient;
  secureClient.setCACert(SpotifyRootCa);

  HTTPClient httpRequest;

  httpRequest.setConnectTimeout(5000);
  httpRequest.setTimeout(8000);

  if (!httpRequest.begin(secureClient, requestUrl)){
    appState.statusMessage = "HTTPS connection failed";
    return HttpResponse{};
  }

  if (usesBearerToken){
    httpRequest.addHeader("Authorization", "Bearer " + accessToken);
  }

  if (requestBody.length()){
    httpRequest.addHeader("Content-Type", contentType);
  }

  if (!requestBody.length() &&
      (!strcmp(httpMethod, "PUT") || !strcmp(httpMethod, "POST") || !strcmp(httpMethod, "DELETE"))){

    // Spotify requires an explicit empty payload for bodyless mutating requests.
    httpRequest.addHeader("Content-Length", "0");
  }

  const int httpStatusCode = httpRequest.sendRequest(httpMethod, requestBody);

  HttpResponse httpResponse;
  httpResponse.statusCode = httpStatusCode;

  if (httpStatusCode > 0 && httpStatusCode != HTTP_CODE_NO_CONTENT){
    httpResponse.responseBody = httpRequest.getString();
  }

  httpRequest.end();

  if (httpStatusCode <= 0){

    // A zero TLS error usually indicates DNS or TCP failure rather than certificate rejection.
    char tlsDetail[96] = {};

    const int tlsError = secureClient.lastError(tlsDetail, sizeof(tlsDetail));

    Serial.printf("HTTPS failure: HTTP=%d TLS=%d (%s)\n", httpStatusCode, tlsError, tlsDetail);

    if (tlsError == 0){
      appState.statusMessage =
          !wifiManager.dnsFallbackInstalled() && wifiManager.installPublicDnsFallback()
              ? "DNS fallback enabled"
              : "DNS/TCP unavailable";
    }

    else{
      char compactError[24];
      snprintf(compactError, sizeof(compactError), "TLS error 0x%04X",
               static_cast<unsigned int>(-tlsError));
      appState.statusMessage = compactError;
    }

  }

  return httpResponse;

}

String SpotifyApi::tokenFingerprint(const String &source)
{

  // This non-secret identifier prevents a saved rotation from crossing credential changes.
  uint32_t hash = 2166136261UL;

  for (size_t i = 0; i < source.length(); ++i)
  {
    hash ^= static_cast<uint8_t>(source[i]);
    hash *= 16777619UL;
  }

  char identifier[9];

  snprintf(identifier, sizeof(identifier), "%08lx", static_cast<unsigned long>(hash));

  return String(identifier);

}

String SpotifyApi::formEncode(const String &source)
{

  // Refresh tokens are form values, so reserved characters must be percent encoded.
  const char *hex = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(source.length() * 3);

  for (size_t i = 0; i < source.length(); ++i)
  {

    const uint8_t value = static_cast<uint8_t>(source[i]);

    if (isalnum(value) || value == '-' || value == '_' || value == '.' || value == '~'){
      encoded += static_cast<char>(value);
    }

    else{
      encoded += '%';
      encoded += hex[value >> 4];
      encoded += hex[value & 0x0F];
    }
    
  }

  return encoded;

}