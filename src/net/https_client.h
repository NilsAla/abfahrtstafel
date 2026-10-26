// HTTPS-GET mit CA-Bundle, User-Agent und Redirects (für OTA und Datenanbieter).
#pragma once

#include <esp_http_client.h>
#include <stddef.h>
#include <stdint.h>

namespace https {

// Offene Verbindung; schließt und räumt im Destruktor auf.
struct Connection {
  esp_http_client_handle_t h = nullptr;
  Connection() = default;
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  ~Connection();
};

// Folgt bis zu 5 Redirects. true nur bei HTTP 200; Body ist danach ungelesen.
// status (optional) liefert den letzten HTTP-Status (0 = Verbindungsfehler).
bool open(const char *url, Connection &conn, int64_t &contentLength, int *status = nullptr);

// Antwort im PSRAM (Fallback: interner Heap), nullterminiert.
struct Body {
  char *data = nullptr;
  size_t len = 0;
  Body() = default;
  Body(const Body &) = delete;
  Body &operator=(const Body &) = delete;
  ~Body();
};

// Lädt die komplette Antwort. false bei Fehler oder wenn sie größer als maxBytes ist.
bool get(const char *url, size_t maxBytes, Body &out, int *status = nullptr);

}  // namespace https
