// Bootanimation: Zeitachse und Bewegungen (lib/dispmodel/src/boot_anim.h)
#include <boot_anim.h>
#include <unity.h>

using namespace abf::boot;

void setUp() {}
void tearDown() {}

static void test_bus_edges(void) {
  TEST_ASSERT_EQUAL_FLOAT(kBusStart, busX(0.0f));
  TEST_ASSERT_EQUAL_FLOAT(kBusStart, busX(kEnter0));
  TEST_ASSERT_EQUAL_FLOAT(kBusStop, busX(kEnter1));
  TEST_ASSERT_EQUAL_FLOAT(kBusStop, busX(kOnceCut));
}

static void test_bus_monotonic_and_decelerating(void) {
  float last = busX(kEnter0);
  float lastStep = 1e9f;
  for (float t = kEnter0 + 0.04f; t <= kEnter1; t += 0.04f) {
    const float x = busX(t);
    TEST_ASSERT_TRUE(x >= last);
    const float step = x - last;
    TEST_ASSERT_TRUE(step <= lastStep + 0.001f);  // ease-out: wird langsamer
    last = x;
    lastStep = step;
  }
}

static void test_reveal_follows_bus(void) {
  TEST_ASSERT_TRUE(revealEdge(0.0f) <= 0.0f);  // vor der Einfahrt nichts sichtbar
  TEST_ASSERT_EQUAL_FLOAT(busX(1.5f) - kRevealGap, revealEdge(1.5f));
  TEST_ASSERT_EQUAL_FLOAT(kBusStop - kRevealGap, revealEdge(kOnceEnd));
}

static void test_door(void) {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, doorOpen(kDoorOpen0 - 0.1f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, doorOpen(kDoorOpen1));
  TEST_ASSERT_TRUE(doorOpen(3.1f) > 0.0f && doorOpen(3.1f) < 1.0f);
}

static void test_frame_frozen_after_cut(void) {
  const Frame a = frameAt(kOnceCut);
  const Frame b = frameAt(kOnceEnd);
  TEST_ASSERT_EQUAL_FLOAT(a.busFront, b.busFront);
  TEST_ASSERT_EQUAL_FLOAT(a.lady.yHip, b.lady.yHip);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LadyMode::Walk), static_cast<int>(b.lady.mode));
}

static void test_lady_phases(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LadyMode::Sit), static_cast<int>(frameAt(1.0f).lady.mode));
  TEST_ASSERT_FALSE(frameAt(1.0f).lady.waving);  // Bus noch weit weg
  TEST_ASSERT_TRUE(frameAt(2.0f).lady.waving);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LadyMode::Stand), static_cast<int>(frameAt(3.8f).lady.mode));
}

static void test_should_play(void) {
  TEST_ASSERT_TRUE(shouldPlay(true, true, false, 80));
  TEST_ASSERT_FALSE(shouldPlay(false, true, false, 80));  // abgeschaltet
  TEST_ASSERT_FALSE(shouldPlay(true, false, false, 80));  // OTA, Rollback, Werksreset, Absturz
  TEST_ASSERT_FALSE(shouldPlay(true, true, true, 80));    // Einrichtung hat Vorrang
  TEST_ASSERT_FALSE(shouldPlay(true, true, false, 0));    // Display aus
}

static void test_finished(void) {
  TEST_ASSERT_FALSE(finished(1.0f, true));    // nie mitten in der Einfahrt abbrechen
  TEST_ASSERT_TRUE(finished(kEnter1, true));  // Abfahrten da: nach der Einfahrt weiter
  TEST_ASSERT_FALSE(finished(4.5f, false));
  TEST_ASSERT_TRUE(finished(kOnceEnd, false));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_bus_edges);
  RUN_TEST(test_bus_monotonic_and_decelerating);
  RUN_TEST(test_reveal_follows_bus);
  RUN_TEST(test_door);
  RUN_TEST(test_frame_frozen_after_cut);
  RUN_TEST(test_lady_phases);
  RUN_TEST(test_should_play);
  RUN_TEST(test_finished);
  return UNITY_END();
}
