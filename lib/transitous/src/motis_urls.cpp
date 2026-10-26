#include "motis_urls.h"

namespace abf {

std::string urlEncode(const std::string &s) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() * 3);
  for (unsigned char c : s) {
    const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                       c == '_' || c == '.' || c == '~';
    if (plain) {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(kHex[c >> 4]);
      out.push_back(kHex[c & 0x0f]);
    }
  }
  return out;
}

std::string stopTimesUrl(const std::string &stopId, uint32_t count, const std::string &cursor) {
  std::string url = std::string(kTransitousBase) + "/api/v6/stoptimes?stopId=" + urlEncode(stopId) +
                    "&n=" + std::to_string(count);
  if (!cursor.empty()) url += "&pageCursor=" + urlEncode(cursor);
  return url;
}

std::string geocodeUrl(const std::string &query, uint32_t results) {
  return std::string(kTransitousBase) + "/api/v1/geocode?text=" + urlEncode(query) +
         "&type=STOP&language=de&numResults=" + std::to_string(results);
}

}  // namespace abf
