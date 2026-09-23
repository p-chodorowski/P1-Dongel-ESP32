# Building Notes

This project expects a few local/private headers outside the repo and a compatible `dsmr3Lib` version.

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

// Optional override when POST_MEENT is enabled. By default DEBUG uses the
// MEENT staging API and release firmware uses the production API.
#define MEENT_API_BASE_URL "https://meent.dev.muze.nl/api/"

// Only needed when POST_KEMP is enabled
#define URL_KEMP "https://example.invalid/api/data"
#define KEMP_API_KEY "replace-with-your-api-key"
```

Notes:
- `OTAURL_PREFIX` is used in `DSMRloggerAPI.h` to build `BaseOTAurl`.
- If `POST_POWERCH` is not enabled, `URL_POWERCH` is not used.
- If `POST_MEENT` is not enabled, `MEENT_API_BASE_URL` is not used.
- If `POST_KEMP` is not enabled, `URL_KEMP` and `KEMP_API_KEY` are not used.
- `POST_POWERCH`, `POST_MEENT`, and `POST_KEMP` are mutually exclusive compile-time features.
- `POST_KEMP` uses a fixed 60-second POST interval and the OTA suffix `kemp/` (for example `p1p/v5/kemp/`).

## MEENT provisioning secret

MEENT builds create a cryptographically random 32-byte client secret before the
first pod/API-key request. It is stored in NVS (separate from the settings
file) and sent only to `/api/pod/` and `/api/register/` in the
`X-Client-Secret` header. The MEENT provider should store a hash of this secret
and use it to make interrupted provisioning requests idempotent. Override the
header name with `MEENT_CLIENT_SECRET_HEADER` in `posts.h` if the provider
chooses another name. A factory reset removes the secret deliberately.

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

The CDN tag is the full Ultra version, including the fork digit (`CDN_FORK_REF`
in `Config.h`, derived from `version.h`). Firmware `5.9.5.4` loads
`p-chodorowski/P1-Dongel-ESP32@5.9.5.4`. Do not reuse the vendor tag `5.9.5`;
that tag is already published.

- `cdn/cdn-config.js` reads that tag from its own script URL.
- `data/DSMRindexEDGE.html` is the shell cached on the device. After a
  successful download of the matching tag, the firmware rewrites every
  `p-chodorowski/P1-Dongel-ESP32@...` URL to the running release and sends
  `Cache-Control: no-store`.
- `EnsureIndexFilePresent()` replaces the cached shell when its UI ref does
  not match the firmware. The previous shell is kept if the versioned URL
  cannot be downloaded.

### Branching model

This fork uses a single integration branch, `main`. Feature work lands on
`main`; each shipped frontend is identified by an immutable Git tag matching the
firmware version in `version.h`.

Do **not** point devices at a floating branch ref such as `@main`. jsDelivr
caches mutable refs aggressively, so `@main` can serve stale assets (a stale
`@main` once shipped a `DSMRindex.js` without `TAP_KEYS`, which dropped the Tap
Electric fields into the wrong settings tab). Always pin to a tag.

### Releasing a new frontend

1. Land all `cdn/` and `data/DSMRindexEDGE.html` changes on `main` and bump
   `_VERSION_FORK` in `version.h` (e.g. `5.9.5.4`). Keep the `@<ver>` URLs in
   `data/DSMRindexEDGE.html` on that same full version tag.
2. Commit, then tag and push to the **public** GitHub fork (jsDelivr only serves
   public repos). Do this before devices running that firmware boot, otherwise
   the UI download fails and the previous shell stays in place:

   ```bash
   git tag 5.9.5.4
   git push origin main 5.9.5.4
   ```

3. Verify jsDelivr is serving the tag before flashing. Open the assets
   directly and confirm the expected content is present:

   - `https://cdn.jsdelivr.net/gh/p-chodorowski/P1-Dongel-ESP32@5.9.5.4/cdn/DSMRindex.js`
     should contain `TAP_KEYS`.
   - `https://cdn.jsdelivr.net/gh/p-chodorowski/P1-Dongel-ESP32@5.9.5.4/cdn/DSMRindex_body.html`
     should contain `settings_tapelectric`.

4. Flash or OTA. On boot the device replaces `/DSMRindexEDGE.html` when the
   stored UI ref differs from the firmware. After a successful update the page
   reloads itself. A browser that still has the previous UI open needs one
   refresh if that UI predates this reload behavior. In the Settings panel, the
   Tap Electric tab should render four fields;
   `document.querySelectorAll('#settings_tapelectric .settingDiv').length`
   returns `4` in the browser console.

Because tags are immutable, a given tag URL is fetched and cached by jsDelivr
once and never goes stale, so no purge step is needed.

Note: device settings fields (including Tap Electric) come from firmware via
`/api/v2/dev/settings`, not from the CDN. The CDN only provides the JavaScript
and translated labels; new settings still require a firmware flash.

## 7) Profile selection

Do not hardcode hardware profile defines in `P1-Dongel-ESP32.ino` when using `build.sh`.

Use `build.sh` to compile all profiles by default:

```bash
./build.sh
```

You can also compile one or more selected profiles:

```bash
./build.sh ULTRA
./build.sh P1P ETH_P1EP
```

Add `--clear-cache` or `--clean` to remove the selected profile build directories before compiling:

```bash
./build.sh --clear-cache ULTRA
```

Extra Arduino build properties can be passed through as well. For `compiler.cpp.extra_flags`, the value is appended to the profile flags:

```bash
./build.sh ULTRA --extra-flags "-DWEBSOCKETS_TCP_TIMEOUT=2000"
./build.sh ULTRA --build-property compiler.cpp.extra_flags="-DWEBSOCKETS_TCP_TIMEOUT=2000"
```

