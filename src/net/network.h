// WLAN, mDNS und NTP. Ablauf steuert abf::WifiSupervisor (lib/wifisup).
#pragma once

#include <wifi_supervisor.h>

#include <string>

namespace net {

void begin(const std::string &hostname);
void loop();

abf::WifiState state();
const std::string &hostname();  // aktiver Hostname (Änderung wirkt nach Neustart)
const std::string &apName();
bool timeSynced();
std::string ip();     // IP im Heim-WLAN, leer ohne Verbindung
bool setupRestart();

// Uhrzeit aus einem HTTP-Date-Header übernehmen, solange keine plausible Zeit vorliegt (NTP gesperrt).
// Threadsicher, aus jedem Task aufrufbar.
void offerHttpDate(const char *value);  // WLAN eben eingerichtet, Neustart ist geplant (Hinweis auf dem Display)

}  // namespace net
