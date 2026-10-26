// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "portal.h"

#include <WiFiManager.h>

#include "../log/app_log.h"

namespace portal {

namespace {

WiFiManager sWm;
const char *sMenu[] = {"wifi"};  // nur WLAN-Auswahl: kein Update/Erase/Exit im Portal

}  // namespace

void configure(const std::string &hostname) {
  sWm.setConfigPortalBlocking(false);
  sWm.setTitle("Abfahrtstafel");
  sWm.setMenu(sMenu, sizeof(sMenu) / sizeof(sMenu[0]));
  sWm.setShowInfoUpdate(false);
  sWm.setConnectTimeout(20);
  sWm.setWiFiAutoReconnect(true);
  sWm.setHostname(hostname.c_str());
  sWm.setSaveConfigCallback([] { LOGI("wifi", "Neue WLAN-Zugangsdaten gespeichert"); });
}

bool hasSavedCredentials() { return sWm.getWiFiIsSaved(); }

void start(const std::string &apName) {
  if (sWm.getConfigPortalActive()) return;
  LOGI("wifi", "Einrichtungs-WLAN \"%s\" geöffnet", apName.c_str());
  sWm.startConfigPortal(apName.c_str());
}

void stop() {
  // WiFiManager schließt das Portal nach erfolgreicher Einrichtung selbst; ein zweites
  // shutdownConfigPortal() dereferenziert dort einen bereits freigegebenen Server.
  if (!sWm.getConfigPortalActive()) return;
  sWm.stopConfigPortal();
  LOGI("wifi", "Einrichtungs-WLAN geschlossen");
}

bool active() { return sWm.getConfigPortalActive(); }

void process() {
  if (sWm.getConfigPortalActive()) sWm.process();
}

}  // namespace portal
