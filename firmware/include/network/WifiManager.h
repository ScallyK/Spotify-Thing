#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/event_groups.h>

#include "Models.h"

// WifiManager tracks AP association and DHCP readiness as separate states.
class WifiManager

{

public:

  explicit WifiManager(AppState &appState);

  // Register Wi-Fi event handling before the application starts station mode.
  void begin();

  // Update connection status and recover from a DHCP lease that never arrives.
  void update();

  bool isAssociated() const;
  bool hasIpAddress() const;
  bool dnsFallbackInstalled() const;
  bool installPublicDnsFallback();

private:

  static void onEvent(WiFiEvent_t event);
  void processEvent(WiFiEvent_t event);

  EventBits_t currentEventBits() const;

  static WifiManager *activeManager;

  // Event bits are used because Wi-Fi callbacks run outside the cooperative app loop.
  AppState &appState;
  EventGroupHandle_t eventGroup = nullptr;

  bool publicDnsInstalled = false;
  bool linkWasUp = false;

  uint32_t linkStartedAt = 0;
  uint32_t lastDhcpRenewAt = 0;

};