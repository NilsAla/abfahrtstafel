// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "https_client.h"

#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <strings.h>

#include "../log/app_log.h"
#include "../version.h"
#include "network.h"

namespace https {

namespace {

constexpr int kMaxRedirects = 5;

bool isRedirect(int status) {
  return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

// Date-Header jeder Antwort als Ersatz-Zeitquelle, falls NTP gesperrt ist
esp_err_t onEvent(esp_http_client_event_t *ev) {
  if (ev->event_id == HTTP_EVENT_ON_HEADER && ev->header_key && ev->header_value &&
      strcasecmp(ev->header_key, "Date") == 0) {
    net::offerHttpDate(ev->header_value);
  }
  return ESP_OK;
}

void *allocLarge(size_t size) {
  void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(size);
}

}  // namespace

Connection::~Connection() {
  if (!h) return;
  esp_http_client_close(h);
  esp_http_client_cleanup(h);
}

Body::~Body() { free(data); }

bool open(const char *url, Connection &conn, int64_t &contentLength, int *status) {
  if (status) *status = 0;
  esp_http_client_config_t cfg = {};
  cfg.url = url;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;  // TLS nur über das eingebaute CA-Bundle
  cfg.user_agent = FW_USER_AGENT;
  cfg.event_handler = onEvent;
  cfg.timeout_ms = 15000;
  cfg.buffer_size = 4096;     // GitHub-Redirect-Header (Location) sind > 512 Byte
  cfg.buffer_size_tx = 2048;  // lange Redirect-URLs in der Anfragezeile
  conn.h = esp_http_client_init(&cfg);
  if (!conn.h) return false;

  for (int i = 0; i <= kMaxRedirects; ++i) {
    const esp_err_t err = esp_http_client_open(conn.h, 0);
    if (err != ESP_OK) {
      LOGW("http", "Verbindung fehlgeschlagen: %s", esp_err_to_name(err));
      return false;
    }
    contentLength = esp_http_client_fetch_headers(conn.h);
    const int code = esp_http_client_get_status_code(conn.h);
    if (status) *status = code;
    if (isRedirect(code)) {
      esp_http_client_flush_response(conn.h, nullptr);
      esp_http_client_set_redirection(conn.h);
      esp_http_client_close(conn.h);
      continue;
    }
    if (code != 200) {
      LOGW("http", "HTTP %d", code);
      return false;
    }
    return true;
  }
  LOGW("http", "Zu viele Weiterleitungen");
  return false;
}

bool get(const char *url, size_t maxBytes, Body &out, int *status) {
  Connection conn;
  int64_t len = 0;
  if (!open(url, conn, len, status)) return false;
  if (len > static_cast<int64_t>(maxBytes)) {
    LOGW("http", "Antwort zu groß (%lld Bytes)", static_cast<long long>(len));
    return false;
  }
  // Bekannte Länge: passend anlegen, sonst (chunked) Maximum
  const size_t cap = (len > 0 ? static_cast<size_t>(len) : maxBytes) + 1;
  free(out.data);
  out.data = static_cast<char *>(allocLarge(cap));
  out.len = 0;
  if (!out.data) {
    LOGE("http", "Kein Speicher für %u Bytes", static_cast<unsigned>(cap));
    return false;
  }
  for (;;) {
    const int r = esp_http_client_read(conn.h, out.data + out.len, static_cast<int>(cap - 1 - out.len));
    if (r < 0) {
      LOGW("http", "Lesefehler");
      return false;
    }
    if (r == 0) break;
    out.len += static_cast<size_t>(r);
    if (out.len >= cap - 1) {
      // Puffer voll: nur in Ordnung, wenn die Antwort genau so lang ist
      if (len > 0 && out.len == static_cast<size_t>(len)) break;
      char probe;
      if (esp_http_client_read(conn.h, &probe, 1) > 0) {
        LOGW("http", "Antwort größer als %u Bytes", static_cast<unsigned>(maxBytes));
        return false;
      }
      break;
    }
  }
  out.data[out.len] = '\0';
  return true;
}

}  // namespace https
