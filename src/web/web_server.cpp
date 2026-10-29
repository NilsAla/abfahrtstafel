// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
//
// Endpunkte:
//   GET  /, /app.js, /app.css    Web-UI (gzip, ETag)
//   GET  /api/status             Systemzustand
//   GET  /api/config             Einstellungen
//   POST /api/config             Teil-Update (JSON), 400 mit Feldfehlern
//   POST /api/restart            Neustart nach kurzer Verzögerung
//   POST /api/factory-reset      Werkseinstellungen (wie BOOT 10 s), 409 während eines Updates
//   GET  /api/log?since=N        Logzeilen > N als Text; Header X-Log-Last, X-Log-Gap
//   GET  /api/update             Update-Zustand, Angebot, Fortschritt, Ergebnis des letzten Updates
//   POST /api/update/check       Nach Update suchen (202, 409 wenn beschäftigt)
//   POST /api/update/install     Angebotenes Update installieren (202, 409)
//   GET  /api/departures         Abfahrten je Haltestelle, aufbereitet, mit Zustand und Datenalter
//   GET  /api/stops?q=…          Haltestellensuche (asynchron: state pending → done/error, erneut abfragen)
//   GET  /api/directions?stop=…  Linien/Richtungen einer Haltestelle für die Auswahl (asynchron, &more=1)
//   GET  /api/coredump/info      Absturzbericht vorhanden? Task, Grund, Backtrace
//   GET  /api/coredump           Absturzbericht als ELF-Datei (404 ohne Bericht)
//   POST /api/coredump/erase     Absturzbericht löschen
#include "web_server.h"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <string.h>

#include <algorithm>
#include <memory>

#include "../config/config_service.h"
#include "../generated/web_assets.h"
#include "../log/app_log.h"
#include "../net/network.h"
#include "../ota/ota.h"
#include "../provider/departure_service.h"
#include "../system/coredump.h"
#include "../system/status.h"
#include "../system/system.h"
#include "../version.h"

namespace web {

namespace {

constexpr uint32_t kRetryMs = 2000;

AsyncWebServer sServer(80);
bool sRoutes = false;
bool sWanted = false;   // begin() aufgerufen, end() noch nicht
bool sRunning = false;  // lauscht tatsächlich
uint32_t sLastTry = 0;
uint32_t sTries = 0;

void sendJson(AsyncWebServerRequest *req, int code, const JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  AsyncWebServerResponse *res = req->beginResponse(code, "application/json", body);
  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

void sendAsset(AsyncWebServerRequest *req, const WebAsset &a) {
  const AsyncWebHeader *inm = req->getHeader("If-None-Match");
  if (inm && inm->value() == a.etag) {
    req->send(304);
    return;
  }
  AsyncWebServerResponse *res = req->beginResponse(200, a.mime, a.data, a.len);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("ETag", a.etag);
  res->addHeader("Cache-Control", "no-cache");
  req->send(res);
}

void handleConfigPost(AsyncWebServerRequest *req, JsonVariant &json) {
  JsonDocument out;
  if (!json.is<JsonObject>()) {
    out["ok"] = false;
    out["message"] = "Ungültige Anfrage.";
    sendJson(req, 400, out);
    return;
  }
  const config::UpdateResult r = config::applyJson(json.as<JsonObjectConst>());
  out["ok"] = r.ok;
  abf::configToJson(r.cfg, out["config"].to<JsonObject>());
  if (r.ok) {
    out["restart_required"] = r.restartRequired;
    sendJson(req, 200, out);
  } else if (r.saveFailed) {
    out["message"] = "Speichern fehlgeschlagen. Bitte das Gerät neu starten und erneut versuchen.";
    sendJson(req, 500, out);
  } else {
    JsonObject errors = out["errors"].to<JsonObject>();
    for (const abf::FieldError &e : r.errors) errors[e.field] = e.message;
    out["message"] = "Bitte die markierten Felder prüfen.";
    sendJson(req, 400, out);
  }
}

void handleLog(AsyncWebServerRequest *req) {
  uint32_t since = 0;
  if (const AsyncWebParameter *p = req->getParam("since")) since = strtoul(p->value().c_str(), nullptr, 10);
  // Der Text muss bis zum Ende der Übertragung leben (die Byte-Variante von beginResponse kopiert nicht)
  auto c = std::make_shared<applog::Chunk>(applog::readSince(since));
  AsyncWebServerResponse *res = req->beginResponse(
      "text/plain; charset=utf-8", c->text.size(), [c](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
        const size_t n = std::min(maxLen, c->text.size() - index);
        memcpy(buf, c->text.data() + index, n);
        return n;
      });
  res->addHeader("X-Log-Last", String(c->last));
  res->addHeader("X-Log-Gap", c->gap ? "1" : "0");
  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

void handleCoredump(AsyncWebServerRequest *req) {
  const size_t total = coredump::size();
  if (total == 0) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["message"] = "Kein Absturzbericht vorhanden.";
    sendJson(req, 404, doc);
    return;
  }
  // Direkt aus dem Flash streamen, kein Puffer für das ganze Image (bis 64 KB)
  AsyncWebServerResponse *res = req->beginResponse(
      "application/octet-stream", total, [total](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
        const size_t n = std::min(maxLen, total - index);
        return coredump::read(index, buf, n) ? n : 0;
      });
  res->addHeader("Content-Disposition", "attachment; filename=\"coredump-" FW_VERSION ".elf\"");
  res->addHeader("Cache-Control", "no-store");
  req->send(res);
}

void registerRoutes() {
  for (size_t i = 0; i < kWebAssetCount; ++i) {
    const WebAsset *a = &kWebAssets[i];
    sServer.on(a->path, HTTP_GET, [a](AsyncWebServerRequest *req) { sendAsset(req, *a); });
    if (strcmp(a->path, "/index.html") == 0) {
      sServer.on("/", HTTP_GET, [a](AsyncWebServerRequest *req) { sendAsset(req, *a); });
    }
  }

  sServer.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    status::build(doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });

  sServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    abf::configToJson(config::get(), doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });

