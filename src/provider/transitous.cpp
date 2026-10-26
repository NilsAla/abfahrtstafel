// Transitous (MOTIS 2), https://transitous.org – UNGETESTET am Board.
// Nutzungsregeln: User-Agent mit Kontakt (FW_USER_AGENT), nicht schneller als alle 30 s abfragen,
// Attribution in der Web-UI.
#include <esp_heap_caps.h>
#include <motis_urls.h>
#include <stdlib.h>

#include "../log/app_log.h"
#include "../net/https_client.h"
#include "provider.h"

namespace provider {

namespace {

constexpr size_t kMaxStopTimesBytes = 320 * 1024;  // ~2,2 KB je Abfahrt, n ≤ 100
constexpr size_t kMaxGeocodeBytes = 64 * 1024;
constexpr uint32_t kSearchResults = 8;

// JSON-Dokumente der großen Antworten im PSRAM halten
class PsramAllocator : public ArduinoJson::Allocator {
 public:
  void *allocate(size_t size) override {
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(size);
  }
  void deallocate(void *p) override { free(p); }
  void *reallocate(void *p, size_t size) override {
    void *n = heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return n ? n : realloc(p, size);
  }
};

PsramAllocator sAlloc;

class Transitous : public Provider {
 public:
  const char *name() const override { return "Transitous"; }

  bool departures(const std::string &stopId, uint32_t count, const std::string &cursor,
                  abf::StopTimesResult &out) override {
    const std::string url = abf::stopTimesUrl(stopId, count, cursor);
    https::Body body;
    if (!https::get(url.c_str(), kMaxStopTimesBytes, body)) return false;
    if (!abf::parseStopTimes(body.data, body.len, out, &sAlloc)) {
      LOGW("transitous", "Abfahrten nicht lesbar (%u Bytes)", static_cast<unsigned>(body.len));
      return false;
    }
    LOGD("transitous", "%u Abfahrten (%u Bytes)", static_cast<unsigned>(out.departures.size()),
         static_cast<unsigned>(body.len));
    return true;
  }

  bool searchStops(const std::string &query, std::vector<abf::StopMatch> &out) override {
    const std::string url = abf::geocodeUrl(query, kSearchResults);
    https::Body body;
    if (!https::get(url.c_str(), kMaxGeocodeBytes, body)) return false;
    if (!abf::parseGeocode(body.data, body.len, out, &sAlloc)) {
      LOGW("transitous", "Suchergebnis nicht lesbar");
      return false;
    }
    return true;
  }
};

Transitous sTransitous;

}  // namespace

Provider &active() { return sTransitous; }

}  // namespace provider
