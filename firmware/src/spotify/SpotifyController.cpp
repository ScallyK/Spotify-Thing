#include "spotify/SpotifyController.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Config.h"
#include "spotify/SpotifyApi.h"

// JSON parsing stays here so SpotifyApi remains responsible only for transport and tokens.
SpotifyController::SpotifyController(SpotifyApi &spotifyApi, AppState &appState) : spotifyApi(spotifyApi), appState(appState) {}

String SpotifyController::jsonString(JsonVariantConst value, const char *fallback)
{
  const char *text = value.as<const char *>();
  return text ? String(text) : String(fallback);
}

String SpotifyController::apiError(const HttpResponse &response) const
{

  // Prefer Spotify's message when it is present, then retain a useful transport fallback.
  if (response.statusCode == HTTP_CODE_TOO_MANY_REQUESTS){
    return "Spotify rate limited";
  }

  DynamicJsonDocument doc(1024);

  if (!deserializeJson(doc, response.responseBody)){
    JsonVariantConst error = doc["error"];

    if (error.is<JsonObjectConst>()){
      return jsonString(error["message"], "Spotify error");
    }

    if (error.is<const char *>()){
      return jsonString(error, "Spotify error");
    }

  }
  return response.statusCode > 0 ? "Spotify error " + String(response.statusCode)
                                 : appState.statusMessage;
}

bool SpotifyController::refreshDevices(bool force)
{

  // The picker may request an immediate refresh, while routine opens use the cache.
  if (!force && millis() - appState.lastDevicesAt < Config::Timing::DeviceCache){
    return true;
  }

  const HttpResponse response = spotifyApi.playerRequest("GET", "/me/player/devices");

  if (response.statusCode != HTTP_CODE_OK){
    appState.statusMessage = apiError(response);
    return false;
  }

  DynamicJsonDocument doc(8192);

  if (deserializeJson(doc, response.responseBody)){
    appState.statusMessage = "Bad devices reply";
    return false;
  }

  appState.devices.count = 0;

  for (JsonObject device : doc["devices"].as<JsonArray>())
  {

    // Keep the fixed AppState list bounded even if Spotify returns more devices.
    if (appState.devices.count >= Config::Spotify::MaxDevices){
      break;
    }

    ConnectDevice &destination = appState.devices.entries[appState.devices.count++];

    destination.id = jsonString(device["id"]);
    destination.name = jsonString(device["name"], "Unnamed device");
    destination.type = jsonString(device["type"], "device");
    destination.active = device["is_active"] | false;
    destination.restricted = device["is_restricted"] | false;

  }
  appState.lastDevicesAt = millis();

  return true;

}

