#pragma once

#include <Arduino.h>

#include "Models.h"

// ClockManager supplies a TLS-safe clock from the build time, NTP, or an HTTP Date header.
class ClockManager
{
  
 public:

  explicit ClockManager(AppState& appState);

  // Seed from the build timestamp before the first HTTPS request needs a valid epoch.
  bool begin();

  // Keep AP association distinct from DHCP readiness so an address loss does not reset NTP.
  void update(bool stationAssociated, bool ipAddressAvailable);

  bool ready() const;

 private:

  // HTTP is a last-resort time source when NTP remains unavailable after its retry window.
  bool syncFromHttpDate();
  void requestNtpSync();

  AppState& appState;

  bool ntpSyncRequested = false;
  bool httpTimeFallbackTried = false;
  bool seededFromBuild = false;

  uint32_t lastNtpAttemptAt = 0;
  uint32_t firstNtpAttemptAt = 0;
};