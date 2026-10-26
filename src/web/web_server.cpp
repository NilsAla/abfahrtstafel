// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
//
// Endpunkte:
//   GET  /, /app.js, /app.css    Web-UI (gzip, ETag)
//   GET  /api/status             Systemzustand
//   GET  /api/config             Einstellungen
//   POST /api/config             Teil-Update (JSON), 400 mit Feldfehlern
//   POST /api/restart            Neustart nach kurzer Verzögerung
//   GET  /api/log?since=N        Logzeilen > N als Text; Header X-Log-Last, X-Log-Gap
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
#include "../system/status.h"
#include "../system/system.h"

namespace web {

namespace {

AsyncWebServer sServer(80);
bool sRoutes = false;
bool sRunning = false;

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

  sServer.on("/api/log", HTTP_GET, handleLog);

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

}  // namespace

void begin() {
  if (sRunning) return;
  if (!sRoutes) {
    registerRoutes();
    sRoutes = true;
  }
  sServer.begin();
  sRunning = true;
  LOGI("web", "Web-UI gestartet (Port 80)");
}

void end() {
  if (!sRunning) return;
  sServer.end();
  sRunning = false;
  LOGI("web", "Web-UI gestoppt");
}

bool running() { return sRunning; }

}  // namespace web
