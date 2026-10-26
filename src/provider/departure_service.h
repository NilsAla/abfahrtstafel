// Abfahrten im Hintergrund abrufen und für Web-UI/Display bereitstellen.
//
// Eine Task fragt jede konfigurierte Haltestelle im Takt ab (≥ 30 s, Backoff bei Fehlern) und
// erledigt die Such- und Richtungsanfragen der Web-UI. Der Webserver blockiert nie: Anfragen
// liefern sofort {"state":"pending"} und werden erneut gestellt, bis das Ergebnis da ist.
#pragma once

#include <ArduinoJson.h>
#include <stdint.h>

#include <string>

namespace departures {

void begin();

// /api/departures: Zustand und aufbereitete Abfahrten je Haltestelle (mit aktueller Uhrzeit berechnet)
void buildBoard(JsonObject out, size_t maxRows);

// /api/stops?q=…
void lookupStops(const std::string &query, JsonObject out);

// /api/directions?stop=…[&more=1]
void lookupDirections(const std::string &stopId, bool more, JsonObject out);

int64_t lastSuccess();  // Unix-Zeit des letzten erfolgreichen Abrufs (0 = nie)

}  // namespace departures
