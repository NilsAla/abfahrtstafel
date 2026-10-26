// Update-Logik: SemVer, Manifest, Ablauf, Download-Prüfung, Selbsttest
#include <manifest.h>
#include <semver.h>
#include <string.h>
#include <unity.h>
#include <update_flow.h>

#include <string>

using namespace abf;

void setUp() {}
void tearDown() {}

#define ASSERT_ENUM(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

constexpr uint32_t kPartition = 6 * 1024 * 1024;

// Echtes Manifest von Release v0.1.0
static const char *kManifestV010 = R"({
  "version": "0.1.0",
  "sha256": "622299ecdcd2b1664dfc01a56319de1f083a196ac1856ed5ec6c5eeaeb1ec7da",
  "size": 1185008,
  "url": "https://github.com/NilsAla/abfahrtstafel/releases/download/v0.1.0/firmware.bin",
  "notes": "WLAN-Einrichtung, Weboberfläche mit Status, Einstellungen und Protokoll, Werksreset per BOOT-Taster"
})";

// --- SemVer ----------------------------------------------------------------

static void test_semver_parse(void) {
  SemVer v;
  TEST_ASSERT_TRUE(parseSemVer("1.2.3", v));
  TEST_ASSERT_EQUAL_UINT32(1, v.major);
  TEST_ASSERT_EQUAL_UINT32(2, v.minor);
  TEST_ASSERT_EQUAL_UINT32(3, v.patch);
  TEST_ASSERT_TRUE(parseSemVer("v0.2.0-rc.1+build.5", v));
  TEST_ASSERT_EQUAL_STRING("rc.1", v.pre.c_str());
  TEST_ASSERT_TRUE(parseSemVer("0.0.0-dev", v));

  const char *bad[] = {"", "1", "1.2", "1.2.3.4", "01.2.3", "1.02.3", "1.2.x", "1.2.3-", "1.2.3-01", "1.2.3-a..b",
                       "1.2.3-ä", "-1.2.3", "1.2.3 "};
  for (const char *b : bad) TEST_ASSERT_FALSE_MESSAGE(parseSemVer(b, v), b);
}

static void test_semver_order(void) {
  // Reihenfolge aus der SemVer-2.0-Spezifikation
  const char *ordered[] = {"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta", "1.0.0-beta.2",
                           "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0", "1.0.1", "1.1.0", "1.10.0", "2.0.0"};
  const size_t n = sizeof(ordered) / sizeof(ordered[0]);
  for (size_t i = 0; i + 1 < n; ++i) {
    TEST_ASSERT_TRUE_MESSAGE(isNewerVersion(ordered[i + 1], ordered[i]), ordered[i + 1]);
    TEST_ASSERT_FALSE_MESSAGE(isNewerVersion(ordered[i], ordered[i + 1]), ordered[i]);
  }
  TEST_ASSERT_FALSE(isNewerVersion("1.0.0", "1.0.0"));
  TEST_ASSERT_FALSE(isNewerVersion("1.0.0+b2", "1.0.0+b1"));  // Build-Metadaten zählen nicht
  TEST_ASSERT_TRUE(isNewerVersion("0.1.0", "0.0.0-dev"));     // lokaler Build sieht jedes Release als neu
  TEST_ASSERT_FALSE(isNewerVersion("kaputt", "0.1.0"));
  TEST_ASSERT_FALSE(isNewerVersion("0.2.0", "kaputt"));
}

// --- Manifest --------------------------------------------------------------

static ManifestError parse(const std::string &json, Manifest &m) {
  return parseManifest(json.c_str(), json.size(), kPartition, m);
}

static std::string manifestWith(const std::string &field, const std::string &value) {
  std::string base = R"({"version":"0.2.0","sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",)"
                     R"("size":1000,"url":"https://github.com/NilsAla/abfahrtstafel/releases/download/v0.2.0/firmware.bin",)"
                     R"("notes":"Hinweis"})";
  if (field.empty()) return base;
  const size_t key = base.find("\"" + field + "\":");
  const size_t start = key + field.size() + 3;
  size_t end = start;
  if (base[start] == '"') {
    end = base.find('"', start + 1) + 1;
  } else {
    while (base[end] != ',' && base[end] != '}') ++end;
  }
  return base.substr(0, start) + value + base.substr(end);
}

static void test_manifest_real_release(void) {
  Manifest m;
  ASSERT_ENUM(ManifestError::None, parse(kManifestV010, m));
  TEST_ASSERT_EQUAL_STRING("0.1.0", m.version.c_str());
  TEST_ASSERT_EQUAL_UINT32(1185008, m.size);
  TEST_ASSERT_EQUAL_STRING("622299ecdcd2b1664dfc01a56319de1f083a196ac1856ed5ec6c5eeaeb1ec7da", m.sha256.c_str());
  TEST_ASSERT_EQUAL_STRING("https://github.com/NilsAla/abfahrtstafel/releases/download/v0.1.0/firmware.bin",
                           m.url.c_str());
  TEST_ASSERT_TRUE(m.notes.find("Werksreset") != std::string::npos);
}

