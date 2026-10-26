#include "semver.h"

#include <vector>

namespace abf {

namespace {

bool isDigits(const std::string &s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

bool parseNumber(const std::string &s, uint32_t &out) {
  if (!isDigits(s) || (s.size() > 1 && s[0] == '0') || s.size() > 9) return false;
  out = 0;
  for (char c : s) out = out * 10 + static_cast<uint32_t>(c - '0');
  return true;
}

std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> parts;
  size_t start = 0;
  for (;;) {
    const size_t pos = s.find(sep, start);
    parts.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
    if (pos == std::string::npos) break;
    start = pos + 1;
  }
  return parts;
}

bool validIdentifier(const std::string &id) {
  if (id.empty()) return false;
  for (char c : id) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
    if (!ok) return false;
  }
  return !(isDigits(id) && id.size() > 1 && id[0] == '0');
}

int compareIdentifier(const std::string &a, const std::string &b) {
  const bool na = isDigits(a);
  const bool nb = isDigits(b);
  if (na && nb) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    return a.compare(b) < 0 ? -1 : (a == b ? 0 : 1);
  }
  if (na) return -1;  // numerisch < alphanumerisch
  if (nb) return 1;
  const int c = a.compare(b);
  return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

}  // namespace

bool parseSemVer(const std::string &text, SemVer &out) {
  std::string s = text;
  if (!s.empty() && s[0] == 'v') s.erase(0, 1);
  const size_t plus = s.find('+');
  if (plus != std::string::npos) s.erase(plus);

  std::string core = s;
  std::string pre;
  const size_t dash = s.find('-');
  if (dash != std::string::npos) {
    core = s.substr(0, dash);
    pre = s.substr(dash + 1);
    for (const std::string &id : split(pre, '.')) {
      if (!validIdentifier(id)) return false;
    }
  }

  const std::vector<std::string> parts = split(core, '.');
  if (parts.size() != 3) return false;
  SemVer v;
  if (!parseNumber(parts[0], v.major) || !parseNumber(parts[1], v.minor) || !parseNumber(parts[2], v.patch)) {
    return false;
  }
  v.pre = pre;
  out = v;
  return true;
}

int compareSemVer(const SemVer &a, const SemVer &b) {
  if (a.major != b.major) return a.major < b.major ? -1 : 1;
  if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
  if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
  if (a.pre.empty() || b.pre.empty()) {
    if (a.pre.empty() && b.pre.empty()) return 0;
    return a.pre.empty() ? 1 : -1;  // Release > Pre-Release
  }
  const std::vector<std::string> pa = split(a.pre, '.');
  const std::vector<std::string> pb = split(b.pre, '.');
  for (size_t i = 0; i < pa.size() && i < pb.size(); ++i) {
    const int c = compareIdentifier(pa[i], pb[i]);
    if (c != 0) return c;
  }
  if (pa.size() == pb.size()) return 0;
  return pa.size() < pb.size() ? -1 : 1;
}

bool isNewerVersion(const std::string &candidate, const std::string &current) {
  SemVer a;
  SemVer b;
  if (!parseSemVer(candidate, a) || !parseSemVer(current, b)) return false;
  return compareSemVer(a, b) > 0;
}

}  // namespace abf
