// WLAN-Ablaufsteuerung: Einrichtung, Reconnect, Fallback-Portal
#include <unity.h>
#include <wifi_supervisor.h>

using namespace abf;

void setUp() {}
void tearDown() {}

#define ASSERT_STATE(expected, sup) \
  TEST_ASSERT_EQUAL_STRING(wifiStateName(expected), wifiStateName((sup).state()))

static WifiInputs connected() {
  WifiInputs in;
  in.staConnected = true;
  return in;
}

static WifiInputs offline(bool clients = false) {
  WifiInputs in;
  in.portalHasClients = clients;
  return in;
}

static WifiSupervisor::Timing timing() {
  WifiSupervisor::Timing t;
  t.reconnectEveryMs = 30000;
  t.fallbackAfterMs = 180000;
  t.fallbackDurationMs = 300000;
  return t;
}

static void test_no_credentials_opens_portal(void) {
  WifiSupervisor s(timing());
  WifiActions a = s.begin(false, 0);
  ASSERT_STATE(WifiState::Provisioning, s);
  TEST_ASSERT_TRUE(a.startPortal);
  TEST_ASSERT_FALSE(a.startWeb);
  TEST_ASSERT_TRUE(s.portalOpen());

  // Provisioning hat kein Zeitlimit
  a = s.update(offline(), 24u * 3600 * 1000);
  ASSERT_STATE(WifiState::Provisioning, s);
  TEST_ASSERT_FALSE(a.any());

  a = s.update(connected(), 24u * 3600 * 1000 + 1);
  ASSERT_STATE(WifiState::Online, s);
  TEST_ASSERT_TRUE(a.stopPortal);
  TEST_ASSERT_TRUE(a.startWeb);
  TEST_ASSERT_TRUE(a.wentOnline);
  TEST_ASSERT_FALSE(s.portalOpen());
}

static void test_credentials_connect(void) {
  WifiSupervisor s(timing());
  WifiActions a = s.begin(true, 1000);
  ASSERT_STATE(WifiState::Connecting, s);
  TEST_ASSERT_TRUE(a.startWeb);
  TEST_ASSERT_TRUE(a.reconnect);
  TEST_ASSERT_FALSE(a.startPortal);

  a = s.update(connected(), 5000);
  ASSERT_STATE(WifiState::Online, s);
  TEST_ASSERT_TRUE(a.wentOnline);
  TEST_ASSERT_FALSE(a.startWeb);  // läuft schon
  TEST_ASSERT_FALSE(a.stopPortal);

  a = s.update(connected(), 999999);
  TEST_ASSERT_FALSE(a.any());
}

static void test_periodic_reconnect(void) {
  WifiSupervisor s(timing());
  s.begin(true, 0);
  TEST_ASSERT_FALSE(s.update(offline(), 29999).reconnect);
  TEST_ASSERT_TRUE(s.update(offline(), 30000).reconnect);
  TEST_ASSERT_FALSE(s.update(offline(), 30001).reconnect);
  TEST_ASSERT_TRUE(s.update(offline(), 60000).reconnect);
}

static void test_fallback_after_timeout(void) {
  WifiSupervisor s(timing());
  s.begin(true, 0);
  WifiActions a = s.update(offline(), 179999);
  ASSERT_STATE(WifiState::Connecting, s);
  a = s.update(offline(), 180000);
  ASSERT_STATE(WifiState::Fallback, s);
  TEST_ASSERT_TRUE(a.stopWeb);
  TEST_ASSERT_TRUE(a.startPortal);
  TEST_ASSERT_TRUE(s.portalOpen());

  // Im Fallback keine eigenen Reconnects
  for (uint32_t t = 180000; t < 480000; t += 1000) {
    TEST_ASSERT_FALSE(s.update(offline(), t).reconnect);
  }

  // Nach Ablauf zurück zu Connecting
  a = s.update(offline(), 480000);
  ASSERT_STATE(WifiState::Connecting, s);
  TEST_ASSERT_TRUE(a.stopPortal);
  TEST_ASSERT_TRUE(a.startWeb);
  TEST_ASSERT_TRUE(a.reconnect);

  // Zyklus wiederholt sich
  a = s.update(offline(), 480000 + 180000);
  ASSERT_STATE(WifiState::Fallback, s);
}

static void test_fallback_stays_open_with_clients(void) {
  WifiSupervisor s(timing());
  s.begin(true, 0);
  s.update(offline(), 180000);
  ASSERT_STATE(WifiState::Fallback, s);
  WifiActions a = s.update(offline(true), 600000);
  ASSERT_STATE(WifiState::Fallback, s);
  TEST_ASSERT_FALSE(a.any());
  a = s.update(offline(false), 600001);
  ASSERT_STATE(WifiState::Connecting, s);
  TEST_ASSERT_TRUE(a.stopPortal);
}

static void test_fallback_connects(void) {
  WifiSupervisor s(timing());
  s.begin(true, 0);
  s.update(offline(), 180000);
  WifiActions a = s.update(connected(), 200000);
  ASSERT_STATE(WifiState::Online, s);
  TEST_ASSERT_TRUE(a.stopPortal);
  TEST_ASSERT_TRUE(a.startWeb);
  TEST_ASSERT_TRUE(a.wentOnline);
}

static void test_online_loss_and_return(void) {
  WifiSupervisor s(timing());
  s.begin(true, 0);
  s.update(connected(), 1000);
  WifiActions a = s.update(offline(), 50000);
  ASSERT_STATE(WifiState::Connecting, s);
  TEST_ASSERT_FALSE(a.stopWeb);  // Web-UI bleibt für den Reconnect bestehen
  // Fallback-Timer zählt ab Verbindungsverlust
  s.update(offline(), 50000 + 179999);
  ASSERT_STATE(WifiState::Connecting, s);
  a = s.update(connected(), 50000 + 179999 + 1);
  ASSERT_STATE(WifiState::Online, s);
  TEST_ASSERT_TRUE(a.wentOnline);
}

static void test_millis_overflow(void) {
  WifiSupervisor s(timing());
  const uint32_t start = 0xFFFFFFFFu - 10000;
  s.begin(true, start);
  s.update(offline(), start + 179999);
  ASSERT_STATE(WifiState::Connecting, s);
  s.update(offline(), start + 180000);
  ASSERT_STATE(WifiState::Fallback, s);
}

static void test_ap_name(void) {
  const uint8_t mac[6] = {0x24, 0x6f, 0x28, 0x01, 0xab, 0x0c};
  TEST_ASSERT_EQUAL_STRING("Abfahrtstafel-AB0C", apName(mac).c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_no_credentials_opens_portal);
  RUN_TEST(test_credentials_connect);
  RUN_TEST(test_periodic_reconnect);
  RUN_TEST(test_fallback_after_timeout);
  RUN_TEST(test_fallback_stays_open_with_clients);
  RUN_TEST(test_fallback_connects);
  RUN_TEST(test_online_loss_and_return);
  RUN_TEST(test_millis_overflow);
  RUN_TEST(test_ap_name);
  return UNITY_END();
}
