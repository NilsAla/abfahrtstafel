// Design-Tokens gegen die Tabelle in docs/branding/BRANDING.md (Abschnitt 2)
#include <stdint.h>
#include <stdlib.h>
#include <tokens.h>
#include <unity.h>

using namespace abf::brand;

void setUp() {}
void tearDown() {}

// Testdaten: Zeilen der Tabelle „Themes (Tokens)“, Spalten Nacht | Tag, wörtlich übernommen
struct Row {
  const char *name;
  const char *night;
  const char *day;
  uint32_t ThemeTokens::*field;
};

static const Row kTable[] = {
    {"bg", "#14162E", "#EEF7FC", &ThemeTokens::bg},
    {"bg_display", "#0E1024", "#EAF4FA", &ThemeTokens::bgDisplay},
    {"bg_rest", "#0A0B1A", "#E1ECF4", &ThemeTokens::bgRest},
    {"card", "#1B1F3B", "#FFFFFF", &ThemeTokens::card},
    {"card_hot", "#163A44", "#DDF5F1", &ThemeTokens::cardHot},
    {"border", "#2E3466", "#CFE3EE", &ThemeTokens::border},
    {"text", "#FFF6E5", "#1B1F3B", &ThemeTokens::text},
    {"muted", "#A9ABC9", "#4A5075", &ThemeTokens::muted},
    {"teal_text", "#3DDCC9", "#0A7D71", &ThemeTokens::tealText},
    {"coral_text", "#FF6B78", "#C81F33", &ThemeTokens::coralText},
    {"teal_big", "#1FC8B5", "#0A8F80", &ThemeTokens::tealBig},
    {"coral_big", "#FF4F5E", "#E02D3C", &ThemeTokens::coralBig},
    {"badge_bg", "#FFF6E5", "#1B1F3B", &ThemeTokens::badgeBg},
    {"badge_text", "#1B1F3B", "#FFF6E5", &ThemeTokens::badgeText},
};

static uint32_t hex(const char *s) { return static_cast<uint32_t>(strtoul(s + 1, nullptr, 16)); }

static void test_theme_tokens_match_table(void) {
  for (const Row &r : kTable) {
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(hex(r.night), kNight.*r.field, r.name);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(hex(r.day), kDay.*r.field, r.name);
  }
}

static void test_every_token_covered(void) {
  // Neues Feld in ThemeTokens ohne Tabellenzeile fällt hier auf
  TEST_ASSERT_EQUAL(sizeof(ThemeTokens) / sizeof(uint32_t), sizeof(kTable) / sizeof(kTable[0]));
}

static void test_brand_colors(void) {
  TEST_ASSERT_EQUAL_HEX32(0x1B1F3B, kNavy);
  TEST_ASSERT_EQUAL_HEX32(0xFFC72C, kYellow);
  TEST_ASSERT_EQUAL_HEX32(0xFF4F5E, kCoral);
  TEST_ASSERT_EQUAL_HEX32(0x1FC8B5, kTeal);
  TEST_ASSERT_EQUAL_HEX32(0xFFF6E5, kCream);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_theme_tokens_match_table);
  RUN_TEST(test_every_token_covered);
  RUN_TEST(test_brand_colors);
  return UNITY_END();
}
