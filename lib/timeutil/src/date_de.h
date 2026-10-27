// Datum auf Deutsch für die Kopfzeile der Tafel, unabhängig von der C-Locale.
#pragma once

#include <stdio.h>
#include <time.h>

#include <string>

namespace abf {

// "Di 27.10."
inline std::string formatDateShort(const struct tm &tm) {
  static const char *const kDays[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  char buf[32];
  snprintf(buf, sizeof(buf), "%s %02d.%02d.", kDays[tm.tm_wday % 7], tm.tm_mday, tm.tm_mon + 1);
  return buf;
}

// "Dienstag, 27. Oktober"
inline std::string formatDateLong(const struct tm &tm) {
  static const char *const kDays[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch",
                                      "Donnerstag", "Freitag", "Samstag"};
  static const char *const kMonths[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli",
                                        "August", "September", "Oktober", "November", "Dezember"};
  char buf[64];
  snprintf(buf, sizeof(buf), "%s, %d. %s", kDays[tm.tm_wday % 7], tm.tm_mday, kMonths[tm.tm_mon % 12]);
  return buf;
}

}  // namespace abf
