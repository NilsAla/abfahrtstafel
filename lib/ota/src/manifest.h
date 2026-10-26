// manifest.json aus dem GitHub-Release (erzeugt von .github/workflows/release.yml).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace abf {

// Firmware darf nur aus den Releases dieses Repos kommen.
constexpr const char *kReleaseDownloadPrefix = "https://github.com/NilsAla/abfahrtstafel/releases/download/";
constexpr size_t kMaxNotesLength = 1500;  // Bytes, gekürzt an UTF-8-Zeichengrenze

struct Manifest {
  std::string version;
  std::string sha256;  // 64 Hex-Zeichen, klein
  uint32_t size = 0;
  std::string url;
  std::string notes;
};

enum class ManifestError { None, Json, Version, Sha256, Size, Url };

// maxSize: Größe der OTA-Partition. Die URL muss zur Version passen (…/download/v<version>/firmware.bin).
ManifestError parseManifest(const char *json, size_t len, uint32_t maxSize, Manifest &out);

const char *manifestErrorName(ManifestError e);

}  // namespace abf
