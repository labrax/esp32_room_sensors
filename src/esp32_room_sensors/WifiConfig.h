#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H
#include <stddef.h>

// Loads the active STA credentials once. Saved NVS values override the
// WIFI_SSID/WIFI_PASSWORD fallback from wifi_pass.h. Call from setup() before
// status_server_begin()/init_time().
void wifi_config_begin(void);

// Active credentials used by try_ntp(). Never NULL.
const char *wifi_config_ssid(void);
const char *wifi_config_password(void);

// True when NVS supplied the active credentials (the AP form was used).
bool wifi_config_is_provisioned(void);

// True when try_ntp() can use the active credentials: either the AP form saved
// them, or the compile-time fallback is filled in (non-empty, not "YOUR_SSID").
bool wifi_config_is_configured(void);

// Validates and persists credentials for the NEXT boot. Does not mutate the
// active RAM copy; the caller restarts. Both values must be printable ASCII.
// On failure returns false and points *error (when non-NULL) at a static
// message.
bool wifi_config_save(const char *ssid, const char *password, const char **error);

#endif
