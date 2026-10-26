// Web-UI und REST-API (ESPAsyncWebServer). Eigene Übersetzungseinheit, siehe net/portal.h.
#pragma once

namespace web {

void begin();  // idempotent
void end();
bool running();

}  // namespace web
