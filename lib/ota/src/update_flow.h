// Ablauf eines Firmware-Updates (ohne Netzwerk/Flash) und Prüfung des Downloads.
//
//   Idle/UpToDate/Available/Error --startCheck--> Checking --> UpToDate | Available | Error
//   Available/Error(mit Angebot)  --startInstall--> Downloading --> Verifying --> Rebooting
//                                                        \-----------\--> Error (Angebot bleibt, erneuter Versuch möglich)
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "manifest.h"

namespace abf {

enum class UpdateState { Idle, Checking, UpToDate, Available, Downloading, Verifying, Rebooting, Error };

enum class UpdateError {
  None,
  NoNetwork,         // kein WLAN
  ManifestDownload,  // Manifest nicht abrufbar (HTTP/TLS)
  ManifestInvalid,   // Manifest kaputt oder unplausibel
  Download,          // Firmware-Download abgebrochen
  SizeMismatch,      // Größe passt nicht zum Manifest
  HashMismatch,      // SHA-256 passt nicht
  FlashWrite,        // Schreiben in die Update-Partition fehlgeschlagen
  ImageInvalid,      // Bootloader-Prüfung des Images fehlgeschlagen
};

const char *updateStateName(UpdateState s);   // Schlüssel für /api/update
const char *updateErrorText(UpdateError e);   // Klartext für die Web-UI

class UpdateFlow {
 public:
  explicit UpdateFlow(std::string currentVersion) : current_(std::move(currentVersion)) {}

  // Version, die schon einmal zurückgerollt wurde: wird nie wieder angeboten
  void blockVersion(const std::string &version) { blocked_ = version; }
  const std::string &blockedVersion() const { return blocked_; }

  bool startCheck();
  void checkSucceeded(const Manifest &m);
  bool startInstall();
  void progress(uint32_t bytesWritten);
  void downloadFinished();
  void verified();
  void fail(UpdateError e);

  UpdateState state() const { return state_; }
  UpdateError error() const { return error_; }
  bool busy() const;  // Prüfen/Download/Verifizieren/Neustart läuft
  bool hasOffer() const { return hasOffer_; }
  const Manifest &offer() const { return offer_; }
  uint32_t bytesWritten() const { return written_; }
  uint8_t percent() const;
  const std::string &currentVersion() const { return current_; }
  bool skippedBlocked() const { return skippedBlocked_; }  // letzte Prüfung fand nur die gesperrte Version

 private:
  std::string current_;
  std::string blocked_;
  bool skippedBlocked_ = false;
  UpdateState state_ = UpdateState::Idle;
  UpdateError error_ = UpdateError::None;
  bool hasOffer_ = false;
  Manifest offer_;
  uint32_t written_ = 0;
};

// Prüft Größe und SHA-256 eines gestreamten Downloads. Hash-Implementierung kommt von außen
// (Firmware: mbedTLS), damit diese Logik ohne Krypto-Bibliothek testbar bleibt.
class Sha256 {
 public:
  virtual ~Sha256() = default;
  virtual void update(const uint8_t *data, size_t len) = 0;
  virtual void finish(uint8_t digest[32]) = 0;
};

class DownloadVerifier {
 public:
  DownloadVerifier(uint32_t expectedSize, const std::string &expectedSha256Hex, Sha256 &hash)
      : size_(expectedSize), sha_(expectedSha256Hex), hash_(hash) {}

  // false, wenn mehr Daten kommen als angekündigt (Download sofort abbrechen)
  bool add(const uint8_t *data, size_t len);
  // None, SizeMismatch oder HashMismatch
  UpdateError finish();
  uint32_t received() const { return received_; }

 private:
  uint32_t size_;
  std::string sha_;
  Sha256 &hash_;
  uint32_t received_ = 0;
  bool overflow_ = false;
};

std::string toHex(const uint8_t *data, size_t len);

// Selbsttest nach einem OTA-Update: Erst wenn WLAN und Web-UI laufen, wird die Firmware
// als gültig markiert. Klappt das nicht innerhalb von timeoutMs → Rollback.
class SelfTest {
 public:
  enum class Decision { None, MarkValid, Rollback };

  // pendingVerify: Bootloader meldet ESP_OTA_IMG_PENDING_VERIFY. forceFail: Test-Build (FW_SELFTEST_FAIL).
  SelfTest(bool pendingVerify, uint32_t timeoutMs, bool forceFail, uint32_t now)
      : active_(pendingVerify), timeoutMs_(timeoutMs), forceFail_(forceFail), start_(now) {}

  Decision update(bool healthy, uint32_t now);
  bool active() const { return active_; }

 private:
  bool active_;
  uint32_t timeoutMs_;
  bool forceFail_;
  uint32_t start_;
};

}  // namespace abf