bool SpotifyController::refreshPlayback()
{

  // A 204 response is normal when Spotify has no currently active device.
  const HttpResponse response = spotifyApi.playerRequest("GET", "/me/player");

  PlaybackState &playback = appState.playback;

  if (response.statusCode == HTTP_CODE_NO_CONTENT){
    const bool changed = !playback.connected || playback.active ||
                         appState.statusMessage != "No active Spotify device";
    playback.connected = true;
    playback.active = false;
    appState.statusMessage = "No active Spotify device";

    if (changed){
      appState.redraw = true;
    }

    return true;

  }

  if (response.statusCode != HTTP_CODE_OK){
    const String message = apiError(response);
    const bool changed = playback.connected || appState.statusMessage != message;

    playback.connected = false;
    appState.statusMessage = message;

    if (changed){
      appState.redraw = true;
    }

    return false;

  }
  
  DynamicJsonDocument doc(12288);

  if (deserializeJson(doc, response.responseBody)){

    const bool changed = playback.connected || appState.statusMessage != "Bad Spotify reply";

    playback.connected = false;
    appState.statusMessage = "Bad Spotify reply";

    if (changed){
      appState.redraw = true;
    }

    return false;
  }

  JsonObject item = doc["item"].as<JsonObject>();
  JsonObject device = doc["device"].as<JsonObject>();

  const bool nextPlaying = doc["is_playing"] | false;
  const bool nextCanVolume = device["supports_volume"] | false;

  const int nextVolume =
      device["volume_percent"].is<int>() ? device["volume_percent"].as<int>() : -1;
  const int nextProgressMs = doc["progress_ms"] | 0;
  const int nextDurationMs = item["duration_ms"] | 0;

  const String nextTitle = jsonString(item["name"], "Unknown title");
  String nextArtist;

  for (JsonObject artist : item["artists"].as<JsonArray>())
  {

    // Spotify exposes artists as an array, but the display presents one label.
    if (nextArtist.length()){
      nextArtist += ", ";
    }

    nextArtist += jsonString(artist["name"]);

  }

  if (!nextArtist.length()){
    nextArtist = "Unknown artist";
  }

  const String nextDevice = jsonString(device["name"], "Unknown device");
  const String nextRepeat = jsonString(doc["repeat_state"], "off");
  const bool nextShuffle = doc["shuffle_state"] | false;
  const bool changed = !playback.connected || !playback.active || playback.playing != nextPlaying ||
                       playback.canVolume != nextCanVolume || playback.volume != nextVolume ||
                       playback.durationMs != nextDurationMs || playback.title != nextTitle ||
                       playback.artist != nextArtist || playback.device != nextDevice ||
                       playback.shuffle != nextShuffle || playback.repeat != nextRepeat ||
                       appState.statusMessage.length();
  playback.connected = true;

  // Commit the complete response together so a redraw never sees partial playback state.
  playback.active = true;
  playback.playing = nextPlaying;
  playback.canVolume = nextCanVolume;
  playback.volume = nextVolume;
  playback.progressMs = nextProgressMs;
  playback.durationMs = nextDurationMs;
  playback.title = nextTitle;
  playback.artist = nextArtist;
  playback.device = nextDevice;
  playback.shuffle = nextShuffle;
  playback.repeat = nextRepeat;
  appState.lastPlaybackAt = millis();
  appState.statusMessage = "";

  if (changed){
    appState.redraw = true;
  }

  return true;

}

bool SpotifyController::sendAction(PlayerAction action, int value, const String &deviceId)
{

  // Centralizing actions keeps the API paths and post-action polling behavior consistent.

  PlaybackState &playback = appState.playback;
  HttpResponse response;

  switch (action)
  {
  case PlayerAction::Toggle:

    if (!playback.active){
      appState.statusMessage = "No active Spotify device";
      return false;
    }

    response = spotifyApi.playerRequest(
        "PUT", playback.playing ? "/me/player/pause" : "/me/player/play");
    break;

  case PlayerAction::Next:
    response = spotifyApi.playerRequest("POST", "/me/player/next");
    break;

  case PlayerAction::Previous:
    response = spotifyApi.playerRequest("POST", "/me/player/previous");
    break;

  case PlayerAction::Volume:
    response =
        spotifyApi.playerRequest("PUT", "/me/player/volume?volume_percent=" + String(value));
    break;

  case PlayerAction::Transfer:
    // Preserve the original behavior by transferring without starting playback.
    response = spotifyApi.playerRequest(
        "PUT", "/me/player", String("{\"device_ids\":[\"") + deviceId + "\"],\"play\":false}");
    break;

  default:
    appState.statusMessage = "Unknown control";
    return false;
  }

  if (response.statusCode != HTTP_CODE_NO_CONTENT && response.statusCode != HTTP_CODE_OK){
    appState.statusMessage = apiError(response);
    return false;
  }

  appState.statusMessage = "";

  if (action == PlayerAction::Toggle){
    playback.playing = !playback.playing;
    appState.lastPlaybackAt = millis();
  }

  appState.lastPollAt = millis() - Config::Timing::Poll + Config::Timing::ControlRefreshDelay;

  // Delay the refresh briefly so Spotify has time to apply the requested change.
  if (action == PlayerAction::Transfer){
    appState.lastDevicesAt = 0;
  }

  appState.lastActionAt = millis();
  appState.redraw = true;

  return true;

}