// UNGETESTET am Board (siehe docs/ROADMAP.md, „Offen am Board“).
#include "ota.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <manifest.h>
#include <mbedtls/sha256.h>
#include <update_flow.h>

#include <memory>
#include <mutex>

#include "../config/config_service.h"
#include "../log/app_log.h"
#include "../net/https_client.h"
#include "../net/network.h"
#include "../system/system.h"
#include "../version.h"
#include "../web/web_server.h"

// Arduino markiert neue Firmware sonst sofort beim Start als gültig (esp32-hal-misc.c).
// Wir entscheiden selbst nach dem Selbsttest.
extern "C" bool verifyRollbackLater() { return true; }

namespace ota {

namespace {

constexpr const char *kManifestUrl =
    "https://github.com/NilsAla/abfahrtstafel/releases/latest/download/manifest.json";
constexpr size_t kManifestMax = 4096;
constexpr size_t kChunk = 4096;
constexpr uint32_t kSelfTestTimeoutMs = 5 * 60 * 1000;
constexpr uint32_t kSelfTestFailTimeoutMs = 30 * 1000;  // Test-Build FW_SELFTEST_FAIL
constexpr uint32_t kFirstAutoCheckMs = 2 * 60 * 1000;  // nach dem Verbinden
constexpr uint32_t kAutoCheckEveryMs = 24 * 60 * 60 * 1000;
constexpr const char *kNvsNamespace = "ota";
constexpr const char *kNvsPendingTo = "pendingTo";  // Version, auf die zuletzt aktualisiert wurde
constexpr const char *kNvsBlocked = "blocked";      // zurückgerollte Version, nie wieder anbieten

#ifdef FW_SELFTEST_FAIL
constexpr bool kForceSelfTestFail = true;
#else
constexpr bool kForceSelfTestFail = false;
#endif

std::mutex sMutex;
abf::UpdateFlow sFlow(FW_VERSION);
abf::SelfTest sSelfTest(false, 0, false, 0);
String sLastResult;   // "ok" | "rolled_back" | ""
String sLastVersion;  // Zielversion des letzten Updates
bool sAutoCheckArmed = false;
uint32_t sNextAutoCheck = 0;

// --- Hilfen ----------------------------------------------------------------

class MbedSha256 : public abf::Sha256 {
 public:
  MbedSha256() {
    mbedtls_sha256_init(&ctx_);
    mbedtls_sha256_starts(&ctx_, 0);
  }
  ~MbedSha256() override { mbedtls_sha256_free(&ctx_); }
  void update(const uint8_t *data, size_t len) override { mbedtls_sha256_update(&ctx_, data, len); }
  void finish(uint8_t digest[32]) override { mbedtls_sha256_finish(&ctx_, digest); }

 private:
  mbedtls_sha256_context ctx_;
};

void setResult(abf::UpdateError e) {
  std::lock_guard<std::mutex> lock(sMutex);
  sFlow.fail(e);
}

// --- Tasks -----------------------------------------------------------------

void runCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    setResult(abf::UpdateError::NoNetwork);
    return;
  }
  const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
  const uint32_t maxSize = target ? target->size : 0;

  https::Body body;
  if (!https::get(kManifestUrl, kManifestMax, body)) {
    setResult(abf::UpdateError::ManifestDownload);
    return;
  }

  abf::Manifest m;
  const abf::ManifestError err = abf::parseManifest(body.data, body.len, maxSize, m);
  if (err != abf::ManifestError::None) {
    LOGW("ota", "Manifest ungültig (%s)", abf::manifestErrorName(err));
    setResult(abf::UpdateError::ManifestInvalid);
    return;
  }

  std::lock_guard<std::mutex> lock(sMutex);
  sFlow.checkSucceeded(m);
  if (sFlow.state() == abf::UpdateState::Available) {
    LOGI("ota", "Update verfügbar: %s → %s", FW_VERSION, m.version.c_str());
  } else if (sFlow.skippedBlocked()) {
    LOGI("ota", "Neueste Version %s wurde zurückgerollt und wird nicht angeboten", m.version.c_str());
  } else {
    LOGI("ota", "Firmware aktuell (neueste: %s)", m.version.c_str());
  }
}

