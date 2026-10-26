// SemVer 2.0: Parsen und Vorrang-Vergleich (Build-Metadaten werden ignoriert).
#pragma once

#include <stdint.h>

#include <string>

namespace abf {

struct SemVer {
  uint32_t major = 0;
  uint32_t minor = 0;
  uint32_t patch = 0;
  std::string pre;  // ohne '-', leer = Release
};

// Akzeptiert optional ein führendes 'v'. Führende Nullen sind wie in SemVer verboten.
bool parseSemVer(const std::string &text, SemVer &out);

// <0: a älter, 0: gleich, >0: a neuer
int compareSemVer(const SemVer &a, const SemVer &b);

// true, wenn candidate eine neuere Version als current ist. Unparsebare Versionen → false.
bool isNewerVersion(const std::string &candidate, const std::string &current);

}  // namespace abf
