// Copy this file to wifi_pass.h and fill in your credentials.
// wifi_pass.h is ignored by git.
//
// WIFI_* is the router (STA) used for NTP. It is the fallback only: once the
// AP page form saves credentials to NVS, the saved pair wins. The form accepts
// printable ASCII only, so set a non-ASCII SSID here; it must be valid UTF-8 so
// /api/status stays valid JSON.
// AP_* is the access point the board runs at boot; it is compile-time only and
// WPA2, so AP_PASSWORD must be at least 8 characters.
// There is no safe default password: set your own. The example value below is
// public knowledge and only exists so the sketch compiles.
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

#define AP_SSID "esp32-room-sensors"
#define AP_PASSWORD "change-me-please" // >= 8 characters (WPA2); set your own
