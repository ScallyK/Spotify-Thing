#pragma once

#include <Arduino.h>

// Hardware assignments and timing values live here so behavior stays visible in one place.
namespace Config
{
    
    namespace Pins
    {

        // GPIO assignments match the physical controls and ST7735 display wiring.
        constexpr uint8_t TftCs = 5;
        constexpr uint8_t TftDc = 21;
        constexpr uint8_t TftRst = 22;
        constexpr uint8_t LeftClk = 32;
        constexpr uint8_t LeftDt = 25;
        constexpr uint8_t LeftSw = 26;
        constexpr uint8_t RightClk = 33;
        constexpr uint8_t RightDt = 27;
        constexpr uint8_t RightSw = 19;

    } // namespace Pins

    namespace Display
    {
        // These dimensions are the display coordinates used by every drawing routine.
        constexpr uint16_t Width = 160;
        constexpr uint16_t Height = 128;

    } // namespace Display

    namespace Timing
    {

        // All intervals are milliseconds and intentionally match the original control behavior.
        constexpr uint32_t Poll = 3500;
        constexpr uint32_t DeviceCache = 15000;
        constexpr uint32_t VolumeDelay = 200;
        constexpr uint32_t ActionDelay = 150;
        constexpr uint32_t ControlRefreshDelay = 750;
        constexpr uint32_t ProgressRedraw = 250;
        constexpr uint32_t NtpRetry = 30000;
        constexpr uint32_t HttpTimeFallback = 45000;
        constexpr uint32_t DhcpRenew = 20000;

    } // namespace Timing

    namespace Spotify
    {
        // Cap parsed devices to a predictable amount of ESP32 memory.
        constexpr uint8_t MaxDevices = 10;
    }
} // namespace Config