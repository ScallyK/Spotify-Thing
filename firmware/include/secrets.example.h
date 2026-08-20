#pragma once

// Copy this file to secrets.h and replace every value. Do not commit or share secrets.h.

#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// From your Spotify Developer Dashboard. This is public, not a client secret.
#define SPOTIFY_CLIENT_ID "replace-with-your-spotify-client-id"

// Generate this with pairing/pair_spotify.py and treat it like a password because the firmware exchanges it for short-lived Spotify access tokens.
#define SPOTIFY_REFRESH_TOKEN "replace-with-your-refresh-token"