### Sketch-local compiler options

The ESP32 Arduino core reads compiler options from `build_opt.h` in the sketch root. This project uses it to disable C++ exceptions without changing the globally installed ESP32 platform:

```text
-fno-exceptions
```

Arduino IDE and `arduino-cli` both apply this file automatically. A `platform.local.txt` in the sketch root is not supported; that file is only read next to the installed ESP32 `platform.txt`. Unlike `platform.local.txt`, `build_opt.h` is project-specific and remains part of the repository when the ESP32 core is updated.

The script injects profile-specific defines and board settings, including:

- ESP32-C3 builds must always use the `Minimal SPIFFS` partition scheme (`OTA 1.9MB / 128KB SPIFFS`).
- ESP32-S3 builds must always use the 8MB partition scheme (`FlashSize=8M`, `PartitionScheme=default_8MB`, OTA 3MB / matching 8MB layout).
- `ULTRA` -> `ESP32S3`, `FlashSize=8M`, `PartitionScheme=default_8MB`

When compiling this sketch from Arduino IDE 2 (with `#define ULTRA` enabled), set **Tools** to the ULTRA list in `P1-Dongel-ESP32.ino` / `sketch.yaml`. Arduino IDE 2.3 does not apply `sketch.yaml` to the Tools menu; it remembers the last Tools choices per sketch. A 4MB flash size with the 8MB partition table will boot-loop (`partition 3 invalid ... exceeds flash chip size 0x400000`).

`arduino-cli compile` / `upload` without `--fqbn` uses `default_fqbn` from `sketch.yaml` (ULTRA, 8MB).

## 8) Custom Ultra OTA URL

Ultra builds default `BaseOTAurl` to our directory even when `POST_MEENT` is enabled (vendor `…/p1u/v5/me/` is not used on Ultra):

```
http://209.38.55.197/p1dongle/ultra/
```

End-user **Update url** (settings field, without `http://`):

```
209.38.55.197/p1dongle/ultra/
```

Do not append `p1u/` or `v5/`. A **test** directory lives at:

```
http://209.38.55.197/p1dongle/ultra/test/
```

Production firmware keeps the production URL. Point a dev Ultra at the test directory with Settings → Update url `209.38.55.197/p1dongle/ultra/test/`, or compile with `-DOTA_TEST_CHANNEL` / `python3 tools/compile_ultra.py --test-channel`. Override any URL with `-DOTA_BASE_URL="http://host/path/"` (keep the trailing slash).

### Cloud compile (GitHub Actions)

The **Compile Ultra** workflow (`.github/workflows/compile-ultra.yml`) builds the Ultra sketch using `sketch.yaml` `default_fqbn` (ESP32-S3, `FlashSize=8M`, `PartitionScheme=default_8MB`, `FlashMode=qio`, `CPUFreq=240`, `CDCOnBoot=default`, `PSRAM=disabled`) and ESP32 core **3.3.11**. It installs `dsmr3Lib` (not `dsmr2Lib`) plus the other sketch libraries and artifacts `version-manifest.json` plus `DSMR-API-V{version}_8Mb.bin`. It does **not** FTP or USB-flash. If the `OTA_SFTP_*` GitHub Secrets are set, it then SFTPs those two files to the droplet; if they are missing the upload step is skipped and compile stays green.

1. GitHub → Actions → **Compile Ultra** → Run workflow.
2. Leave **ota_channel** = `production` (default URL) or choose `test` to bake `.../ultra/test/`.
3. Download the `ultra-ota-production` or `ultra-ota-test` artifact (always).
4. When secrets are configured, CI also SFTPs the same files. Locally, after a compile:

```
OTA_SFTP_HOST=209.38.55.197
OTA_SFTP_USER=<ssh-user>
OTA_SFTP_PATH=/var/www/html/p1dongle/ultra
python3 tools/sftp_ota.py --dir dist/ultra
python3 tools/sftp_ota.py --dir dist/ultra --channel test --dry-run
```

`OTA_SFTP_PATH` is the filesystem directory Apache/nginx serves as `http://209.38.55.197/p1dongle/ultra/`. Test files go to `$OTA_SFTP_PATH/test/` unless `OTA_SFTP_PATH_TEST` is set. Auth is SSH key (`OTA_SFTP_IDENTITY` / secret `OTA_SFTP_KEY`) or secret `OTA_SFTP_PASSWORD`. Never commit those values.

Pushes and pull requests also run a production compile.

Local equivalent (needs `arduino-cli`):

```
python3 tools/compile_ultra.py --out dist/ultra
python3 tools/compile_ultra.py --out dist/ultra-test --test-channel
```

Or stage an already-compiled `.bin`:

```
python3 tools/publish_ota.py --firmware path/to/compiled.bin --out dist/ultra
```

This fork is on vendor `5.9.5`; bump `_VERSION_FORK` in `version.h` for each Ultra OTA release (`5.9.5.1`, `5.9.5.2`, …).

Cloud agents and GitHub Actions **cannot USB-flash** a dongle. Flash the downloaded `.bin` locally (Arduino IDE / `esptool` / web installer) if you need a first image on hardware.

`BaseOTAurl` is 96 bytes (`BASE_OTA_URL_SIZE` in `profile.h`).

## 9) Libraries used by this project

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
- `dsmr3Lib` (`dsmr3.h`)
  Repo: <https://github.com/mhendriks/dsmr3Lib>
  DSMR-API 5.9.0 and newer require dsmr3Lib 1.0.0 or a newer compatible
  release. The firmware uses the v3-only `P1FieldWarning`, `P1Diagnostics` and
  `CompleteRaw(String&)` APIs; dsmr2Lib cannot be substituted without
  reverting those integrations.
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
