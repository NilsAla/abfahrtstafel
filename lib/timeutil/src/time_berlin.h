// Zeitzone und Plausibilität der Systemzeit.
#pragma once

#include <stdint.h>

namespace abf {

// POSIX-TZ für Europe/Berlin: MEZ, MESZ vom letzten Sonntag im März 02:00
// bis zum letzten Sonntag im Oktober 03:00.
constexpr const char *kTzBerlin = "CET-1CEST,M3.5.0,M10.5.0/3";

// Vor NTP-Sync steht die Uhr auf 1970. Alles vor 2026-01-01 gilt als nicht synchronisiert.
constexpr int64_t kPlausibleEpoch = 1767225600;

inline bool isTimePlausible(int64_t epoch) { return epoch >= kPlausibleEpoch; }

}  // namespace abf
