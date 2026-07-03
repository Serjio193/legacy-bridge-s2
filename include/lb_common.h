#pragma once

#include <Arduino.h>
#include <Preferences.h>

static constexpr uint32_t LB_CONFIG_MAGIC = 0x4C425332UL;
static constexpr uint16_t LB_CONFIG_VERSION = 1;
static constexpr const char *LB_PREFS_NS = "lb2";
static constexpr const char *LB_DEFAULT_DEVICE_PREFIX = "apa102";
static constexpr const char *LB_DEFAULT_AP_PASS_PREFIX = "APA102";
static constexpr const char *LB_DEFAULT_PACK_BASE_URL =
    "https://serjio193.github.io/lg_apa102/latest/";
static constexpr uint32_t LB_RECOVERY_DOUBLE_RESET_MS = 5000;

static inline String lbDeviceMacHex() {
  const uint64_t mac = ESP.getEfuseMac();
  char text[13];
  snprintf(text, sizeof(text), "%04X%08X",
           static_cast<uint16_t>(mac >> 32),
           static_cast<uint32_t>(mac));
  return String(text);
}

static inline String lbDefaultDeviceName() {
  String name(LB_DEFAULT_DEVICE_PREFIX);
  name += lbDeviceMacHex();
  name.toLowerCase();
  return name;
}

static inline String lbDefaultApSsid() {
  return lbDefaultDeviceName();
}

static inline String lbDefaultApPass() {
  const String mac = lbDeviceMacHex();
  String pass(LB_DEFAULT_AP_PASS_PREFIX);
  pass += mac.substring(mac.length() >= 4 ? mac.length() - 4 : 0);
  return pass;
}

struct LBConfig {
  uint32_t magic;
  uint16_t version;
  char deviceName[33];
  char wifiSsid[33];
  char wifiPass[65];
  char packBaseUrl[161];
  int16_t dataPin;
  int16_t clockPin;
  int16_t oePin;
  int16_t powerPin;
  uint16_t ledCount;
  uint8_t ledBrightness;
  uint8_t oeActiveLow;
  uint8_t powerActiveHigh;
  uint16_t powerOnDelayMs;
  uint16_t powerOffDelayMs;
};

static inline void lbSetDefaults(LBConfig *cfg) {
  if (!cfg) return;
  memset(cfg, 0, sizeof(*cfg));
  cfg->magic = LB_CONFIG_MAGIC;
  cfg->version = LB_CONFIG_VERSION;
  const String defaultName = lbDefaultDeviceName();
  strlcpy(cfg->deviceName, defaultName.c_str(), sizeof(cfg->deviceName));
  strlcpy(cfg->packBaseUrl, LB_DEFAULT_PACK_BASE_URL, sizeof(cfg->packBaseUrl));
  cfg->dataPin = 7;
  cfg->clockPin = 9;
  cfg->oePin = 11;
  cfg->powerPin = 3;
  cfg->ledCount = 60;
  cfg->ledBrightness = 24;
  cfg->oeActiveLow = 0;
  cfg->powerActiveHigh = 1;
  cfg->powerOnDelayMs = 50;
  cfg->powerOffDelayMs = 100;
}

static inline void lbNormalizeDefaults(LBConfig *cfg) {
  if (!cfg) return;
  cfg->magic = LB_CONFIG_MAGIC;
  cfg->version = LB_CONFIG_VERSION;
  if (cfg->deviceName[0] == '\0' || strcmp(cfg->deviceName, "lg_apa102") == 0) {
    const String defaultName = lbDefaultDeviceName();
    strlcpy(cfg->deviceName, defaultName.c_str(), sizeof(cfg->deviceName));
  }
  if (cfg->packBaseUrl[0] == '\0') {
    strlcpy(cfg->packBaseUrl, LB_DEFAULT_PACK_BASE_URL, sizeof(cfg->packBaseUrl));
  }
  if (cfg->ledCount == 0) cfg->ledCount = 1;
  if (cfg->ledBrightness == 0) cfg->ledBrightness = 1;
}

static inline bool lbLoadConfig(LBConfig *cfg) {
  if (!cfg) return false;
  lbSetDefaults(cfg);
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, true);
  LBConfig stored{};
  size_t len = prefs.getBytes("cfg", &stored, sizeof(stored));
  prefs.end();
  if (len == sizeof(stored) && stored.magic == LB_CONFIG_MAGIC && stored.version == LB_CONFIG_VERSION) {
    *cfg = stored;
    lbNormalizeDefaults(cfg);
    return true;
  }
  return false;
}

static inline void lbSaveConfig(const LBConfig *cfg) {
  if (!cfg) return;
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.putBytes("cfg", cfg, sizeof(*cfg));
  prefs.end();
}

