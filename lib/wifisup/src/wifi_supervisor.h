// WLAN-Ablaufsteuerung, unabhängig vom WLAN-Stack.
//
//   Provisioning  keine Zugangsdaten → Einrichtungs-WLAN (Captive Portal) offen, bis verbunden
//   Connecting    Zugangsdaten vorhanden, Verbindung fehlt → regelmäßig neu verbinden;
//                 nach fallbackAfterMs zusätzlich Einrichtungs-WLAN öffnen (Router getauscht?)
//   Fallback      Einrichtungs-WLAN offen; schließt nach fallbackDurationMs, sobald niemand verbunden ist.
//                 Keine eigenen Reconnects: Im AP+STA-Betrieb würde die Kanalsuche den AP stören.
//   Online        verbunden
//
// Einrichtungs-WLAN und Web-UI teilen sich Port 80 → nie gleichzeitig aktiv.
#pragma once

#include <stdint.h>

#include <string>

namespace abf {

enum class WifiState { Provisioning, Connecting, Fallback, Online };

const char *wifiStateName(WifiState s);  // Schlüssel für /api/status

struct WifiActions {
  bool startPortal = false;
  bool stopPortal = false;
  bool startWeb = false;
  bool stopWeb = false;
  bool reconnect = false;      // erneut verbinden (gespeicherte Zugangsdaten)
  bool wentOnline = false;     // erstmals oder wieder verbunden
  bool any() const { return startPortal || stopPortal || startWeb || stopWeb || reconnect || wentOnline; }
};

struct WifiInputs {
  bool staConnected = false;
  bool portalHasClients = false;  // jemand ist mit dem Einrichtungs-WLAN verbunden
};

class WifiSupervisor {
 public:
  struct Timing {
    uint32_t reconnectEveryMs = 30 * 1000;
    uint32_t fallbackAfterMs = 3 * 60 * 1000;
    uint32_t fallbackDurationMs = 5 * 60 * 1000;
  };

  WifiSupervisor() = default;
  explicit WifiSupervisor(const Timing &t) : t_(t) {}

  WifiActions begin(bool hasCredentials, uint32_t now);
  WifiActions update(const WifiInputs &in, uint32_t now);

  WifiState state() const { return state_; }
  bool portalOpen() const { return state_ == WifiState::Provisioning || state_ == WifiState::Fallback; }

 private:
  void enter(WifiState s, uint32_t now);

  Timing t_;
  WifiState state_ = WifiState::Connecting;
  uint32_t enteredAt_ = 0;
  uint32_t lastReconnect_ = 0;
};

// Name des Einrichtungs-WLANs aus der MAC: "Abfahrtstafel-XXXX" (letzte zwei Bytes, hex).
std::string apName(const uint8_t mac[6]);

}  // namespace abf
