#include "status.h"

#include <Arduino.h>
#include <WiFi.h>
#include <display_model.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <time.h>
#include <time_berlin.h>

#include "../config/config_service.h"
#include "../display/display_task.h"
#include "../net/network.h"
#include "../provider/departure_service.h"
#include "../version.h"
#include "coredump.h"
#include "heap_monitor.h"
#include "system.h"

namespace status {

void build(JsonObject out) {
  out["version"] = FW_VERSION;
  out["uptime_s"] = static_cast<uint32_t>(esp_timer_get_time() / 1000000);
  out["reset_reason"] = sys::resetReasonText();
  const esp_partition_t *running = esp_ota_get_running_partition();
  out["partition"] = running ? running->label : "?";
  out["hostname"] = net::hostname();
  out["display"] = display::active();
  {
    const abf::Config cfg = config::get();
    const int64_t now = time(nullptr);
    const bool timeValid = abf::isTimePlausible(now);
    out["theme_mode"] = abf::themeModeKey(cfg.themeMode);
    out["theme_active"] =
        abf::themeKey(abf::backlightFor(cfg, timeValid, timeValid ? abf::localMinuteOfDay(now) : 0).theme);
  }
  out["coredump"] = coredump::present();

  JsonObject wifi = out["wifi"].to<JsonObject>();
  const abf::WifiState state = net::state();
  wifi["state"] = abf::wifiStateName(state);
  wifi["mac"] = WiFi.macAddress();
  if (state == abf::WifiState::Online) {
    wifi["ssid"] = WiFi.SSID();
    wifi["rssi"] = WiFi.RSSI();
    wifi["ip"] = WiFi.localIP().toString();
  } else if (state == abf::WifiState::Provisioning || state == abf::WifiState::Fallback) {
    wifi["ap"] = net::apName();
  }

  JsonObject t = out["time"].to<JsonObject>();
  t["synced"] = net::timeSynced();
  if (net::timeSynced()) {
    const time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &tm);
    t["local"] = buf;
  }

  JsonObject heap = out["heap"].to<JsonObject>();
  heap["free"] = ESP.getFreeHeap();
  heap["min_free"] = ESP.getMinFreeHeap();
  heap["largest_block"] = heapmon::largestBlock();
  heap["state"] = abf::heapLevelName(heapmon::level());
  heap["total"] = ESP.getHeapSize();

  JsonObject psram = out["psram"].to<JsonObject>();
  psram["free"] = ESP.getFreePsram();
  psram["min_free"] = ESP.getMinFreePsram();
  psram["total"] = ESP.getPsramSize();

  const int64_t fetched = departures::lastSuccess();
  if (fetched > 0) {
    const time_t ft = static_cast<time_t>(fetched);
    struct tm ftm;
    localtime_r(&ft, &ftm);
    char fbuf[32];
    strftime(fbuf, sizeof(fbuf), "%Y-%m-%dT%H:%M:%S%z", &ftm);
    out["last_fetch"] = fbuf;
  } else {
    out["last_fetch"] = nullptr;
  }
}

}  // namespace status
