// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "network.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <time.h>
#include <time_berlin.h>

#include <atomic>

#include "../log/app_log.h"
#include "../web/web_server.h"
#include "portal.h"

namespace net {

namespace {

abf::WifiSupervisor sSup;
std::atomic<abf::WifiState> sState{abf::WifiState::Connecting};
std::string sHostname;
std::string sApName;
bool sMdnsStarted = false;
std::atomic<bool> sTimeSynced{false};

void startMdns() {
  if (sMdnsStarted) return;
  if (!MDNS.begin(sHostname.c_str())) {
    LOGW("net", "mDNS konnte nicht gestartet werden");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  sMdnsStarted = true;
  LOGI("net", "mDNS: http://%s.local", sHostname.c_str());
}

void reconnect() {
  if (WiFi.status() == WL_CONNECTED) return;
  LOGD("wifi", "Verbindungsversuch");
  WiFi.disconnect(false, false);  // Zugangsdaten behalten
  WiFi.begin();
}

void apply(const abf::WifiActions &a) {
  // Erst stoppen, dann starten: Portal und Web-UI teilen sich Port 80
  if (a.stopWeb) web::end();
  if (a.stopPortal) portal::stop();
  if (a.startPortal) portal::start(sApName);
  if (a.startWeb) web::begin();
  if (a.reconnect) reconnect();
  if (a.wentOnline) {
    LOGI("wifi", "Verbunden mit \"%s\", IP %s, RSSI %d dBm", WiFi.SSID().c_str(),
         WiFi.localIP().toString().c_str(), WiFi.RSSI());
    startMdns();
  }
}

}  // namespace

void begin(const std::string &hostname) {
  sHostname = hostname;
  WiFi.persistent(true);
  WiFi.setHostname(sHostname.c_str());  // vor WiFi.mode(), sonst wirkt es erst nach dem nächsten Start
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // Netzbetrieb: Reaktionszeit der Web-UI wichtiger als Strom
  WiFi.setAutoReconnect(true);

  uint8_t mac[6];
  // Aus dem eFuse lesen: WiFi.macAddress() liefert vor dem Start des STA-Interfaces nur Nullen
  // (am Board gesehen: AP hieß "Abfahrtstafel-0000")
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  sApName = abf::apName(mac);

  configTzTime(abf::kTzBerlin, "de.pool.ntp.org", "pool.ntp.org", "time.cloudflare.com");

  portal::configure(sHostname);
  const bool creds = portal::hasSavedCredentials();
  if (creds) {
    LOGI("wifi", "Zugangsdaten vorhanden, verbinde");
  } else {
    LOGI("wifi", "Noch kein WLAN eingerichtet");
  }
  apply(sSup.begin(creds, millis()));
  sState = sSup.state();
}

void loop() {
  portal::process();

  abf::WifiInputs in;
  in.staConnected = WiFi.status() == WL_CONNECTED;
  in.portalHasClients = sSup.portalOpen() && WiFi.softAPgetStationNum() > 0;

  const abf::WifiState before = sSup.state();
  apply(sSup.update(in, millis()));
  if (sSup.state() != before) {
    LOGI("wifi", "Zustand: %s → %s", abf::wifiStateName(before), abf::wifiStateName(sSup.state()));
    sState = sSup.state();
  }

  if (!sTimeSynced && abf::isTimePlausible(time(nullptr))) {
    sTimeSynced = true;
    const time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[32];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S %Z", &tm);
    LOGI("time", "Uhrzeit per NTP gestellt: %s", buf);
  }
}

abf::WifiState state() { return sState; }
const std::string &hostname() { return sHostname; }
const std::string &apName() { return sApName; }
bool timeSynced() { return sTimeSynced; }

}  // namespace net
