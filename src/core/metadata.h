#pragma once

namespace meta {

    // System Branding & Platform Identity
    constexpr const char* FW_NAME        = "QRPickle";
    constexpr const char* FW_VERSION     = "v0.1.8";
    constexpr const char* HARDWARE_PLAT  = "ESP32 CYD (2.8\" TFT)";

    // Project Ownership & Operational Credits
    constexpr const char* AUTHOR_NAME    = "Bharath M. Palavalli";
    constexpr const char* AUTHOR_CALL    = "VU3GLJ";
    constexpr const char* SUPPORT_EMAIL  = "vu3glj@disroot.org";

    // Cloud OTA Target Repository
    constexpr const char* GITHUB_REPO    = "bmp/QRPickle";

    // Build Telemetry (Evaluated automatically by the compiler on every build)
    constexpr const char* BUILD_DATE     = __DATE__;
    constexpr const char* BUILD_TIME     = __TIME__;

} // namespace meta
