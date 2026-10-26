// Log-Ringpuffer für /api/log
#include <log_ring.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

using namespace abf;

void setUp() {}
void tearDown() {}

static void add(LogRing &r, const char *s) { r.append(s, strlen(s)); }

static void test_empty(void) {
  char buf[32];
  LogRing r(buf, sizeof(buf));
  LogRing::ReadResult res = r.readSince(0);
  TEST_ASSERT_EQUAL_STRING("", res.text.c_str());
  TEST_ASSERT_EQUAL_UINT32(0, res.last);
  TEST_ASSERT_FALSE(res.gap);
}

static void test_append_and_read(void) {
  char buf[64];
  LogRing r(buf, sizeof(buf));
  add(r, "eins");
  add(r, "zwei");
  add(r, "drei");
  LogRing::ReadResult res = r.readSince(0);
  TEST_ASSERT_EQUAL_STRING("eins\nzwei\ndrei\n", res.text.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, res.last);

  res = r.readSince(2);
  TEST_ASSERT_EQUAL_STRING("drei\n", res.text.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, res.last);

  res = r.readSince(3);
  TEST_ASSERT_EQUAL_STRING("", res.text.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, res.last);
}

static void test_overflow_drops_oldest(void) {
  char buf[16];
  LogRing r(buf, sizeof(buf));
  add(r, "aaaa");  // 5 Bytes
  add(r, "bbbb");  // 10
  add(r, "cccc");  // 15
  add(r, "dddd");  // verdrängt aaaa
  TEST_ASSERT_EQUAL_UINT32(2, r.firstSeq());
  TEST_ASSERT_EQUAL_UINT32(4, r.lastSeq());
  LogRing::ReadResult res = r.readSince(0);
  TEST_ASSERT_EQUAL_STRING("bbbb\ncccc\ndddd\n", res.text.c_str());
  TEST_ASSERT_TRUE(res.gap);

  res = r.readSince(1);  // Zeile 1 kannte der Client schon → keine Lücke
  TEST_ASSERT_FALSE(res.gap);
  TEST_ASSERT_EQUAL_STRING("bbbb\ncccc\ndddd\n", res.text.c_str());
}

static void test_wraparound_many_lines(void) {
  char buf[50];
  LogRing r(buf, sizeof(buf));
  char line[16];
  for (int i = 1; i <= 1000; ++i) {
    int n = snprintf(line, sizeof(line), "Zeile %d", i);
    r.append(line, static_cast<size_t>(n));
    TEST_ASSERT_TRUE(r.used() <= sizeof(buf));
  }
  LogRing::ReadResult res = r.readSince(998);
  TEST_ASSERT_EQUAL_STRING("Zeile 999\nZeile 1000\n", res.text.c_str());
  TEST_ASSERT_EQUAL_UINT32(1000, res.last);
  TEST_ASSERT_FALSE(res.gap);
}

static void test_long_line_truncated(void) {
  char buf[8];
  LogRing r(buf, sizeof(buf));
  add(r, "abc");
  add(r, "0123456789");
  LogRing::ReadResult res = r.readSince(0);
  TEST_ASSERT_EQUAL_STRING("0123456\n", res.text.c_str());
  TEST_ASSERT_EQUAL_UINT32(2, r.firstSeq());
}

static void test_embedded_newlines_replaced(void) {
  char buf[32];
  LogRing r(buf, sizeof(buf));
  add(r, "a\nb\rc");
  add(r, "d");
  TEST_ASSERT_EQUAL_STRING("a b c\nd\n", r.readSince(0).text.c_str());
  TEST_ASSERT_EQUAL_UINT32(2, r.lastSeq());
}

static void test_since_beyond_last_resets(void) {
  // Client hat noch eine Sequenznummer von vor dem Neustart
  char buf[32];
  LogRing r(buf, sizeof(buf));
  add(r, "x");
  LogRing::ReadResult res = r.readSince(500);
  TEST_ASSERT_EQUAL_STRING("", res.text.c_str());
  TEST_ASSERT_EQUAL_UINT32(1, res.last);
}

static void test_empty_line(void) {
  char buf[16];
  LogRing r(buf, sizeof(buf));
  add(r, "");
  add(r, "x");
  TEST_ASSERT_EQUAL_STRING("\nx\n", r.readSince(0).text.c_str());
  TEST_ASSERT_EQUAL_STRING("x\n", r.readSince(1).text.c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_empty);
  RUN_TEST(test_append_and_read);
  RUN_TEST(test_overflow_drops_oldest);
  RUN_TEST(test_wraparound_many_lines);
  RUN_TEST(test_long_line_truncated);
  RUN_TEST(test_embedded_newlines_replaced);
  RUN_TEST(test_since_beyond_last_resets);
  RUN_TEST(test_empty_line);
  return UNITY_END();
}
