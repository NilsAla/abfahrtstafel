#include "http_date.h"

#include <string.h>

namespace abf {

namespace {

bool num(const char *&p, int digits, int &out) {
  out = 0;
  for (int i = 0; i < digits; ++i, ++p) {
    if (*p < '0' || *p > '9') return false;
    out = out * 10 + (*p - '0');
  }
  return true;
}

bool lit(const char *&p, char c) {
  if (*p != c) return false;
  ++p;
  return true;
}

// Tage seit 1970-01-01 (proleptischer Gregorianischer Kalender, H. Hinnant)
int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

int daysInMonth(int y, int m) {
  static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return m == 2 && leap ? 29 : kDays[m - 1];
}

}  // namespace

bool parseHttpDate(const char *s, int64_t &epoch) {
  static const char *kMonths = "JanFebMarAprMayJunJulAugSepOctNovDec";
  if (!s || strlen(s) < 29) return false;
  const char *p = s + 3;  // Wochentag wird nicht geprüft
  int day, year, hh, mm, ss;
  if (!lit(p, ',') || !lit(p, ' ') || !num(p, 2, day) || !lit(p, ' ')) return false;
  int month = 0;
  for (int i = 0; i < 12; ++i) {
    if (strncmp(p, kMonths + 3 * i, 3) == 0) month = i + 1;
  }
  if (month == 0) return false;
  p += 3;
  if (!lit(p, ' ') || !num(p, 4, year) || !lit(p, ' ') || !num(p, 2, hh) || !lit(p, ':') || !num(p, 2, mm) ||
      !lit(p, ':') || !num(p, 2, ss) || strncmp(p, " GMT", 4) != 0) {
    return false;
  }
  if (day < 1 || day > daysInMonth(year, month) || hh > 23 || mm > 59 || ss > 60) return false;
  epoch = daysFromCivil(year, month, day) * 86400 + hh * 3600 + mm * 60 + ss;
  return true;
}

}  // namespace abf
