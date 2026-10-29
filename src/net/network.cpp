// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "network.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_sntp.h>
#include <http_date.h>
#include <sys/time.h>
#include <time.h>
#include <time_berlin.h>

#include <atomic>

#include "../log/app_log.h"
#include "https_client.h"
#include "../system/system.h"
#include "../web/web_server.h"
#include "portal.h"

namespace net {

namespace {

abf::WifiSupervisor sSup;
std::atomic<abf::WifiState> sState{abf::WifiState::Connecting};
std::string sHostname;
std::string sApName;
bool sMdnsStarted = false;
std::atomic<bool> sTimeSynced{false};   // Uhrzeit plausibel (NTP oder über Software-Reset erhalten)
std::atomic<bool> sNtpEvent{false};
std::atomic<bool> sHttpTimeEvent{false};
std::atomic<bool> sTimeFetchRunning{false};
uint32_t sOnlineSince = 0;
uint32_t sLastTimeFetch = 0;
constexpr uint32_t kTimeFetchAfterMs = 30 * 1000;   // so lange bekommt NTP Zeit
constexpr uint32_t kTimeFetchEveryMs = 2 * 60 * 1000;
// Kleine Antwort mit Date-Header; GitHub ist für Updates ohnehin erreichbar
constexpr const char *kTimeUrl = "https://github.com/NilsAla/abfahrtstafel/releases/latest/download/manifest.json";

void timeFetchTask(void *) {
  https::Body body;
  https::get(kTimeUrl, 4096, body);  // Date wird im Event-Handler übernommen
  sTimeFetchRunning = false;
  vTaskDelete(nullptr);
}
std::atomic<bool> sSetupRestart{false};
bool sPortalSaved = false;  // im Portal gespeichert, Verbindung steht noch aus
uint32_t sApCheck = 0;
constexpr uint32_t kApCheckMs = 15 * 1000;

bool apRunning() { return (WiFi.getMode() & WIFI_MODE_AP) != 0; }

void openPortal() {
  if (sSup.state() == abf::WifiState::Fallback) {
    // Der Arduino-Core verbindet bei „WLAN nicht gefunden“ von selbst endlos neu (Auto-Reconnect). Die Kanalsuche
    // der Station stört den eigenen AP bzw. schaltet die Station nach dem Portalstart wieder ein → vorher abschalten.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
  }
  portal::start(sApName);
  sApCheck = millis();
  if (apRunning()) {
    LOGI("wifi", "Einrichtungs-WLAN aktiv: \"%s\", %s", WiFi.softAPSSID().c_str(), WiFi.softAPIP().toString().c_str());
  } else {
    LOGE("wifi", "Einrichtungs-WLAN ließ sich nicht öffnen (WLAN-Modus %d), neuer Versuch in %lu s",
         static_cast<int>(WiFi.getMode()), static_cast<unsigned long>(kApCheckMs / 1000));
  }
}
constexpr uint32_t kSetupRestartMs = 4000;  // Zeit für den Hinweis auf dem Display

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

void apply(const abf::WifiActions &a, bool fromPortal) {
  // Erst stoppen, dann starten: Portal und Web-UI teilen sich Port 80
  if (a.stopWeb) web::end();
  if (a.stopPortal) {
    portal::stop();
    WiFi.setAutoReconnect(true);
  }
  if (a.startPortal) openPortal();
  if (a.startWeb) web::begin();
  if (a.reconnect) reconnect();
  if (a.wentOnline) {
    sOnlineSince = millis();
    LOGI("wifi", "Verbunden mit \"%s\", IP %s, RSSI %d dBm", WiFi.SSID().c_str(),
         WiFi.localIP().toString().c_str(), WiFi.RSSI());
    startMdns();
    // Nach dem Einrichtungsportal war die Web-UI am Board teils erst nach einem Neustart erreichbar
    // (Port 80/WLAN-Stack im AP+STA-Zustand). Ein Neustart mit den neuen Zugangsdaten ist der sichere Weg.
    if (fromPortal && !sSetupRestart) {
      LOGI("wifi", "WLAN eingerichtet, Neustart in %lu s", static_cast<unsigned long>(kSetupRestartMs / 1000));
      sSetupRestart = true;
      sys::scheduleRestart(kSetupRestartMs);
    }
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
  esp_sntp_set_time_sync_notification_cb([](struct timeval *) { sNtpEvent = true; });

  portal::configure(sHostname);
  const bool creds = portal::hasSavedCredentials();
  if (creds) {
    LOGI("wifi", "Zugangsdaten vorhanden, verbinde");
  } else {
    LOGI("wifi", "Noch kein WLAN eingerichtet");
  }
  apply(sSup.begin(creds, millis()), false);
  sState = sSup.state();
}

void loop() {
  portal::process();

  abf::WifiInputs in;
  in.staConnected = WiFi.status() == WL_CONNECTED;
  in.portalHasClients = sSup.portalOpen() && WiFi.softAPgetStationNum() > 0;

  const abf::WifiState before = sSup.state();
  // Neu eingerichtet: aus der Ersteinrichtung oder mit im Fallback-Portal gespeicherten Zugangsdaten
  sPortalSaved |= portal::takeCredentialsSaved();
  const abf::WifiActions actions = sSup.update(in, millis());
  apply(actions, actions.wentOnline && (before == abf::WifiState::Provisioning || sPortalSaved));
  if (actions.wentOnline) sPortalSaved = false;

  // Portal soll offen sein, AP läuft aber nicht (Start fehlgeschlagen oder vom WLAN-Stack wieder abgeschaltet)
  if (sSup.portalOpen() && millis() - sApCheck >= kApCheckMs) {
    sApCheck = millis();
    if (!apRunning()) {
      LOGW("wifi", "Einrichtungs-WLAN läuft nicht, starte es neu");
      portal::stop();
      openPortal();
    }
  }
  if (sSup.state() != before) {
    LOGI("wifi", "Zustand: %s → %s", abf::wifiStateName(before), abf::wifiStateName(sSup.state()));
    sState = sSup.state();
  }

  // Keine Uhrzeit trotz Verbindung: NTP (UDP 123) ist in manchen Gäste-WLANs gesperrt → Date-Header per HTTPS
  if (!sTimeSynced && sSup.state() == abf::WifiState::Online && millis() - sOnlineSince >= kTimeFetchAfterMs &&
      (sLastTimeFetch == 0 || millis() - sLastTimeFetch >= kTimeFetchEveryMs) && !sTimeFetchRunning) {
    sLastTimeFetch = millis();
    LOGW("time", "Noch keine Uhrzeit per NTP, hole sie per HTTPS");
    sTimeFetchRunning = true;
    if (xTaskCreatePinnedToCore(timeFetchTask, "time", 12 * 1024, nullptr, 1, nullptr, 1) != pdPASS) {
      sTimeFetchRunning = false;
    }
  }

  // Die RTC läuft über einen Software-Reset weiter → Zeit kann schon vor dem ersten NTP-Sync gültig sein
  const bool ntp = sNtpEvent.exchange(false);
  const bool http = sHttpTimeEvent.exchange(false);
  if (ntp || (!sTimeSynced && abf::isTimePlausible(time(nullptr)))) {
    const bool first = !sTimeSynced;
    sTimeSynced = true;
    const time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[32];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S %Z", &tm);
    if (ntp) {
      LOGI("time", "Uhrzeit per NTP synchronisiert: %s", buf);
    } else if (http) {
      LOGI("time", "Uhrzeit per HTTPS übernommen (NTP nicht erreichbar): %s", buf);
    } else if (first) {
      LOGI("time", "Uhrzeit aus RTC übernommen (NTP folgt): %s", buf);
    }
  }
}

abf::WifiState state() { return sState; }
const std::string &hostname() { return sHostname; }
const std::string &apName() { return sApName; }
bool timeSynced() { return sTimeSynced; }

std::string ip() {
  if (sState != abf::WifiState::Online) return std::string();
  return WiFi.localIP().toString().c_str();
}

bool setupRestart() { return sSetupRestart; }

void offerHttpDate(const char *value) {
  if (abf::isTimePlausible(time(nullptr))) return;  // NTP bzw. RTC haben Vorrang
  int64_t epoch = 0;
  if (!abf::parseHttpDate(value, epoch) || !abf::isTimePlausible(epoch)) return;
  struct timeval tv = {static_cast<time_t>(epoch), 0};
  settimeofday(&tv, nullptr);
  sHttpTimeEvent = true;
}

}  // namespace net
