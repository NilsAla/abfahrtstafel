// HTTP-Date (RFC 7231, IMF-fixdate) als Ersatz-Zeitquelle, wenn NTP gesperrt ist (z. B. Gäste-WLAN ohne UDP 123).
#pragma once

#include <stdint.h>

namespace abf {

// "Wed, 29 Oct 2026 08:12:45 GMT" → Unix-Zeit. false bei anderem Format oder ungültigen Werten.
bool parseHttpDate(const char *s, int64_t &epoch);

}  // namespace abf