static void test_manifest_template_valid(void) {
  Manifest m;
  ASSERT_ENUM(ManifestError::None, parse(manifestWith("", ""), m));
  ASSERT_ENUM(ManifestError::None, parse(manifestWith("notes", "\"\""), m));
  ASSERT_ENUM(ManifestError::None, parse(manifestWith("notes", "null"), m));
  TEST_ASSERT_EQUAL_STRING("", m.notes.c_str());
}

static void test_manifest_rejects(void) {
  Manifest m;
  ASSERT_ENUM(ManifestError::Json, parse("", m));
  ASSERT_ENUM(ManifestError::Json, parse("[1,2]", m));
  ASSERT_ENUM(ManifestError::Json, parse("<html>Not Found</html>", m));
  ASSERT_ENUM(ManifestError::Version, parse(manifestWith("version", "\"v0.2.0\""), m));
  ASSERT_ENUM(ManifestError::Version, parse(manifestWith("version", "\"0.2\""), m));
  ASSERT_ENUM(ManifestError::Version, parse(manifestWith("version", "020"), m));
  ASSERT_ENUM(ManifestError::Sha256, parse(manifestWith("sha256", "\"abc\""), m));
  ASSERT_ENUM(ManifestError::Sha256,
              parse(manifestWith("sha256", "\"0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef\""), m));
  ASSERT_ENUM(ManifestError::Size, parse(manifestWith("size", "0"), m));
  ASSERT_ENUM(ManifestError::Size, parse(manifestWith("size", "-5"), m));
  ASSERT_ENUM(ManifestError::Size, parse(manifestWith("size", "\"1000\""), m));
  ASSERT_ENUM(ManifestError::Size, parse(manifestWith("size", std::to_string(kPartition + 1)), m));
  ASSERT_ENUM(ManifestError::None, parse(manifestWith("size", std::to_string(kPartition)), m));
  // Fremde Quelle, falsche Version im Pfad, HTTP statt HTTPS, latest statt versioniert
  const char *badUrls[] = {
      "\"https://evil.example/firmware.bin\"",
      "\"https://github.com/NilsAla/abfahrtstafel/releases/download/v0.1.9/firmware.bin\"",
      "\"http://github.com/NilsAla/abfahrtstafel/releases/download/v0.2.0/firmware.bin\"",
      "\"https://github.com/NilsAla/abfahrtstafel/releases/latest/download/firmware.bin\"",
      "\"https://github.com/NilsAla/abfahrtstafel/releases/download/v0.2.0/firmware.bin?x=1\"",
      "\"https://github.com/Other/abfahrtstafel/releases/download/v0.2.0/firmware.bin\"",
  };
  for (const char *u : badUrls) ASSERT_ENUM(ManifestError::Url, parse(manifestWith("url", u), m));
}

static void test_manifest_notes_truncated_utf8(void) {
  std::string notes;
  while (notes.size() < kMaxNotesLength + 10) notes += "ä";  // 2 Byte je Zeichen
  notes = "x" + notes;                                       // Grenze fällt mitten in ein Zeichen
  Manifest m;
  ASSERT_ENUM(ManifestError::None, parse(manifestWith("notes", "\"" + notes + "\""), m));
  TEST_ASSERT_TRUE(m.notes.size() <= kMaxNotesLength + strlen("…"));
  const std::string body = m.notes.substr(0, m.notes.size() - strlen("…"));
  TEST_ASSERT_EQUAL_UINT32(0, (body.size() - 1) % 2);  // nur ganze 2-Byte-Zeichen nach dem 'x'
  TEST_ASSERT_EQUAL_STRING("…", m.notes.substr(m.notes.size() - strlen("…")).c_str());
}

// --- Ablauf ----------------------------------------------------------------

static Manifest offer(const char *version, uint32_t size = 1000) {
  Manifest m;
  m.version = version;
  m.size = size;
  return m;
}

static void test_flow_up_to_date(void) {
  UpdateFlow f("0.2.0");
  TEST_ASSERT_TRUE(f.startCheck());
  TEST_ASSERT_TRUE(f.busy());
  TEST_ASSERT_FALSE(f.startCheck());  // läuft schon
  f.checkSucceeded(offer("0.2.0"));
  ASSERT_ENUM(UpdateState::UpToDate, f.state());
  TEST_ASSERT_FALSE(f.hasOffer());
  TEST_ASSERT_FALSE(f.startInstall());
}

