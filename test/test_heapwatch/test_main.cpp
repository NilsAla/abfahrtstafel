// Heap-Überwachung: Schwellen, Hysterese, Neustart nach Dauer-Knappheit
#include <heap_watch.h>
#include <unity.h>

using namespace abf;
using Ev = HeapWatch::Event;

void setUp() {}
void tearDown() {}

#define ASSERT_EV(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))
#define ASSERT_LEVEL(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

constexpr size_t KB = 1024;

static void test_ok_stays_quiet(void) {
  HeapWatch w;
  ASSERT_EV(Ev::None, w.update(118 * KB, 90 * KB, 0));
  ASSERT_EV(Ev::None, w.update(100 * KB, 60 * KB, 5000));
  ASSERT_LEVEL(HeapLevel::Ok, w.level());
  TEST_ASSERT_EQUAL_STRING("ok", heapLevelName(w.level()));
}

static void test_low_by_free_or_block(void) {
  HeapWatch a;
  ASSERT_EV(Ev::BecameLow, a.update(39 * KB, 30 * KB, 0));
  HeapWatch b;
  ASSERT_EV(Ev::BecameLow, b.update(80 * KB, 23 * KB, 0));  // fragmentiert
  ASSERT_LEVEL(HeapLevel::Low, b.level());
}

static void test_hysteresis(void) {
  HeapWatch w;
  ASSERT_EV(Ev::BecameLow, w.update(39 * KB, 30 * KB, 0));
  // knapp über der Schwelle: bleibt Low, kein Flattern
  ASSERT_EV(Ev::None, w.update(41 * KB, 33 * KB, 5000));
  ASSERT_LEVEL(HeapLevel::Low, w.level());
  ASSERT_EV(Ev::Recovered, w.update(49 * KB, 33 * KB, 10000));
  ASSERT_LEVEL(HeapLevel::Ok, w.level());
}

static void test_critical_then_restart_once(void) {
  HeapWatch w;
  ASSERT_EV(Ev::BecameCritical, w.update(19 * KB, 15 * KB, 1000));
  ASSERT_EV(Ev::None, w.update(19 * KB, 15 * KB, 1000 + 10 * 60 * 1000 - 1));
  ASSERT_EV(Ev::RestartDue, w.update(19 * KB, 15 * KB, 1000 + 10 * 60 * 1000));
  ASSERT_EV(Ev::None, w.update(19 * KB, 15 * KB, 1000 + 20 * 60 * 1000));
}

static void test_short_critical_phase_no_restart(void) {
  HeapWatch w;
  ASSERT_EV(Ev::BecameCritical, w.update(30 * KB, 9 * KB, 0));
  // erholt sich (mit Hysterese) → Low, Uhr läuft von vorn
  ASSERT_EV(Ev::None, w.update(30 * KB, 19 * KB, 5 * 60 * 1000));
  ASSERT_LEVEL(HeapLevel::Low, w.level());
  ASSERT_EV(Ev::BecameCritical, w.update(30 * KB, 9 * KB, 9 * 60 * 1000));
  ASSERT_EV(Ev::None, w.update(30 * KB, 9 * KB, 15 * 60 * 1000));
  ASSERT_EV(Ev::RestartDue, w.update(30 * KB, 9 * KB, 19 * 60 * 1000));
}

static void test_millis_overflow(void) {
  HeapWatch w;
  const uint32_t start = 0xFFFFFFFFu - 1000;
  ASSERT_EV(Ev::BecameCritical, w.update(10 * KB, 5 * KB, start));
  ASSERT_EV(Ev::None, w.update(10 * KB, 5 * KB, start + 60 * 1000));
  ASSERT_EV(Ev::RestartDue, w.update(10 * KB, 5 * KB, start + 10 * 60 * 1000));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_ok_stays_quiet);
  RUN_TEST(test_low_by_free_or_block);
  RUN_TEST(test_hysteresis);
  RUN_TEST(test_critical_then_restart_once);
  RUN_TEST(test_short_critical_phase_no_restart);
  RUN_TEST(test_millis_overflow);
  return UNITY_END();
}
