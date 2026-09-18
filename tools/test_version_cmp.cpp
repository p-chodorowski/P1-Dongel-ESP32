/* Compile-time tests for four-part fork versioning in version.h */
#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../version.h"

static int failures = 0;

static void expect_true(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    failures++;
  }
}

static void expect_int(int got, int want, const char* msg) {
  if (got != want) {
    std::fprintf(stderr, "FAIL: %s (got %d want %d)\n", msg, got, want);
    failures++;
  }
}

int main() {
  expect_true(std::strcmp(_VERSION_ONLY, "5.8.4.1") == 0, "_VERSION_ONLY is 5.8.4.1");
  expect_int(_VERSION_FORK, 1, "_VERSION_FORK starts at 1");

  expect_true(ota_version_is_newer(5, 8, 4, 1, 5, 8, 4, 0), "5.8.4.1 > 5.8.4 (fork 0)");
  expect_true(!ota_version_is_newer(5, 8, 4, 1, 5, 8, 4, 1), "5.8.4.1 is not newer than itself");
  expect_true(ota_version_is_newer(5, 8, 4, 2, 5, 8, 4, 1), "5.8.4.2 > 5.8.4.1");
  expect_true(!ota_version_is_newer(5, 8, 4, 1, 5, 8, 4, 2), "5.8.4.1 is not newer than 5.8.4.2");
  expect_true(ota_version_is_newer(5, 8, 13, 0, 5, 8, 4, 1), "vendor 5.8.13 > fork 5.8.4.1");
  expect_true(!ota_version_is_newer(5, 8, 4, 9, 5, 9, 0, 0), "5.8.4.9 is not newer than 5.9.0");

  expect_int(ota_fork_from_version_string("5.8.4.1"), 1, "parse 5.8.4.1");
  expect_int(ota_fork_from_version_string("5.8.4"), 0, "parse 5.8.4 has fork 0");
  expect_int(ota_fork_from_version_string("5.8.4.12 ( Jan 1 )"), 12, "parse fork before suffix");
  expect_int(ota_fork_from_version_string(nullptr), 0, "null version");

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  return 0;
}
