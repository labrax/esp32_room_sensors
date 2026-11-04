#include "WifiConfig.h"

#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#include "wifi_pass.h"

// A compile-time pair longer than the WPA2 limits would be silently truncated
// into the active buffer, so fail the build instead.
static_assert(sizeof(WIFI_SSID) - 1 <= 32, "WIFI_SSID must be at most 32 bytes");
static_assert(sizeof(WIFI_PASSWORD) - 1 <= 63, "WIFI_PASSWORD must be at most 63 bytes");

static const char NVS_NAMESPACE[] = "wifi";

// WPA2 limits: SSID 32 bytes, passphrase 63 bytes. Fixes the buffer size, so
// the getters stay heap-free.
static char active_ssid[33];
static char active_password[64];
static bool provisioned = false;
static bool begun = false;

// The form and the saved pair are printable ASCII only: control, DEL, and every
// byte >= 0x80 are rejected. Non-ASCII SSIDs go in wifi_pass.h.
static bool has_non_printable_ascii(const char *s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x20 || c > 0x7e) {
      return true;
    }
  }
  return false;
}

void wifi_config_begin(void) {
  if (begun) {
    return;
  }
  begun = true;

  snprintf(active_ssid, sizeof(active_ssid), "%s", WIFI_SSID);
  snprintf(active_password, sizeof(active_password), "%s", WIFI_PASSWORD);

  // Read-write open: creating the empty namespace is silent, while a read-only
  // open of a missing namespace logs nvs_open failed on every fresh boot.
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    return;
  }

  // isKey() first: "not provisioned yet" is the normal state and must not
  // reach the Preferences error log on every boot.
  bool valid = prefs.isKey("valid") ? prefs.getBool("valid", false) : false;
  char ssid[sizeof(active_ssid)] = {0};
  char password[sizeof(active_password)] = {0};
  size_t ssid_len = prefs.isKey("ssid") ? prefs.getString("ssid", ssid, sizeof(ssid)) : 0;
  size_t password_len =
      prefs.isKey("pass") ? prefs.getString("pass", password, sizeof(password)) : 0;
  prefs.end();

  // Lengths include the terminating NUL; a missing or oversized value reads 0.
  // `valid` is the commit marker written last by wifi_config_save(), so a pair
  // interrupted mid-write (or an undersized password) is never trusted.
  if (!valid || ssid_len < 2 || ssid_len > sizeof(active_ssid) ||
      password_len < 9 || password_len > sizeof(active_password)) {
    return;
  }
  if (has_non_printable_ascii(ssid, ssid_len - 1) ||
      has_non_printable_ascii(password, password_len - 1)) {
    return;
  }

  strcpy(active_ssid, ssid);
  strcpy(active_password, password);
  provisioned = true;
}

const char *wifi_config_ssid(void) {
  return active_ssid;
}

const char *wifi_config_password(void) {
  return active_password;
}

bool wifi_config_is_provisioned(void) {
  return provisioned;
}

bool wifi_config_is_configured(void) {
  return provisioned || (active_ssid[0] != '\0' && strcmp(active_ssid, "YOUR_SSID") != 0);
}

static const char *validate(const char *ssid, const char *password) {
  if (ssid == NULL) {
    return "SSID is missing.";
  }
  size_t ssid_len = strlen(ssid);
  if (ssid_len == 0) {
    return "SSID must not be empty.";
  }
  if (ssid_len > 32) {
    return "SSID must be at most 32 bytes.";
  }
  if (has_non_printable_ascii(ssid, ssid_len)) {
    return "SSID must be printable ASCII only.";
  }
  if (password == NULL) {
    return "Password is missing.";
  }
  size_t password_len = strlen(password);
  if (password_len < 8) {
    return "Password must be at least 8 characters.";
  }
  if (password_len > 63) {
    return "Password must be at most 63 bytes.";
  }
  if (has_non_printable_ascii(password, password_len)) {
    return "Password must be printable ASCII only.";
  }
  return NULL;
}

// Persists for the next boot only: the reboot applies the pair, so a running
// try_ntp() never sees the buffers change under it.
bool wifi_config_save(const char *ssid, const char *password, const char **error) {
  const char *problem = validate(ssid, password);
  if (problem != NULL) {
    if (error != NULL) {
      *error = problem;
    }
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    if (error != NULL) {
      *error = "Could not open NVS storage.";
    }
    return false;
  }
  // putBool()/putString() return 0 when the write or its commit failed.
  // `valid` is cleared first and set last, so a failure anywhere leaves the
  // stored pair marked invalid and wifi_config_begin() falls back.
  bool saved = prefs.putBool("valid", false) == 1;
  if (saved) {
    saved = prefs.putString("ssid", ssid) != 0;
  }
  if (saved) {
    saved = prefs.putString("pass", password) != 0;
  }
  if (saved) {
    saved = prefs.putBool("valid", true) == 1;
  }
  prefs.end();

  if (!saved && error != NULL) {
    *error = "Could not save credentials.";
  }
  return saved;
}
