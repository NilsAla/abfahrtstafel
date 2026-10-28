// Zeitzone Europe/Berlin (POSIX-TZ-String) inkl. Sommerzeitwechsel
#include <date_de.h>
#include <stdlib.h>
#include <time.h>
#include <time_berlin.h>
#include <unity.h>

using namespace abf;

void setUp() {}
void tearDown() {}

static struct tm local(time_t t) {
  struct tm tm {};
  localtime_r(&t, &tm);
  return tm;
}

static void test_winter_time(void) {
  struct tm tm = local(1798545600);  // 2026-12-29 12:00:00 UTC
  TEST_ASSERT_EQUAL_INT(13, tm.tm_hour);
  TEST_ASSERT_EQUAL_INT(0, tm.tm_isdst);
}

static void test_summer_time(void) {
  struct tm tm = local(1781092800);  // 2026-06-10 12:00:00 UTC
  TEST_ASSERT_EQUAL_INT(14, tm.tm_hour);
  TEST_ASSERT_GREATER_THAN_INT(0, tm.tm_isdst);
}

static void test_spring_forward_2026(void) {
  // Letzter Sonntag im März 2026 = 29.03.; 01:00 UTC springt 02:00 MEZ auf 03:00 MESZ
  struct tm before = local(1774745999);  // 00:59:59 UTC
  struct tm after = local(1774746000);   // 01:00:00 UTC
  TEST_ASSERT_EQUAL_INT(1, before.tm_hour);
  TEST_ASSERT_EQUAL_INT(59, before.tm_min);
  TEST_ASSERT_EQUAL_INT(3, after.tm_hour);
  TEST_ASSERT_EQUAL_INT(0, after.tm_min);
  TEST_ASSERT_EQUAL_INT(29, after.tm_mday);
}

static void test_fall_back_2026(void) {
  // Letzter Sonntag im Oktober 2026 = 25.10.; 01:00 UTC springt 03:00 MESZ auf 02:00 MEZ
  struct tm before = local(1792889999);  // 00:59:59 UTC
  struct tm after = local(1792890000);   // 01:00:00 UTC
  TEST_ASSERT_EQUAL_INT(2, before.tm_hour);
  TEST_ASSERT_EQUAL_INT(59, before.tm_min);
  TEST_ASSERT_GREATER_THAN_INT(0, before.tm_isdst);
  TEST_ASSERT_EQUAL_INT(2, after.tm_hour);
  TEST_ASSERT_EQUAL_INT(0, after.tm_min);
  TEST_ASSERT_EQUAL_INT(0, after.tm_isdst);
}

static void test_plausibility(void) {
  TEST_ASSERT_FALSE(isTimePlausible(0));
  TEST_ASSERT_FALSE(isTimePlausible(86400));  // 1970-01-02, Uhr ohne NTP
  TEST_ASSERT_FALSE(isTimePlausible(kPlausibleEpoch - 1));
  TEST_ASSERT_TRUE(isTimePlausible(kPlausibleEpoch));
  TEST_ASSERT_TRUE(isTimePlausible(1792890000));
}

static void test_date_german(void) {
  struct tm tm = local(1798545600);  // Dienstag, 2026-12-29 13:00 MEZ
  TEST_ASSERT_EQUAL_STRING("Di 29.12.", formatDateShort(tm).c_str());
  TEST_ASSERT_EQUAL_STRING("Dienstag, 29. Dezember", formatDateLong(tm).c_str());
  tm = local(1774746000);  // Sonntag, 2026-03-29 03:00 MESZ
  TEST_ASSERT_EQUAL_STRING("So 29.03.", formatDateShort(tm).c_str());
  TEST_ASSERT_EQUAL_STRING("Sonntag, 29. März", formatDateLong(tm).c_str());
  tm = local(1781092800);  // Mittwoch, 2026-06-10 – einstelliger Tag ohne führende Null im Langformat
  TEST_ASSERT_EQUAL_STRING("Mi 10.06.", formatDateShort(tm).c_str());
  tm = local(1767229200);  // Donnerstag, 2026-01-01 02:00 MEZ
  TEST_ASSERT_EQUAL_STRING("Donnerstag, 1. Januar", formatDateLong(tm).c_str());
}

int main(int, char **) {
  setenv("TZ", kTzBerlin, 1);
  tzset();
  UNITY_BEGIN();
  RUN_TEST(test_winter_time);
  RUN_TEST(test_summer_time);
  RUN_TEST(test_spring_forward_2026);
  RUN_TEST(test_fall_back_2026);
  RUN_TEST(test_plausibility);
  RUN_TEST(test_date_german);
  return UNITY_END();
}
