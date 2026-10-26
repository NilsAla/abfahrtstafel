// Web-UI und REST-API (ESPAsyncWebServer). Eigene Übersetzungseinheit, siehe net/portal.h.
#pragma once

namespace web {

void begin();  // idempotent; bindet Port 80, bei Fehlschlag Wiederholung in loop()
void end();
void loop();     // wiederholt den Start, solange Port 80 noch belegt ist (z. B. direkt nach dem Portal)
bool running();  // true nur, wenn der Server wirklich auf Port 80 lauscht

}  // namespace web
