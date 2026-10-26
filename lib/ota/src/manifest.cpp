#include "manifest.h"

#include <ArduinoJson.h>

#include "semver.h"

namespace abf {

namespace {

std::string truncateUtf8(const std::string &s, size_t maxBytes) {
  if (s.size() <= maxBytes) return s;
  size_t cut = maxBytes;
  // Nicht mitten in einer Multibyte-Sequenz schneiden (Folgebytes 10xxxxxx)
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
  return s.substr(0, cut) + "…";
}

bool isLowerHex(const std::string &s) {
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

}  // namespace

ManifestError parseManifest(const char *json, size_t len, uint32_t maxSize, Manifest &out) {
  JsonDocument filter;
  filter["version"] = true;
  filter["sha256"] = true;
  filter["size"] = true;
  filter["url"] = true;
  filter["notes"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter)) || !doc.is<JsonObject>()) {
    return ManifestError::Json;
  }

  Manifest m;
  if (!doc["version"].is<const char *>()) return ManifestError::Version;
  m.version = doc["version"].as<const char *>();
  SemVer v;
  if (!parseSemVer(m.version, v) || m.version[0] == 'v') return ManifestError::Version;

  if (!doc["sha256"].is<const char *>()) return ManifestError::Sha256;
  m.sha256 = doc["sha256"].as<const char *>();
  if (m.sha256.size() != 64 || !isLowerHex(m.sha256)) return ManifestError::Sha256;

  if (!doc["size"].is<uint32_t>()) return ManifestError::Size;
  m.size = doc["size"].as<uint32_t>();
  if (m.size == 0 || m.size > maxSize) return ManifestError::Size;

  if (!doc["url"].is<const char *>()) return ManifestError::Url;
  m.url = doc["url"].as<const char *>();
  const std::string expectedUrl = std::string(kReleaseDownloadPrefix) + "v" + m.version + "/firmware.bin";
  if (m.url != expectedUrl) return ManifestError::Url;

  if (doc["notes"].is<const char *>()) m.notes = truncateUtf8(doc["notes"].as<const char *>(), kMaxNotesLength);

  out = m;
  return ManifestError::None;
}

const char *manifestErrorName(ManifestError e) {
  switch (e) {
    case ManifestError::None:    return "ok";
    case ManifestError::Json:    return "json";
    case ManifestError::Version: return "version";
    case ManifestError::Sha256:  return "sha256";
    case ManifestError::Size:    return "size";
    case ManifestError::Url:     return "url";
  }
  return "?";
}

}  // namespace abf
