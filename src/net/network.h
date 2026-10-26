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

}  // namespace net
