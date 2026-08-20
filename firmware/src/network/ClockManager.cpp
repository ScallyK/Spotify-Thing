#include "network/ClockManager.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

#include "Config.h"

// Build time gives certificate validation a plausible clock before network time arrives.
ClockManager::ClockManager(AppState &appState) : appState(appState) {}

bool ClockManager::begin()
{
  // Parse the compiler timestamp as a temporary UTC clock for TLS verification.
  struct tm buildTimestamp = {};

  if (!strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &buildTimestamp)){
    return false;
  }

  buildTimestamp.tm_isdst = 0;
  const time_t clockEpoch = mktime(&buildTimestamp);

  if (clockEpoch <= 1700000000){
    return false;
  }

  const timeval systemTime = {clockEpoch, 0};
  seededFromBuild = settimeofday(&systemTime, nullptr) == 0;

  if (seededFromBuild){
    appState.statusMessage = "Clock seeded; starting Wi-Fi...";
  }

  return seededFromBuild;
}

bool ClockManager::ready() const
{
  return time(nullptr) > 1700000000;
}

void ClockManager::requestNtpSync()
{

  // Remember the first attempt so the HTTP fallback has one stable deadline.
  if (!ntpSyncRequested){
    firstNtpAttemptAt = millis();
  }

  setenv("TZ", "UTC0", 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  lastNtpAttemptAt = millis();
  ntpSyncRequested = true;

  if (!seededFromBuild){
    appState.statusMessage = "Syncing NTP clock...";
    appState.redraw = true;
  }
}

bool ClockManager::syncFromHttpDate()
{

  // A plain HTTP Date header is sufficient because it only seeds the local clock.
  WiFiClient httpDateClient;
  HTTPClient httpDateRequest;

  const char *headerKeys[] = {"Date"};

  httpDateRequest.setConnectTimeout(4000);
  httpDateRequest.setTimeout(5000);

  if (!httpDateRequest.begin(httpDateClient, "http://example.com/")){
    return false;
  }

  httpDateRequest.collectHeaders(headerKeys, 1);

  const int httpStatusCode = httpDateRequest.GET();
  const String httpDateHeader = httpDateRequest.header("Date");

  httpDateRequest.end();

  if (httpStatusCode <= 0 || !httpDateHeader.length()){
    return false;
  }

  struct tm utcTimestamp = {};

  if (!strptime(httpDateHeader.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &utcTimestamp)){
    return false;
  }

  utcTimestamp.tm_isdst = 0;
  const time_t clockEpoch = mktime(&utcTimestamp);

  if (clockEpoch <= 1700000000){
    return false;
  }

  const timeval systemTime = {clockEpoch, 0};

  return settimeofday(&systemTime, nullptr) == 0 && ready();
}

void ClockManager::update(bool stationAssociated, bool ipAddressAvailable)
{

  // Losing association resets sync work, but an address transition does not.
  if (!stationAssociated){
    if (ntpSyncRequested){
      ntpSyncRequested = false;
      httpTimeFallbackTried = false;
    }
    return;
  }

  if (!ipAddressAvailable){
    return;
  }

  if (!ntpSyncRequested){
    requestNtpSync();
  }

  if (ready()){
    // NTP may update asynchronously, so stop retrying as soon as the epoch is credible.
    return;
  }

  if (millis() - lastNtpAttemptAt >= Config::Timing::NtpRetry){
    requestNtpSync();
  }

  if (!httpTimeFallbackTried && ntpSyncRequested &&
      millis() - firstNtpAttemptAt >= Config::Timing::HttpTimeFallback){
    // Some captive or filtered networks block NTP while still allowing ordinary HTTP.
    httpTimeFallbackTried = true;
    appState.statusMessage = "Getting clock from web...";
                             appState.redraw = true;
    if (syncFromHttpDate()){
      appState.statusMessage = "Clock synced";
      appState.lastPollAt = 0;
    }

    else{
      appState.statusMessage = "Clock blocked: try hotspot";
    }
  }
  
}