static inline void lbLoadApiPin(char *pin, size_t pinSize) {
  if (!pin || pinSize == 0) return;
  pin[0] = '\0';
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, true);
  String stored = prefs.getString("api_pin", "");
  prefs.end();
  strlcpy(pin, stored.c_str(), pinSize);
}

static inline void lbSaveApiPin(const char *pin) {
  Preferences prefs;
  prefs.begin(LB_PREFS_NS, false);
  prefs.putString("api_pin", pin ? pin : "");
  prefs.end();
}

static inline bool lbIsValidApiPin(const String &pin) {
  if (pin.length() == 0) return true;
  if (pin.length() < 4 || pin.length() > 16) return false;
  for (size_t i = 0; i < pin.length(); ++i) {
    const char c = pin[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return false;
  }
  return true;
}

static inline bool lbIsValidPackBaseUrl(const char *url) {
  if (!url || url[0] == '\0') return false;
  const String value(url);
  return (value.startsWith("https://") || value.startsWith("http://")) &&
         value.length() < 161;
}

static inline String lbJsonEscape(const char *value) {
  String escaped;
  if (!value) return escaped;
  escaped.reserve(strlen(value) + 8);
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
    switch (*p) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (*p < 0x20) {
          char encoded[7];
          snprintf(encoded, sizeof(encoded), "\\u%04x", *p);
          escaped += encoded;
        } else {
          escaped += static_cast<char>(*p);
        }
    }
  }
  return escaped;
}

static inline bool lbIsValidOutputPin(int16_t pin) {
  return pin < 0 || GPIO_IS_VALID_OUTPUT_GPIO(static_cast<gpio_num_t>(pin));
}

static inline String lbHostnameFromName(const char *name) {
  String host = name ? String(name) : lbDefaultDeviceName();
  host.trim();
  if (host.length() == 0) host = lbDefaultDeviceName();
  host.toLowerCase();
  for (size_t i = 0; i < host.length(); ++i) {
    char c = host[i];
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-') host.setCharAt(i, '-');
  }
  return host;
}

static inline bool lbValidateConfig(const LBConfig *cfg) {
  if (!cfg) return false;
  if (cfg->deviceName[0] == '\0') return false;
  if (cfg->ledCount < 1 || cfg->ledCount > 2048) return false;
  if (cfg->ledBrightness < 1 || cfg->ledBrightness > 31) return false;
  if (!lbIsValidPackBaseUrl(cfg->packBaseUrl)) return false;
  if (!lbIsValidOutputPin(cfg->dataPin) || !lbIsValidOutputPin(cfg->clockPin) ||
      !lbIsValidOutputPin(cfg->oePin) || !lbIsValidOutputPin(cfg->powerPin)) {
    return false;
  }
  if (cfg->dataPin >= 0 && cfg->clockPin >= 0 && cfg->dataPin == cfg->clockPin) return false;
  if (cfg->dataPin >= 0 && cfg->oePin >= 0 && cfg->dataPin == cfg->oePin) return false;
  if (cfg->dataPin >= 0 && cfg->powerPin >= 0 && cfg->dataPin == cfg->powerPin) return false;
  if (cfg->clockPin >= 0 && cfg->oePin >= 0 && cfg->clockPin == cfg->oePin) return false;
  if (cfg->clockPin >= 0 && cfg->powerPin >= 0 && cfg->clockPin == cfg->powerPin) return false;
  if (cfg->oePin >= 0 && cfg->powerPin >= 0 && cfg->oePin == cfg->powerPin) return false;
  return true;
}

static inline void lbCopyArg(char *dst, size_t dstSize, const String &value) {
  if (!dst || dstSize == 0) return;
  size_t n = value.length();
  if (n >= dstSize) n = dstSize - 1;
  memcpy(dst, value.c_str(), n);
  dst[n] = '\0';
}

static inline int16_t lbArgPin(WebServer &server, const char *name, int16_t fallback) {
  if (!server.hasArg(name)) return fallback;
  return static_cast<int16_t>(server.arg(name).toInt());
}

static inline uint16_t lbArgU16(WebServer &server, const char *name, uint16_t fallback, uint16_t minValue, uint16_t maxValue) {
  if (!server.hasArg(name)) return fallback;
  long v = server.arg(name).toInt();
  if (v < static_cast<long>(minValue)) v = minValue;
  if (v > static_cast<long>(maxValue)) v = maxValue;
  return static_cast<uint16_t>(v);
}

static inline uint8_t lbArgU8(WebServer &server, const char *name, uint8_t fallback, uint8_t minValue, uint8_t maxValue) {
  if (!server.hasArg(name)) return fallback;
  long v = server.arg(name).toInt();
  if (v < static_cast<long>(minValue)) v = minValue;
  if (v > static_cast<long>(maxValue)) v = maxValue;
  return static_cast<uint8_t>(v);
}
