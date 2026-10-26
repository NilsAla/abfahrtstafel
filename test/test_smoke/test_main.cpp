// Platzhalter-Test: stellt sicher, dass env:native und die CI-Testkette funktionieren.
// Wird ab M1 durch echte Tests der Module in lib/ ergänzt.
#include <unity.h>

void setUp() {}
void tearDown() {}

static void test_toolchain(void) {
  TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_toolchain);
  return UNITY_END();
}
