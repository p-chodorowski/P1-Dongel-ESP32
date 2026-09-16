/* Compile-time check of profile.h OTA URL construction.
 * Built twice by tools/test_ota_url.py (with and without ULTRA).
 */
#ifndef DIRECT_AP_CONNECT
#define DIRECT_AP_CONNECT 0
#endif

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif
#include "../version.h"
#include "../profile.h"

#ifndef OTAURL_PREFIX
#define OTAURL_PREFIX ""
#endif

#include <stdio.h>
#include <string.h>

#ifndef EXPECTED_OTAURL
#error EXPECTED_OTAURL must be defined
#endif

int main(void) {
  const char *url = OTAURL OTAURL_PREFIX;
  const char *expect = EXPECTED_OTAURL;
  if (strcmp(url, expect) != 0) {
    fprintf(stderr, "OTAURL mismatch\n  got:    [%s]\n  expect: [%s]\n", url, expect);
    return 1;
  }
#ifdef BASE_OTA_URL_SIZE
  if (BASE_OTA_URL_SIZE < 96) {
    fprintf(stderr, "BASE_OTA_URL_SIZE is %d, need >= 96\n", (int)BASE_OTA_URL_SIZE);
    return 2;
  }
#else
  fprintf(stderr, "BASE_OTA_URL_SIZE is not defined\n");
  return 2;
#endif
  return 0;
}
