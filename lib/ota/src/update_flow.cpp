#include "update_flow.h"

#include "semver.h"

namespace abf {

const char *updateStateName(UpdateState s) {
  switch (s) {
    case UpdateState::Idle:        return "idle";
    case UpdateState::Checking:    return "checking";
    case UpdateState::UpToDate:    return "up_to_date";
    case UpdateState::Available:   return "available";
    case UpdateState::Downloading: return "downloading";
    case UpdateState::Verifying:   return "verifying";
    case UpdateState::Rebooting:   return "rebooting";
    case UpdateState::Error:       return "error";
  }
  return "unknown";
}

const char *updateErrorText(UpdateError e) {
  switch (e) {
    case UpdateError::None:             return "";
    case UpdateError::NoNetwork:        return "Keine WLAN-Verbindung.";
    case UpdateError::ManifestDownload: return "Update-Server nicht erreichbar. Bitte später erneut versuchen.";
    case UpdateError::ManifestInvalid:  return "Die Update-Informationen sind fehlerhaft. Bitte später erneut versuchen.";
    case UpdateError::Download:         return "Download abgebrochen. Bitte erneut versuchen.";
    case UpdateError::SizeMismatch:     return "Download unvollständig. Bitte erneut versuchen.";
    case UpdateError::HashMismatch:     return "Download beschädigt (Prüfsumme falsch). Bitte erneut versuchen.";
    case UpdateError::FlashWrite:       return "Speichern der neuen Firmware fehlgeschlagen.";
    case UpdateError::ImageInvalid:     return "Die heruntergeladene Firmware ist ungültig.";
  }
  return "Unbekannter Fehler.";
}

bool UpdateFlow::busy() const {
  return state_ == UpdateState::Checking || state_ == UpdateState::Downloading ||
         state_ == UpdateState::Verifying || state_ == UpdateState::Rebooting;
}

bool UpdateFlow::startCheck() {
  if (busy()) return false;
  state_ = UpdateState::Checking;
  error_ = UpdateError::None;
  return true;
}

void UpdateFlow::checkSucceeded(const Manifest &m) {
  if (state_ != UpdateState::Checking) return;
  if (isNewerVersion(m.version, current_)) {
    offer_ = m;
    hasOffer_ = true;
    state_ = UpdateState::Available;
  } else {
    hasOffer_ = false;
    offer_ = Manifest();
    state_ = UpdateState::UpToDate;
  }
}

bool UpdateFlow::startInstall() {
  if (busy() || !hasOffer_) return false;
  state_ = UpdateState::Downloading;
  error_ = UpdateError::None;
  written_ = 0;
  return true;
}

void UpdateFlow::progress(uint32_t bytesWritten) {
  if (state_ == UpdateState::Downloading) written_ = bytesWritten;
}

void UpdateFlow::downloadFinished() {
  if (state_ == UpdateState::Downloading) state_ = UpdateState::Verifying;
}

void UpdateFlow::verified() {
  if (state_ == UpdateState::Verifying) state_ = UpdateState::Rebooting;
}

void UpdateFlow::fail(UpdateError e) {
  if (state_ == UpdateState::Rebooting) return;
  state_ = UpdateState::Error;
  error_ = e;
}

uint8_t UpdateFlow::percent() const {
  if (!hasOffer_ || offer_.size == 0) return 0;
  if (state_ == UpdateState::Verifying || state_ == UpdateState::Rebooting) return 100;
  const uint64_t p = static_cast<uint64_t>(written_) * 100 / offer_.size;
  return static_cast<uint8_t>(p > 100 ? 100 : p);
}

bool DownloadVerifier::add(const uint8_t *data, size_t len) {
  if (overflow_ || len > size_ - received_) {
    overflow_ = true;
    return false;
  }
  hash_.update(data, len);
  received_ += static_cast<uint32_t>(len);
  return true;
}

UpdateError DownloadVerifier::finish() {
  if (overflow_ || received_ != size_) return UpdateError::SizeMismatch;
  uint8_t digest[32];
  hash_.finish(digest);
  return toHex(digest, sizeof(digest)) == sha_ ? UpdateError::None : UpdateError::HashMismatch;
}

std::string toHex(const uint8_t *data, size_t len) {
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(kHex[data[i] >> 4]);
    out.push_back(kHex[data[i] & 0x0f]);
  }
  return out;
}

SelfTest::Decision SelfTest::update(bool healthy, uint32_t now) {
  if (!active_) return Decision::None;
  if (healthy && !forceFail_) {
    active_ = false;
    return Decision::MarkValid;
  }
  if (now - start_ >= timeoutMs_) {
    active_ = false;
    return Decision::Rollback;
  }
  return Decision::None;
}

}  // namespace abf