static void test_flow_older_release_not_offered(void) {
  // Rollback-Gerät oder lokaler Build mit höherer Version
  UpdateFlow f("0.3.0");
  f.startCheck();
  f.checkSucceeded(offer("0.2.1"));
  ASSERT_ENUM(UpdateState::UpToDate, f.state());
}

static void test_flow_install_success(void) {
  UpdateFlow f("0.2.0");
  f.startCheck();
  f.checkSucceeded(offer("0.2.1", 2000));
  ASSERT_ENUM(UpdateState::Available, f.state());
  TEST_ASSERT_TRUE(f.hasOffer());
  TEST_ASSERT_EQUAL_STRING("0.2.1", f.offer().version.c_str());

  TEST_ASSERT_TRUE(f.startInstall());
  ASSERT_ENUM(UpdateState::Downloading, f.state());
  TEST_ASSERT_FALSE(f.startCheck());
  TEST_ASSERT_FALSE(f.startInstall());
  f.progress(500);
  TEST_ASSERT_EQUAL_UINT8(25, f.percent());
  f.progress(2000);
  TEST_ASSERT_EQUAL_UINT8(100, f.percent());
  f.downloadFinished();
  ASSERT_ENUM(UpdateState::Verifying, f.state());
  f.verified();
  ASSERT_ENUM(UpdateState::Rebooting, f.state());
  f.fail(UpdateError::Download);  // nach dem Umschalten keine Fehler mehr
  ASSERT_ENUM(UpdateState::Rebooting, f.state());
  TEST_ASSERT_TRUE(f.busy());
}

static void test_flow_error_keeps_offer_for_retry(void) {
  UpdateFlow f("0.2.0");
  f.startCheck();
  f.checkSucceeded(offer("0.2.1"));
  f.startInstall();
  f.progress(300);
  f.fail(UpdateError::HashMismatch);
  ASSERT_ENUM(UpdateState::Error, f.state());
  ASSERT_ENUM(UpdateError::HashMismatch, f.error());
  TEST_ASSERT_FALSE(f.busy());
  TEST_ASSERT_TRUE(f.hasOffer());
  TEST_ASSERT_TRUE(f.startInstall());
  TEST_ASSERT_EQUAL_UINT32(0, f.bytesWritten());
  ASSERT_ENUM(UpdateError::None, f.error());
}

static void test_flow_check_error(void) {
  UpdateFlow f("0.2.0");
  f.startCheck();
  f.fail(UpdateError::ManifestDownload);
  ASSERT_ENUM(UpdateState::Error, f.state());
  TEST_ASSERT_FALSE(f.startInstall());
  TEST_ASSERT_TRUE(f.startCheck());
}

static void test_flow_late_results_ignored(void) {
  UpdateFlow f("0.2.0");
  f.checkSucceeded(offer("9.9.9"));  // ohne laufende Prüfung
  ASSERT_ENUM(UpdateState::Idle, f.state());
  f.downloadFinished();
  f.verified();
  ASSERT_ENUM(UpdateState::Idle, f.state());
}

static void test_error_texts(void) {
  for (int e = static_cast<int>(UpdateError::NoNetwork); e <= static_cast<int>(UpdateError::ImageInvalid); ++e) {
    TEST_ASSERT_TRUE(strlen(updateErrorText(static_cast<UpdateError>(e))) > 5);
  }
  TEST_ASSERT_EQUAL_STRING("", updateErrorText(UpdateError::None));
}

// --- Download-Prüfung ------------------------------------------------------

class FakeSha : public Sha256 {
 public:
  void update(const uint8_t *data, size_t len) override { seen.append(reinterpret_cast<const char *>(data), len); }
  void finish(uint8_t digest[32]) override {
    // Pseudo-Hash: Länge und Summe der Bytes reichen, um Verfälschungen im Test zu erkennen
    uint32_t sum = 0;
    for (unsigned char c : seen) sum += c;
    memset(digest, 0, 32);
    digest[0] = static_cast<uint8_t>(seen.size());
    digest[1] = static_cast<uint8_t>(sum);
    digest[2] = static_cast<uint8_t>(sum >> 8);
  }
  std::string seen;
};

static std::string fakeDigestHex(const std::string &data) {
  FakeSha s;
  s.update(reinterpret_cast<const uint8_t *>(data.data()), data.size());
  uint8_t d[32];
  s.finish(d);
  return toHex(d, 32);
}

static void feed(DownloadVerifier &v, const std::string &data, size_t chunk) {
  for (size_t i = 0; i < data.size(); i += chunk) {
    const size_t n = data.size() - i < chunk ? data.size() - i : chunk;
    v.add(reinterpret_cast<const uint8_t *>(data.data() + i), n);
  }
}