void runInstall() {
  abf::Manifest m;
  {
    std::lock_guard<std::mutex> lock(sMutex);
    m = sFlow.offer();
  }
  LOGI("ota", "Installiere %s (%lu Bytes)", m.version.c_str(), static_cast<unsigned long>(m.size));

  const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
  if (!target || m.size > target->size) {
    setResult(abf::UpdateError::FlashWrite);
    return;
  }

  https::Connection http;
  int64_t len = 0;
  if (!https::open(m.url.c_str(), http, len)) {
    setResult(abf::UpdateError::Download);
    return;
  }
  if (len >= 0 && len != static_cast<int64_t>(m.size)) {
    LOGW("ota", "Content-Length %lld statt %lu", static_cast<long long>(len), static_cast<unsigned long>(m.size));
    setResult(abf::UpdateError::SizeMismatch);
    return;
  }

  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle);
  if (err != ESP_OK) {
    LOGE("ota", "esp_ota_begin: %s", esp_err_to_name(err));
    setResult(abf::UpdateError::FlashWrite);
    return;
  }

  MbedSha256 sha;
  abf::DownloadVerifier verifier(m.size, m.sha256, sha);
  std::unique_ptr<uint8_t[]> buf(new uint8_t[kChunk]);
  abf::UpdateError failure = abf::UpdateError::None;

  for (;;) {
    const int r = esp_http_client_read(http.h, reinterpret_cast<char *>(buf.get()), kChunk);
    if (r < 0) {
      failure = abf::UpdateError::Download;
      break;
    }
    if (r == 0) {
      if (!esp_http_client_is_complete_data_received(http.h)) failure = abf::UpdateError::Download;
      break;
    }
    if (!verifier.add(buf.get(), r)) {
      failure = abf::UpdateError::SizeMismatch;
      break;
    }
    err = esp_ota_write(handle, buf.get(), r);
    if (err != ESP_OK) {
      LOGE("ota", "esp_ota_write: %s", esp_err_to_name(err));
      failure = abf::UpdateError::FlashWrite;
      break;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    sFlow.progress(verifier.received());
  }

  if (failure == abf::UpdateError::None) {
    {
      std::lock_guard<std::mutex> lock(sMutex);
      sFlow.downloadFinished();
    }
    // Prüfsumme vor dem Umschalten prüfen
    failure = verifier.finish();
    if (failure == abf::UpdateError::HashMismatch) LOGE("ota", "SHA-256 stimmt nicht");
  }
  if (failure != abf::UpdateError::None) {
    esp_ota_abort(handle);
    LOGW("ota", "Update abgebrochen: %s", abf::updateErrorText(failure));
    setResult(failure);
    return;
  }

  err = esp_ota_end(handle);  // prüft Image-Header und Segment-Prüfsummen
  if (err != ESP_OK) {
    LOGE("ota", "esp_ota_end: %s", esp_err_to_name(err));
    setResult(err == ESP_ERR_OTA_VALIDATE_FAILED ? abf::UpdateError::ImageInvalid : abf::UpdateError::FlashWrite);
    return;
  }

  Preferences prefs;
  if (prefs.begin(kNvsNamespace, false)) {
    prefs.putString(kNvsPendingTo, m.version.c_str());
    prefs.end();
  }

  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK) {
    LOGE("ota", "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
    setResult(abf::UpdateError::FlashWrite);
    return;
  }

  {
    std::lock_guard<std::mutex> lock(sMutex);
    sFlow.verified();
  }
  LOGI("ota", "Update auf %s geschrieben, Neustart", m.version.c_str());
  sys::scheduleRestart(2000);  // Web-UI bekommt noch den Zustand „rebooting“ mit
}

enum class Job { Check, Install };

void taskMain(void *arg) {
  const Job job = static_cast<Job>(reinterpret_cast<uintptr_t>(arg));
  if (job == Job::Check) {
    runCheck();
  } else {
    runInstall();
  }
  LOGD("ota", "Task-Stack frei: %u Bytes", static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  vTaskDelete(nullptr);
}

bool spawn(Job job) {
  // TLS-Handshake braucht viel Stack; mbedTLS-Puffer selbst liegen auf dem Heap
  const BaseType_t ok = xTaskCreatePinnedToCore(taskMain, "ota", 12 * 1024,
                                                reinterpret_cast<void *>(static_cast<uintptr_t>(job)), 1, nullptr, 1);
  if (ok != pdPASS) {
    LOGE("ota", "Task konnte nicht gestartet werden");
    return false;
  }
  return true;
}

void readLastUpdate() {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, true)) return;  // Namespace existiert noch nicht
  const String pendingTo = prefs.getString(kNvsPendingTo, "");
  const String blocked = prefs.getString(kNvsBlocked, "");
  prefs.end();
  if (!blocked.isEmpty()) sFlow.blockVersion(blocked.c_str());
  if (pendingTo.isEmpty()) return;
  sLastVersion = pendingTo;
  if (pendingTo != FW_VERSION) {
    // Neue Firmware ist durchgefallen, Bootloader hat die alte gestartet
    sLastResult = "rolled_back";
    LOGW("ota", "Update auf %s fehlgeschlagen, %s läuft weiter (Rollback) – wird nicht erneut angeboten",
         pendingTo.c_str(), FW_VERSION);
    sFlow.blockVersion(pendingTo.c_str());
    if (prefs.begin(kNvsNamespace, false)) {
      prefs.remove(kNvsPendingTo);
      prefs.putString(kNvsBlocked, pendingTo);
      prefs.end();
    }
  }
  // pendingTo == FW_VERSION: Ergebnis entscheidet der Selbsttest
}

