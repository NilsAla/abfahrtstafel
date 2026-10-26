// Taster-Entprellung und Halte-Erkennung (Factory Reset)
#include <hold_button.h>
#include <unity.h>

using namespace abf;
using Ev = HoldButton::Event;

void setUp() {}
void tearDown() {}

#define ASSERT_EV(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

static void test_debounce_press_and_release(void) {
  HoldButton b(10000, 50);
  ASSERT_EV(Ev::None, b.update(true, 1000));
  ASSERT_EV(Ev::None, b.update(true, 1049));
  ASSERT_EV(Ev::Pressed, b.update(true, 1050));
  TEST_ASSERT_TRUE(b.pressed());
  ASSERT_EV(Ev::None, b.update(false, 2000));
  ASSERT_EV(Ev::Released, b.update(false, 2050));
  TEST_ASSERT_FALSE(b.pressed());
}

static void test_bounce_ignored(void) {
  HoldButton b(10000, 50);
  for (uint32_t t = 0; t < 200; t += 10) {
    ASSERT_EV(Ev::None, b.update((t / 10) % 2 == 0, t));
  }
  TEST_ASSERT_FALSE(b.pressed());
}

static void test_hold_fires_once(void) {
  HoldButton b(10000, 50);
  b.update(true, 0);
  ASSERT_EV(Ev::Pressed, b.update(true, 50));
  ASSERT_EV(Ev::None, b.update(true, 9999));
  TEST_ASSERT_EQUAL_UINT32(9999, b.heldMs(9999));  // ab dem ersten Kontakt gemessen
  ASSERT_EV(Ev::HoldReached, b.update(true, 10000));
  ASSERT_EV(Ev::None, b.update(true, 20000));
  ASSERT_EV(Ev::None, b.update(true, 30000));
}

static void test_short_press_no_hold(void) {
  HoldButton b(10000, 50);
  b.update(true, 0);
  b.update(true, 50);
  b.update(false, 5000);
  ASSERT_EV(Ev::Released, b.update(false, 5050));
  ASSERT_EV(Ev::None, b.update(false, 20000));
  TEST_ASSERT_EQUAL_UINT32(0, b.heldMs(20000));
}

static void test_hold_rearms_after_release(void) {
  HoldButton b(1000, 50);
  b.update(true, 0);
  b.update(true, 50);
  ASSERT_EV(Ev::HoldReached, b.update(true, 1000));
  b.update(false, 1100);
  ASSERT_EV(Ev::Released, b.update(false, 1150));
  b.update(true, 2000);
  ASSERT_EV(Ev::Pressed, b.update(true, 2050));
  ASSERT_EV(Ev::None, b.update(true, 2999));
  ASSERT_EV(Ev::HoldReached, b.update(true, 3000));
}

static void test_millis_overflow(void) {
  HoldButton b(10000, 50);
  const uint32_t start = 0xFFFFFFFFu - 3000;
  b.update(true, start);
  ASSERT_EV(Ev::Pressed, b.update(true, start + 50));
  ASSERT_EV(Ev::None, b.update(true, start + 9000));       // läuft über 0
  ASSERT_EV(Ev::HoldReached, b.update(true, start + 10000));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_debounce_press_and_release);
  RUN_TEST(test_bounce_ignored);
  RUN_TEST(test_hold_fires_once);
  RUN_TEST(test_short_press_no_hold);
  RUN_TEST(test_hold_rearms_after_release);
  RUN_TEST(test_millis_overflow);
  return UNITY_END();
}