static void test_verifier_ok_in_chunks(void) {
  const std::string data = "Firmware-Inhalt 0123456789";
  FakeSha sha;
  DownloadVerifier v(static_cast<uint32_t>(data.size()), fakeDigestHex(data), sha);
  feed(v, data, 7);
  TEST_ASSERT_EQUAL_UINT32(data.size(), v.received());
  ASSERT_ENUM(UpdateError::None, v.finish());
  TEST_ASSERT_EQUAL_STRING(data.c_str(), sha.seen.c_str());
}

static void test_verifier_truncated(void) {
  const std::string data = "Firmware-Inhalt";
  FakeSha sha;
  DownloadVerifier v(static_cast<uint32_t>(data.size()), fakeDigestHex(data), sha);
  feed(v, data.substr(0, 10), 4);
  ASSERT_ENUM(UpdateError::SizeMismatch, v.finish());
}

static void test_verifier_overflow_rejected(void) {
  const std::string data = "Firmware";
  FakeSha sha;
  DownloadVerifier v(static_cast<uint32_t>(data.size()), fakeDigestHex(data), sha);
  TEST_ASSERT_TRUE(v.add(reinterpret_cast<const uint8_t *>("Firm"), 4));
  TEST_ASSERT_FALSE(v.add(reinterpret_cast<const uint8_t *>("ware!"), 5));
  TEST_ASSERT_FALSE(v.add(reinterpret_cast<const uint8_t *>("w"), 1));  // bleibt abgelehnt
  ASSERT_ENUM(UpdateError::SizeMismatch, v.finish());
}

static void test_verifier_hash_mismatch(void) {
  const std::string data = "Firmware";
  FakeSha sha;
  DownloadVerifier v(static_cast<uint32_t>(data.size()), fakeDigestHex(data), sha);
  feed(v, "Firmwarf", 3);  // gleiche Länge, ein Byte anders
  ASSERT_ENUM(UpdateError::HashMismatch, v.finish());
}

static void test_to_hex(void) {
  const uint8_t d[] = {0x00, 0x0f, 0xa5, 0xff};
  TEST_ASSERT_EQUAL_STRING("000fa5ff", toHex(d, sizeof(d)).c_str());
}

// --- Selbsttest ------------------------------------------------------------

static void test_selftest_not_pending(void) {
  SelfTest t(false, 1000, false, 0);
  TEST_ASSERT_FALSE(t.active());
  ASSERT_ENUM(SelfTest::Decision::None, t.update(false, 999999));
}

static void test_selftest_marks_valid_when_healthy(void) {
  SelfTest t(true, 300000, false, 1000);
  ASSERT_ENUM(SelfTest::Decision::None, t.update(false, 5000));
  ASSERT_ENUM(SelfTest::Decision::MarkValid, t.update(true, 12000));
  TEST_ASSERT_FALSE(t.active());
  ASSERT_ENUM(SelfTest::Decision::None, t.update(false, 999999));  // nur einmal
}

static void test_selftest_rollback_on_timeout(void) {
  SelfTest t(true, 300000, false, 1000);
  ASSERT_ENUM(SelfTest::Decision::None, t.update(false, 300999));
  ASSERT_ENUM(SelfTest::Decision::Rollback, t.update(false, 301000));
  ASSERT_ENUM(SelfTest::Decision::None, t.update(true, 301001));
}

static void test_selftest_force_fail(void) {
  SelfTest t(true, 30000, true, 0);
  ASSERT_ENUM(SelfTest::Decision::None, t.update(true, 29999));
  ASSERT_ENUM(SelfTest::Decision::Rollback, t.update(true, 30000));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_semver_parse);
  RUN_TEST(test_semver_order);
  RUN_TEST(test_manifest_real_release);
  RUN_TEST(test_manifest_template_valid);
  RUN_TEST(test_manifest_rejects);
  RUN_TEST(test_manifest_notes_truncated_utf8);
  RUN_TEST(test_flow_up_to_date);
  RUN_TEST(test_flow_older_release_not_offered);
  RUN_TEST(test_flow_install_success);
  RUN_TEST(test_flow_error_keeps_offer_for_retry);
  RUN_TEST(test_flow_check_error);
  RUN_TEST(test_flow_late_results_ignored);
  RUN_TEST(test_error_texts);
  RUN_TEST(test_verifier_ok_in_chunks);
  RUN_TEST(test_verifier_truncated);
  RUN_TEST(test_verifier_overflow_rejected);
  RUN_TEST(test_verifier_hash_mismatch);
  RUN_TEST(test_to_hex);
  RUN_TEST(test_selftest_not_pending);
  RUN_TEST(test_selftest_marks_valid_when_healthy);
  RUN_TEST(test_selftest_rollback_on_timeout);
  RUN_TEST(test_selftest_force_fail);
  return UNITY_END();
}