void clearPending() {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false)) return;
  prefs.remove(kNvsPendingTo);
  prefs.end();
}

}  // namespace

void begin() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  const bool pending = running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
                       state == ESP_OTA_IMG_PENDING_VERIFY;
  const uint32_t timeout = kForceSelfTestFail ? kSelfTestFailTimeoutMs : kSelfTestTimeoutMs;
  sSelfTest = abf::SelfTest(pending, timeout, kForceSelfTestFail, millis());
  if (pending) {
    LOGI("ota", "Neue Firmware %s, Selbsttest läuft (max. %lu s)%s", FW_VERSION,
         static_cast<unsigned long>(timeout / 1000), kForceSelfTestFail ? " – Test-Build: schlägt absichtlich fehl" : "");
  }
  readLastUpdate();
}

void loop() {
  const bool healthy = net::state() == abf::WifiState::Online && web::running();
  switch (sSelfTest.update(healthy, millis())) {
    case abf::SelfTest::Decision::MarkValid: {
      const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
      if (err == ESP_OK) {
        LOGI("ota", "Selbsttest bestanden, Firmware %s gültig", FW_VERSION);
      } else {
        LOGE("ota", "Markieren fehlgeschlagen: %s", esp_err_to_name(err));
      }
      clearPending();
      std::lock_guard<std::mutex> lock(sMutex);
      sLastResult = "ok";
      sLastVersion = FW_VERSION;
      break;
    }
    case abf::SelfTest::Decision::Rollback:
      LOGE("ota", "Selbsttest fehlgeschlagen (WLAN/Web-UI nicht bereit), Rollback");
      delay(200);
      esp_ota_mark_app_invalid_rollback_and_reboot();
      break;
    case abf::SelfTest::Decision::None:
      break;
  }

  // Täglicher Check: zeigt nur einen Hinweis, installiert nie selbst
  if (net::state() != abf::WifiState::Online || !config::get().updateCheck) return;
  const uint32_t now = millis();
  if (!sAutoCheckArmed) {
    sAutoCheckArmed = true;
    sNextAutoCheck = now + kFirstAutoCheckMs;
    return;
  }
  if (static_cast<int32_t>(now - sNextAutoCheck) < 0) return;
  sNextAutoCheck = now + kAutoCheckEveryMs;
  LOGI("ota", "Automatische Update-Prüfung");
  requestCheck();
}

bool requestCheck() {
  {
    std::lock_guard<std::mutex> lock(sMutex);
    if (!sFlow.startCheck()) return false;
  }
  if (spawn(Job::Check)) return true;
  setResult(abf::UpdateError::ManifestDownload);
  return false;
}

bool requestInstall() {
  {
    std::lock_guard<std::mutex> lock(sMutex);
    if (!sFlow.startInstall()) return false;
  }
  if (spawn(Job::Install)) return true;
  setResult(abf::UpdateError::FlashWrite);
  return false;
}

bool busy() {
  std::lock_guard<std::mutex> lock(sMutex);
  return sFlow.busy();
}

void buildStatus(JsonObject out) {
  std::lock_guard<std::mutex> lock(sMutex);
  out["current"] = FW_VERSION;
  out["state"] = abf::updateStateName(sFlow.state());
  out["busy"] = sFlow.busy();
  out["percent"] = sFlow.percent();
  if (sFlow.error() != abf::UpdateError::None) out["error"] = abf::updateErrorText(sFlow.error());
  if (sFlow.hasOffer()) {
    JsonObject offer = out["offer"].to<JsonObject>();
    offer["version"] = sFlow.offer().version;
    offer["notes"] = sFlow.offer().notes;
    offer["size"] = sFlow.offer().size;
  }
  if (!sLastResult.isEmpty()) {
    JsonObject last = out["last_update"].to<JsonObject>();
    last["result"] = sLastResult;
    last["version"] = sLastVersion;
  }
  if (sFlow.skippedBlocked()) out["skipped"] = sFlow.blockedVersion();
  out["selftest_running"] = sSelfTest.active();
  out["auto_check"] = config::get().updateCheck;
}

}  // namespace ota
