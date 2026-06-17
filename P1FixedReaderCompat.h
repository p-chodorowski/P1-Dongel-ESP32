#pragma once

// Fallback until mhendriks/dsmr2Lib ships P1FixedReader (required since P1-Dongel 5.8.3).
// Remove this include from DSMRloggerAPI.h once the library provides P1FixedReader.

#include <dsmr2.h>

#if !defined(DSMR2_HAS_P1_FIXED_READER)

template <size_t Cap>
class P1FixedReader : public P1Reader {
 public:
  P1FixedReader(Stream* stream, int8_t req_pin, bool checksum = true)
      : P1Reader(stream, static_cast<uint8_t>(req_pin), checksum) {}

  size_t rawLength() { return P1Reader::raw().length(); }
};

#endif
