# Building Notes

This project expects a few local/private headers outside the repo and a compatible `dsmr2Lib` version.

## 1) Local `_secrets` headers (optional but expected by default)

Some source files include headers from:

`./../../_secrets/`

That resolves (relative to this project) to a sibling folder outside the repo. These files are used for local/private overrides and credentials.

### Expected files

1. `../../_secrets/posts.h`
2. `../../_secrets/energyid.h`
3. `../../_secrets/direct_ap.h`
4. `../../_secrets/tapelectric.h` (optional)

The code now compiles without them (safe defaults are provided), but if you want the related features you should create them.

## 2) Example `posts.h`

Create `../../_secrets/posts.h` with at least:

```cpp
#pragma once

// Optional suffix appended to OTAURL, e.g. "latest/" or ""
#define OTAURL_PREFIX ""

// Only needed when POST_POWERCH is enabled
#define URL_POWERCH "https://example.invalid/api/power"

// Only needed when POST_MEENT is enabled
#define URL_MEENT "https://meent.dev.muze.nl/api/data/"
```

Notes:
- `OTAURL_PREFIX` is used in `DSMRloggerAPI.h` to build `BaseOTAurl`.
- If `POST_POWERCH` is not enabled, `URL_POWERCH` is not used.
- If `POST_MEENT` is not enabled, `URL_MEENT` is not used.
- `POST_POWERCH` and `POST_MEENT` are mutually exclusive compile-time features.

## 3) Example `energyid.h`

Create `../../_secrets/energyid.h` with:

```cpp
#pragma once

#define EID_PROV_URL   "https://hooks.energyid.eu/hello"
#define EID_PROF_KEY   "replace-with-your-key"
#define EID_PROF_SECR  "replace-with-your-secret"
```

Notes:
- If you do not use EnergyID, the project can still compile without this file.
- When EnergyID is enabled in settings, these values must be valid.

## 4) Example `direct_ap.h`

Create `../../_secrets/direct_ap.h` for a closed-network firmware that connects directly to a customer AP:

```cpp
#pragma once

#define DIRECT_AP_CONNECT 1
#define DIRECT_AP_SSID_PREFIX "Vendor-"
#define DIRECT_AP_TARGET_SERIAL ""

#define DIRECT_AP_CONNECT_TIMEOUT_MS 45000
#define DIRECT_AP_SCAN_INTERVAL_MS 5000
#define DIRECT_AP_ENABLE_LOCAL_LOGS 0

#define DIRECT_AP_OTAURL_PREFIX "direct-ap/"
```

Notes:
- `DIRECT_AP_SSID_PREFIX` and `DIRECT_AP_TARGET_SERIAL` are compile-time settings; no filesystem config file is needed.
- `DIRECT_AP_TARGET_SERIAL` can be left empty to connect to the strongest matching AP.
- `DIRECT_AP_OTAURL_PREFIX` is used when `DIRECT_AP_CONNECT` is enabled, unless `OTAURL_PREFIX` is explicitly defined elsewhere.

## 5) Tap Electric meter push (optional)

Tap Electric is a runtime feature: there is no compile-time flag. Enable it and
configure it entirely from the WebUI Settings tab:

- `Tap Electric push` (on/off)
- `Tap Electric API key` (sent as the `X-Api-Key` header)
- `Tap Electric meter ID` (used in the request path)
- `Send Tap Electric data (sec.)` (interval, 1-30 s)

When enabled with a valid API key and meter ID, the dongle posts per-phase
meter data to `POST https://api.tapelectric.app/api/v1/meters/{meterId}/data`
off the realtime path via the worker queue.

The base URL can optionally be overridden for testing by creating
`../../_secrets/tapelectric.h`:

```cpp
#pragma once

// Defaults to "https://api.tapelectric.app" when this file is absent.
#define URL_TAPELECTRIC_BASE "https://api.tapelectric.app"
```

## 6) Frontend CDN (fork development)

The web UI is not served from the device; the device only caches a small index
shell (`/DSMRindexEDGE.html`) on LittleFS and pulls the rest of the frontend
(`DSMRindex.js`, `DSMRindex_body.html`, language files, CSS) from jsDelivr.

The CDN location is defined in two places that must stay in sync:

- Firmware index download: `CDN_FORK_REPO` / `CDN_FORK_REF` in `Config.h`
  (used to build `PATH_DATA_FILES`).
