/*
***************************************************************************
**  Program  : TapElectric, part of DSMRloggerAPI
**  Purpose  : Push per-phase meter data to the Tap Electric platform
**             (external meters API, e.g. for dynamic load balancing).
**             POST https://api.tapelectric.app/api/v1/meters/{meterId}/data
**             Auth via the X-Api-Key header. Both meter ID and API key are
**             configured at runtime through the WebUI settings.
***************************************************************************
*/

#if __has_include("./../../_secrets/tapelectric.h")
  #include "./../../_secrets/tapelectric.h"
#endif

#ifndef URL_TAPELECTRIC_BASE
  #define URL_TAPELECTRIC_BASE "https://api.tapelectric.app"
#endif

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
  const char* ts = (DSMRdata.timestamp_present && DSMRdata.timestamp.length() >= 12)
                     ? DSMRdata.timestamp.c_str()
                     : actTimestamp;
  if (strlen(ts) < 12) {
    strlcpy(out, ts, outLen);
    return;
  }

  struct tm t = {};
  t.tm_year = (ts[0] - '0') * 10 + (ts[1] - '0') + 100;
  t.tm_mon = (ts[2] - '0') * 10 + (ts[3] - '0') - 1;
  t.tm_mday = (ts[4] - '0') * 10 + (ts[5] - '0');
  t.tm_hour = (ts[6] - '0') * 10 + (ts[7] - '0');
  t.tm_min = (ts[8] - '0') * 10 + (ts[9] - '0');
  t.tm_sec = (ts[10] - '0') * 10 + (ts[11] - '0');
  t.tm_isdst = -1;

  if (mktime(&t) != (time_t)-1) {
    char buf[32];
    if (strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &t) > 0) {
      const size_t len = strlen(buf);
      if (len >= 5 && (buf[len - 5] == '+' || buf[len - 5] == '-')) {
        char formatted[32];
        snprintf(formatted, sizeof(formatted), "%.*s:%s", (int)(len - 2), buf, buf + len - 2);
        strlcpy(out, formatted, outLen);
        return;
      }
      strlcpy(out, buf, outLen);
      return;
    }
  }

  const char* offset = (strlen(ts) >= 13 && (ts[12] == 'S' || ts[12] == 's')) ? "+02:00" : "+01:00";
  snprintf(out, outLen, "20%c%c-%c%c-%c%cT%c%c:%c%c:%c%c%s",
           ts[0], ts[1], ts[2], ts[3], ts[4], ts[5],
           ts[6], ts[7], ts[8], ts[9], ts[10], ts[11], offset);
}

String JsonTapElectric(const WorkerTapPayload& payload) {
  JsonDocument doc;
  doc["timestamp"] = payload.timestamp;
  JsonObject perPhase = doc["perPhase"].to<JsonObject>();

  static const char* keys[3] = { "l1", "l2", "l3" };
  for (uint8_t i = 0; i < 3; i++) {
    if (!(payload.phaseMask & (1 << i))) continue;
    JsonObject phase = perPhase[keys[i]].to<JsonObject>();
    if (payload.voltage[i]) phase["voltage"] = payload.voltage[i] / 1000.0;
    if (payload.current[i]) phase["current"] = payload.current[i] / 1000.0;
    phase["power"] = payload.power[i];
  }

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

  // L1 is always sent; fall back to the aggregate power when a meter does not
  // report per-phase power (typical for single-phase meters).
  if (DSMRdata.power_delivered_l1_present || DSMRdata.power_returned_l1_present) {
    payload.power[0] = DSMRdata.power_delivered_l1.int_val() - DSMRdata.power_returned_l1.int_val();
  } else {
    payload.power[0] = DSMRdata.power_delivered.int_val() - DSMRdata.power_returned.int_val();
  }
  if (DSMRdata.voltage_l1_present) payload.voltage[0] = DSMRdata.voltage_l1.int_val();
  if (DSMRdata.current_l1_present) payload.current[0] = DSMRdata.current_l1.int_val();
  payload.phaseMask = 0x01;

  if (DSMRdata.voltage_l2_present || DSMRdata.power_delivered_l2_present) {
    payload.power[1] = DSMRdata.power_delivered_l2.int_val() - DSMRdata.power_returned_l2.int_val();
    if (DSMRdata.voltage_l2_present) payload.voltage[1] = DSMRdata.voltage_l2.int_val();
    if (DSMRdata.current_l2_present) payload.current[1] = DSMRdata.current_l2.int_val();
    payload.phaseMask |= 0x02;
  }
  if (DSMRdata.voltage_l3_present || DSMRdata.power_delivered_l3_present) {
    payload.power[2] = DSMRdata.power_delivered_l3.int_val() - DSMRdata.power_returned_l3.int_val();
    if (DSMRdata.voltage_l3_present) payload.voltage[2] = DSMRdata.voltage_l3.int_val();
    if (DSMRdata.current_l3_present) payload.current[2] = DSMRdata.current_l3.int_val();
    payload.phaseMask |= 0x04;
  }

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
