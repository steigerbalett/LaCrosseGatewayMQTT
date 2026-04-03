#include "AccessPoint.h"
#include <DNSServer.h>

static DNSServer dnsServer;

AccessPoint::AccessPoint(IPAddress ip, IPAddress gateway, IPAddress subnet, String ssidPrefix) {
  m_ip = ip;
  m_gateway = gateway;
  m_subnet = subnet;
  m_ssidPrefix = ssidPrefix;
}

void AccessPoint::SetLogItemCallback(LogItemCallbackType *callback) {
  m_logItemCallback = callback;
}

void AccessPoint::Begin(int autoClose) {
  if (m_logItemCallback != NULL) m_logItemCallback("Starting ...");

  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WiFiMode::WIFI_AP);
  delay(100);

  // RICHTIGE Reihenfolge: erst Config, dann DHCP starten
  wifi_softap_dhcps_stop();
  WiFi.softAPConfig(m_ip, m_ip, m_subnet);
  wifi_softap_dhcps_start();

  String ssid = m_ssidPrefix + "_" + String((unsigned int)ESP.getChipId());
  WiFi.softAP(ssid.c_str());
  delay(500);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", m_ip);

  if (m_logItemCallback != NULL) m_logItemCallback("running, SSID=" + ssid);

  m_autoClose    = autoClose;
  m_startMillis  = millis();
  m_running      = true;
}

void AccessPoint::End() {
  if (m_logItemCallback != NULL) m_logItemCallback("Closing ...");
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  m_running  = false;
  m_autoClose = 0;
  WiFi.mode(WiFiMode::WIFI_STA);
  delay(100);
  if (m_logItemCallback != NULL) m_logItemCallback("closed");
}

void AccessPoint::Handle() {
  if (m_running) {
    dnsServer.processNextRequest();
    if (m_autoClose > 0) {
      if (millis() - m_startMillis > (uint32_t)m_autoClose * 1000UL) {
        End();
      }
    }
  }
}