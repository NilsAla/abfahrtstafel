// Datenanbieter für Abfahrten. Austauschbar (Transitous jetzt, Fallback-Provider später).
// Aufrufe blockieren (HTTPS) → nur aus eigenen Tasks verwenden, nie aus dem Webserver oder Loop.
#pragma once

#include <motis_parser.h>
#include <transit_types.h>

#include <string>
#include <vector>

namespace provider {

class Provider {
 public:
  virtual ~Provider() = default;
  virtual const char *name() const = 0;
  // cursor: Folgeseite (leer = ab jetzt)
  virtual bool departures(const std::string &stopId, uint32_t count, const std::string &cursor,
                          abf::StopTimesResult &out) = 0;
  virtual bool searchStops(const std::string &query, std::vector<abf::StopMatch> &out) = 0;
};

Provider &active();

}  // namespace provider
