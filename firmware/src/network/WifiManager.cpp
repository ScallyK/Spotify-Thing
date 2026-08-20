#include "network/WifiManager.h"

#include <WiFi.h>
#include <esp_netif.h>
#include <lwip/dns.h>
#include <lwip/ip_addr.h>

#include "Config.h"
#include "secrets.h"

// Event bits preserve the distinction between joining an AP and receiving a DHCP address.
namespace
{
constexpr EventBits_t AssociatedBit = BIT0;
constexpr EventBits_t HasIpBit = BIT1;
}  // namespace

WifiManager* WifiManager::activeManager = nullptr;

WifiManager::WifiManager(AppState& appState) : appState(appState){}

void WifiManager::begin()
{
  // The Arduino event callback is static, so retain the active instance for dispatch.
  activeManager = this;
  eventGroup = xEventGroupCreate();
  WiFi.onEvent(onEvent);
}

void WifiManager::onEvent(WiFiEvent_t event)
{
  if (activeManager){
    activeManager->processEvent(event);
  }

}

void WifiManager::processEvent(WiFiEvent_t event)
{
  if (!eventGroup){
    return;
  }

  switch (event)
  {
    // Association alone does not prove that DHCP has completed.
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      xEventGroupSetBits(eventGroup, AssociatedBit);
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      xEventGroupSetBits(eventGroup, AssociatedBit | HasIpBit);
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      xEventGroupClearBits(eventGroup, HasIpBit);
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      xEventGroupClearBits(eventGroup, AssociatedBit | HasIpBit);
      break;

    default:
      break;

  }
}

EventBits_t WifiManager::currentEventBits() const
{
  return eventGroup ? xEventGroupGetBits(eventGroup) : 0;
}

bool WifiManager::isAssociated() const
{
  return (currentEventBits() & AssociatedBit) != 0;
}

bool WifiManager::hasIpAddress() const
{
  return (currentEventBits() & HasIpBit) != 0 || !(WiFi.localIP() == IPAddress(0, 0, 0, 0));
}

bool WifiManager::dnsFallbackInstalled() const
{
  return publicDnsInstalled;
}

void WifiManager::update()
{
  // Preserve the original reconnect policy while showing the stage users can act on.
  const bool stationAssociated = isAssociated();
  const bool ipAddressAvailable = hasIpAddress();

  if (stationAssociated && !linkWasUp){
    
    // Start DHCP timeout measurement at each newly associated link.
    linkWasUp = true;
    linkStartedAt = millis();
    publicDnsInstalled = false;

  }

  if (!stationAssociated){
    linkWasUp = false;

    if (appState.statusMessage != "Wi-Fi joining..."){
      appState.statusMessage = "Wi-Fi joining...";
      appState.redraw = true;
    }

    return;
  }

  if (!ipAddressAvailable){

    // Rejoin only after the DHCP grace period to avoid reconnect churn.
    if (millis() - linkStartedAt >= Config::Timing::DhcpRenew && millis() - lastDhcpRenewAt >= Config::Timing::DhcpRenew){

      lastDhcpRenewAt = millis();
      publicDnsInstalled = false;

      WiFi.disconnect(false, false);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

      appState.statusMessage = "Renewing DHCP...";

    }

    else{
      appState.statusMessage = "Wi-Fi getting IP...";
    }

    appState.redraw = true;

  }

}

bool WifiManager::installPublicDnsFallback()
{
  // Apply fallback servers to both ESP-IDF and lwIP resolver paths.
  esp_netif_t* stationInterface = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

  if (!stationInterface){
    return false;
  }

  esp_netif_dns_info_t dnsConfiguration = {};

  dnsConfiguration.ip.type = ESP_IPADDR_TYPE_V4;
  dnsConfiguration.ip.u_addr.ip4.addr = ESP_IP4TOADDR(1, 1, 1, 1);

  const esp_err_t primaryDnsResult =
      esp_netif_set_dns_info(stationInterface, ESP_NETIF_DNS_MAIN, &dnsConfiguration);

  dnsConfiguration.ip.u_addr.ip4.addr = ESP_IP4TOADDR(8, 8, 8, 8);

  const esp_err_t backupDnsResult =
      esp_netif_set_dns_info(stationInterface, ESP_NETIF_DNS_BACKUP, &dnsConfiguration);

  ip_addr_t cloudflare = {}, google = {};

  IP_ADDR4(&cloudflare, 1, 1, 1, 1);
  IP_ADDR4(&google, 8, 8, 8, 8);

  dns_setserver(0, &cloudflare);
  dns_setserver(1, &google);

  publicDnsInstalled = primaryDnsResult == ESP_OK && backupDnsResult == ESP_OK;
  Serial.printf("DNS fallback: main=%d backup=%d\n", primaryDnsResult, backupDnsResult);

  return publicDnsInstalled;

}