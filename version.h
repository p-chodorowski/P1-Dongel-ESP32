#pragma once

#include <stdlib.h>

#define P1_STR1(x) #x
#define P1_STR(x) P1_STR1(x)

#define _VERSION_MAJOR 5
#define _VERSION_MINOR 9
#define _VERSION_PATCH 5
#ifndef _VERSION_FORK
#define _VERSION_FORK 2
#endif
#define _VERSION_ONLY P1_STR(_VERSION_MAJOR) "." P1_STR(_VERSION_MINOR) "." P1_STR(_VERSION_PATCH) "." P1_STR(_VERSION_FORK)

#ifndef STR1
#define STR1(x) #x
#endif
#ifndef STR
#define STR(x) STR1(x)
#endif

static inline uint32_t packed_version_u32() {
  return ( (uint32_t(_VERSION_MAJOR) << 16) | (uint32_t(_VERSION_MINOR) << 8) | uint32_t(_VERSION_PATCH) );
}

static inline int ota_fork_from_version_string(const char* version) {
  if (!version) return 0;
  int dots = 0;
  const char* p = version;
  while (*p && *p != ' ' && *p != '+' && *p != '(') {
    if (*p == '.') {
      dots++;
      p++;
      if (dots == 3) {
        return atoi(p);
      }
      continue;
    }
    p++;
  }
  return 0;
}

static inline int ota_version_cmp(int aMaj, int aMin, int aFix, int aFork,
                                  int bMaj, int bMin, int bFix, int bFork) {
  if (aMaj != bMaj) return (aMaj > bMaj) ? 1 : -1;
  if (aMin != bMin) return (aMin > bMin) ? 1 : -1;
  if (aFix != bFix) return (aFix > bFix) ? 1 : -1;
  if (aFork != bFork) return (aFork > bFork) ? 1 : -1;
  return 0;
}

static inline bool ota_version_is_newer(int remoteMaj, int remoteMin, int remoteFix, int remoteFork,
                                        int localMaj, int localMin, int localFix, int localFork) {
  return ota_version_cmp(remoteMaj, remoteMin, remoteFix, remoteFork,
                         localMaj, localMin, localFix, localFork) > 0;
}
