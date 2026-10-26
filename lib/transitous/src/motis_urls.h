// URLs der Transitous-API (MOTIS 2).
#pragma once

#include <stdint.h>

#include <string>

namespace abf {

constexpr const char *kTransitousBase = "https://api.transitous.org";

std::string urlEncode(const std::string &s);  // RFC 3986, alles außer A-Z a-z 0-9 - _ . ~

// cursor: nextPageCursor einer vorherigen Antwort ("" = ab jetzt)
std::string stopTimesUrl(const std::string &stopId, uint32_t count, const std::string &cursor = "");
std::string geocodeUrl(const std::string &query, uint32_t results);

}  // namespace abf
