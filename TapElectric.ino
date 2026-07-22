/*
***************************************************************************
**  Program  : TapElectric, part of DSMRloggerAPI
**  Purpose  : Push per-phase meter data to the Tap Electric platform
**             (external meters API, e.g. for dynamic load balancing).
**             POST https://api.tapelectric.app/api/v1/meters/{meterId}/data
**             Auth via the X-Api-Key header. Both meter ID and API key are
**             configured at runtime through the WebUI settings.
**
**  Tap API body contract (L1-only, single-phase production default):
**    timestamp  UTC ISO-8601 with ms and Z suffix (system clock via NTP,
**               not P1 meter time), e.g. 2026-07-22T09:58:17.204Z
**    perPhase.l1  always present with voltage, current, and power
**               (zero values are sent, not omitted). L2/L3 are not sent.
***************************************************************************
*/

#if __has_include("./../../_secrets/tapelectric.h")
  #include "./../../_secrets/tapelectric.h"
#endif

#ifndef URL_TAPELECTRIC_BASE
  #define URL_TAPELECTRIC_BASE "https://api.tapelectric.app"
#endif

#include <sys/time.h>

static WiFiClientSecure tapTlsClient;
static uint8_t  tapPostErrors = 0;
static uint32_t tapLastPostMs = 0;
static bool     tapPostPending = false;

static constexpr size_t kTapMonitorCapacity = 30;
static constexpr size_t kTapMonitorBodyMax = 384;

struct TapMonitorEntry {
  char timestamp[20];
  char body[kTapMonitorBodyMax];
  int16_t httpStatus;  // -1 = connection/begin failed
};

static TapMonitorEntry g_tapMonitorEntries[kTapMonitorCapacity];
static size_t g_tapMonitorCount = 0;
static size_t g_tapMonitorHead = 0;

void logTapMonitorEntry(const char* body, int16_t httpStatus) {
  if (!bTapMonitor) return;

  TapMonitorEntry& entry = g_tapMonitorEntries[g_tapMonitorHead];
  strlcpy(entry.timestamp, actTimestamp, sizeof(entry.timestamp));
  strlcpy(entry.body, body, sizeof(entry.body));
  entry.httpStatus = httpStatus;

  g_tapMonitorHead = (g_tapMonitorHead + 1) % kTapMonitorCapacity;
  if (g_tapMonitorCount < kTapMonitorCapacity) g_tapMonitorCount++;
}

String tapMonitorJson() {
  JsonDocument doc;
  doc["enabled"] = bTapMonitor;
  doc["capacity"] = kTapMonitorCapacity;
  doc["count"] = g_tapMonitorCount;

  JsonArray data = doc["data"].to<JsonArray>();
  for (size_t i = 0; i < g_tapMonitorCount; i++) {
    const size_t index = (g_tapMonitorHead + kTapMonitorCapacity - 1 - i) % kTapMonitorCapacity;
    const TapMonitorEntry& entry = g_tapMonitorEntries[index];
    JsonObject row = data.add<JsonObject>();
    row["timestamp"] = entry.timestamp;
    row["body"] = entry.body;
    row["status"] = entry.httpStatus;
  }

  String body;
  serializeJson(doc, body);
  return body;
}

void clearTapMonitorEntries() {
  g_tapMonitorCount = 0;
  g_tapMonitorHead = 0;
}

static void tapBuildTimestamp(char* out, size_t outLen) {
  struct timeval tv = {};
  if (gettimeofday(&tv, nullptr) != 0) {
    time_t now = time(nullptr);
    struct tm* utc = gmtime(&now);
    if (!utc) {
      out[0] = '\0';
      return;
    }
    snprintf(out, outLen, "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
             utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday,
             utc->tm_hour, utc->tm_min, utc->tm_sec);
    return;
  }

  struct tm* utc = gmtime(&tv.tv_sec);
  if (!utc) {
    out[0] = '\0';
    return;
  }

  snprintf(out, outLen, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
           utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday,
           utc->tm_hour, utc->tm_min, utc->tm_sec,
           (long)(tv.tv_usec / 1000));
}

String JsonTapElectric(const WorkerTapPayload& payload) {
  JsonDocument doc;
  doc["timestamp"] = payload.timestamp;
  JsonObject perPhase = doc["perPhase"].to<JsonObject>();
  JsonObject l1 = perPhase["l1"].to<JsonObject>();
  l1["voltage"] = payload.voltage / 1000.0;
  l1["current"] = payload.current / 1000.0;
  l1["power"] = payload.power;

  String output;
  serializeJson(doc, output);

#ifdef DEBUG
  Debugf("TapElectric Json: %s\n", output.c_str());
#endif

  return output;
}

void PostTapElectric() {
  if (!bTapEnabled || bNewTelegramTap == false) return;
  if (netw_state == NW_NONE || tapPostErrors > 100 || tapPostPending) return;
  if (!strlen(settingTapApiKey) || !strlen(settingTapMeterId)) return;

  const uint16_t effectiveInterval = constrain(settingTapInterval, (uint16_t)1, (uint16_t)30);
  const uint32_t intervalMs = (uint32_t)effectiveInterval * 1000UL;
  const uint32_t nowMs = millis();
  if (tapLastPostMs != 0 && (uint32_t)(nowMs - tapLastPostMs) < intervalMs) return;

  WorkerTapPayload payload = {};
  tapBuildTimestamp(payload.timestamp, sizeof(payload.timestamp));
  if (!payload.timestamp[0]) return;

  // L1 only. Fall back to aggregate power when the meter has no per-phase OBIS.
  if (DSMRdata.power_delivered_l1_present || DSMRdata.power_returned_l1_present) {
    payload.power = DSMRdata.power_delivered_l1.int_val() - DSMRdata.power_returned_l1.int_val();
  } else {
    payload.power = DSMRdata.power_delivered.int_val() - DSMRdata.power_returned.int_val();
  }
  if (DSMRdata.voltage_l1_present) payload.voltage = DSMRdata.voltage_l1.int_val();
  if (DSMRdata.current_l1_present) payload.current = DSMRdata.current_l1.int_val();

  if (!WorkerEnqueueTapPost(payload)) return;

  bNewTelegramTap = false;
  tapPostPending = true;
}

void PostTapElectricFromWorker(const WorkerTapPayload& payload) {
  if (netw_state == NW_NONE || tapPostErrors > 100) {
    tapPostPending = false;
    return;
  }
  if (!strlen(settingTapApiKey) || !strlen(settingTapMeterId)) {
    tapPostPending = false;
    return;
  }

  String url = String(URL_TAPELECTRIC_BASE) + "/api/v1/meters/" + settingTapMeterId + "/data";
  String jsonBody = JsonTapElectric(payload);

  HTTPClient http;
  if (http.begin(tapTlsClient, url)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Api-Key", settingTapApiKey);

    int httpResponseCode = http.POST(jsonBody);
    DebugT(F("TapElectric HTTP Response code: ")); Debugln(httpResponseCode);
    logTapMonitorEntry(jsonBody.c_str(), httpResponseCode);

    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      tapPostErrors = 0;
      tapLastPostMs = millis();
    } else {
      tapPostErrors++;
      tapTlsClient.stop();
      delay(10);
    }
    http.end();
  } else {
    tapPostErrors++;
    DebugTln(F("TapElectric HTTP begin failed"));
    logTapMonitorEntry(jsonBody.c_str(), -1);
    tapTlsClient.stop();
    delay(10);
  }

  tapPostPending = false;
}

void StartTapElectric() {
  tapTlsClient.setInsecure();
  tapTlsClient.setTimeout(5000);
}
