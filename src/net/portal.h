// Einrichtungs-WLAN (Captive Portal) über WiFiManager.
// Eigene Übersetzungseinheit: WiFiManager bringt den synchronen WebServer mit,
// der soll nie im selben .cpp wie ESPAsyncWebServer landen.
#pragma once

#include <string>

namespace portal {

void configure(const std::string &hostname);
bool hasSavedCredentials();  // erst nach WiFi.mode() aufrufen
void start(const std::string &apName);
void stop();
bool active();
void process();  // in jedem Loop-Durchlauf
bool takeCredentialsSaved();  // true einmalig, nachdem im Portal neue Zugangsdaten gespeichert wurden

}  // namespace portal