- Browser asset loading: `CDN_REPO` / `CDN_REF` in `cdn/cdn-config.js`
  (used to build `CDN_BASE`, which `DSMRindex.js` uses for language files).

Both currently point at `p-chodorowski/P1-Dongel-ESP32@main`.

To develop against your fork:

1. Set the fork repo/ref in both `Config.h` and `cdn/cdn-config.js`.
2. Push your `cdn/` and `data/DSMRindexEDGE.html` changes to the **public**
   GitHub fork (jsDelivr only serves public repos).
3. On the device, delete the cached `/DSMRindexEDGE.html` (via the file manager
   or telnet) and reboot. `EnsureIndexFilePresent()` in `FS.ino` only
   re-downloads the index when it is missing, so an old cached shell will keep
   loading until you remove it.
4. Hard-refresh the browser (Ctrl+F5) to bypass the browser cache.
5. jsDelivr caches branch content for a few minutes; verify a push is live by
   opening a file directly, e.g.
   `https://cdn.jsdelivr.net/gh/p-chodorowski/P1-Dongel-ESP32@main/cdn/lang/en.json`.

Note: device settings fields (including Tap Electric) come from firmware via
`/api/v2/dev/settings`, not from the CDN. The CDN only provides the JavaScript
and translated labels; new settings still require a firmware flash.

## 7) Profile selection

Do not hardcode hardware profile defines in `P1-Dongel-ESP32.ino` when using `build.sh`.

Use `build.sh` to compile all profiles. It injects profile-specific defines and board settings, including:

- ESP32-C3 builds must always use the `Minimal SPIFFS` partition scheme (`OTA 1.9MB / 128KB SPIFFS`).
- ESP32-S3 builds must always use the 8MB partition scheme (`FlashSize=8M`, `PartitionScheme=default_8MB`, OTA 3MB / matching 8MB layout).
- `ULTRA` -> `ESP32S3`, `FlashSize=8M`, `PartitionScheme=default_8MB`

## 8) Libraries used by this project

The codebase uses a mix of libraries from the ESP32 Arduino core and a small set of external libraries that must be installed separately.

### ESP32 Arduino core (installed via board package)

These headers come from the ESP32 board support package rather than a separate Arduino library install:

- `WiFi.h`, `WiFiClientSecure.h`, `HTTPClient.h`, `HTTPUpdate.h`, `Update.h`, `Preferences.h`, `LittleFS.h`, `ESPmDNS.h`, `AsyncUDP.h`
- `esp_wifi.h`, `esp_now.h`, `esp_sntp.h`, `esp_timer.h`, `esp_mac.h`, `esp_task_wdt.h`, `esp_chip_info.h`, `esp_system.h`, `esp_efuse.h`, `esp_efuse_table.h`, `rom/rtc.h`
- Repo: <https://github.com/espressif/arduino-esp32>

### External libraries (install separately)

- `ArduinoJson` (`ArduinoJson.h`)
  Repo: <https://github.com/bblanchon/ArduinoJson>
- `Time` (`TimeLib.h`)
  Repo: <https://github.com/PaulStoffregen/Time>
- `TelnetStream` (`TelnetStream.h`)
  Repo: <https://github.com/jandrassy/TelnetStream>
- `dsmr2Lib` (`dsmr2.h`)
  Repo: <https://github.com/mhendriks/dsmr2Lib>
- `WiFiManager` (`WiFiManager.h`)
  Repo: <https://github.com/tzapu/WiFiManager>
- `CRC32` (`CRC32.h`)
  Repo: <https://github.com/bakercp/CRC32>
- `eModbus` (`ModbusServerRTU.h`)
  Repo: <https://github.com/eModbus/eModbus>
- `micro-ecc` (`uECC.h`)
  Repo: <https://github.com/kmackay/micro-ecc>
- `ESPAsyncWebServer` (`ESPAsyncWebServer.h`)
  Repo: <https://github.com/ESP32Async/ESPAsyncWebServer>
- `AsyncTCP` (`AsyncTCP.h`)
  Repo: <https://github.com/ESP32Async/AsyncTCP>

### Local project headers

These are included by the sketch but live in this repository, so they are not extra dependencies:

- `safeTimers.h`
- `espnow.h`
