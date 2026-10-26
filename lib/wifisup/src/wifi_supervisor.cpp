#include "wifi_supervisor.h"

#include <stdio.h>

namespace abf {

const char *wifiStateName(WifiState s) {
  switch (s) {
    case WifiState::Provisioning: return "provisioning";
    case WifiState::Connecting:   return "connecting";
    case WifiState::Fallback:     return "fallback";
    case WifiState::Online:       return "online";
  }
  return "unknown";
}

void WifiSupervisor::enter(WifiState s, uint32_t now) {
  state_ = s;
  enteredAt_ = now;
  lastReconnect_ = now;
}

WifiActions WifiSupervisor::begin(bool hasCredentials, uint32_t now) {
  WifiActions a;
  if (hasCredentials) {
    enter(WifiState::Connecting, now);
    a.startWeb = true;
    a.reconnect = true;
  } else {
    enter(WifiState::Provisioning, now);
    a.startPortal = true;
  }
  return a;
}

WifiActions WifiSupervisor::update(const WifiInputs &in, uint32_t now) {
  WifiActions a;
  switch (state_) {
    case WifiState::Provisioning:
    case WifiState::Fallback:
      if (in.staConnected) {
        enter(WifiState::Online, now);
        a.stopPortal = true;
        a.startWeb = true;
        a.wentOnline = true;
      } else if (state_ == WifiState::Fallback && now - enteredAt_ >= t_.fallbackDurationMs &&
                 !in.portalHasClients) {
        enter(WifiState::Connecting, now);
        a.stopPortal = true;
        a.startWeb = true;
        a.reconnect = true;
      }
      break;

    case WifiState::Connecting:
      if (in.staConnected) {
        enter(WifiState::Online, now);
        a.wentOnline = true;
      } else if (now - enteredAt_ >= t_.fallbackAfterMs) {
        enter(WifiState::Fallback, now);
        a.stopWeb = true;
        a.startPortal = true;
      } else if (now - lastReconnect_ >= t_.reconnectEveryMs) {
        lastReconnect_ = now;
        a.reconnect = true;
      }
      break;

    case WifiState::Online:
      if (!in.staConnected) enter(WifiState::Connecting, now);
      break;
  }
  return a;
}

std::string apName(const uint8_t mac[6]) {
  char buf[24];
  snprintf(buf, sizeof(buf), "Abfahrtstafel-%02X%02X", mac[4], mac[5]);
  return buf;
}

}  // namespace abf