  sServer.on("/api/config", HTTP_POST, handleConfigPost);

  sServer.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *req) {
    LOGI("web", "Neustart über Web-UI angefordert");
    sys::scheduleRestart(1000);
    JsonDocument doc;
    doc["ok"] = true;
    sendJson(req, 202, doc);
  });

  sServer.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    if (ota::busy()) {
      doc["ok"] = false;
      doc["message"] = "Gerade läuft ein Update. Bitte warten, bis es fertig ist.";
      sendJson(req, 409, doc);
      return;
    }
    LOGW("web", "Werksreset über Web-UI angefordert");
    sys::scheduleFactoryReset(1500);  // Antwort soll noch beim Browser ankommen
    doc["ok"] = true;
    doc["ap"] = net::apName();
    sendJson(req, 202, doc);
  });

  sServer.on("/api/log", HTTP_GET, handleLog);

  sServer.on("/api/update", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    ota::buildStatus(doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });

  sServer.on("/api/update/check", HTTP_POST, [](AsyncWebServerRequest *req) {
    const bool ok = ota::requestCheck();
    JsonDocument doc;
    doc["ok"] = ok;
    if (!ok) doc["message"] = "Es läuft bereits eine Prüfung oder ein Update.";
    sendJson(req, ok ? 202 : 409, doc);
  });

  sServer.on("/api/update/install", HTTP_POST, [](AsyncWebServerRequest *req) {
    LOGI("web", "Update-Installation über Web-UI angefordert");
    const bool ok = ota::requestInstall();
    JsonDocument doc;
    doc["ok"] = ok;
    if (!ok) doc["message"] = "Kein Update verfügbar oder es läuft bereits eines.";
    sendJson(req, ok ? 202 : 409, doc);
  });

  sServer.on("/api/departures", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    departures::buildBoard(doc.to<JsonObject>(), 8);
    sendJson(req, 200, doc);
  });

  sServer.on("/api/stops", HTTP_GET, [](AsyncWebServerRequest *req) {
    const AsyncWebParameter *q = req->getParam("q");
    JsonDocument doc;
    departures::lookupStops(q ? q->value().c_str() : "", doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });

  sServer.on("/api/directions", HTTP_GET, [](AsyncWebServerRequest *req) {
    const AsyncWebParameter *stop = req->getParam("stop");
    JsonDocument doc;
    departures::lookupDirections(stop ? stop->value().c_str() : "", req->hasParam("more"), doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });

  sServer.on("/api/coredump/info", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    coredump::buildInfo(doc.to<JsonObject>());
    sendJson(req, 200, doc);
  });

  sServer.on("/api/coredump", HTTP_GET, handleCoredump);

  sServer.on("/api/coredump/erase", HTTP_POST, [](AsyncWebServerRequest *req) {
    const bool ok = coredump::erase();
    JsonDocument doc;
    doc["ok"] = ok;
    if (!ok) doc["message"] = "Löschen fehlgeschlagen.";
    sendJson(req, ok ? 200 : 500, doc);
  });

  sServer.onNotFound([](AsyncWebServerRequest *req) {
    if (req->url().startsWith("/api/")) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["message"] = "Unbekannter Endpunkt.";
      sendJson(req, 404, doc);
    } else {
      req->redirect("/");
    }
  });
}

void tryStart() {
  sLastTry = millis();
  ++sTries;
  sServer.begin();
  // Am Board gesehen: Direkt nach dem Einrichtungsportal ist Port 80 noch belegt (AsyncTCP: "bind error: -8").
  // AsyncTCP gibt den Socket dann frei, ein späteres begin() versucht es neu.
  sRunning = sServer.state() == LISTEN;
  if (sRunning) {
    LOGI("web", "Web-UI gestartet (Port 80%s)", sTries > 1 ? ", nach erneutem Versuch" : "");
  } else if (sTries == 1) {
    LOGW("web", "Port 80 noch belegt, neuer Versuch alle %lu s", static_cast<unsigned long>(kRetryMs / 1000));
  }
}

}  // namespace

void begin() {
  if (sWanted) return;
  if (!sRoutes) {
    registerRoutes();
    sRoutes = true;
  }
  sWanted = true;
  sTries = 0;
  tryStart();
}

void end() {
  if (!sWanted) return;
  sWanted = false;
  if (!sRunning) return;
  sServer.end();
  sRunning = false;
  LOGI("web", "Web-UI gestoppt");
}

void loop() {
  if (sWanted && !sRunning && millis() - sLastTry >= kRetryMs) tryStart();
}

bool running() { return sRunning; }

}  // namespace web
