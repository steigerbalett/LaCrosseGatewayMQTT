#include "CaptivePortal.h"

// ============================================================
//  CaptivePortal.cpp  –  v4  (ESP8266WebServer, synchron)
//
//  Kein ESPAsyncWebServer / ESPAsyncTCP mehr!
//  Spart ~4-6 KB BSS/IRAM → Crash vor Serial.begin() behoben.
//
//  ESP8266WebServer ist synchron: m_server->handleClient() muss
//  im Loop (Handle()) regelmäßig aufgerufen werden. Das macht
//  die bestehende while-Schleife im INO bereits (ruft Handle() auf).
//
//  WiFi-Init-Phase: ets_delay_us() statt yield()/delay() –
//  verhindert panic(__yield) wenn SDK Interrupts deaktiviert hat.
// ============================================================

#define CP_DBG(msg)       do { Serial.print(F("[CP] ")); Serial.println(F(msg)); Serial.flush(); } while(0)
#define CP_DBGF(fmt, ...) do { Serial.printf("[CP] " fmt "\n", ##__VA_ARGS__); Serial.flush(); } while(0)
#define CP_HEAP()         do { Serial.printf("[CP] Heap=%u ContStack=%u\n", \
                                ESP.getFreeHeap(), ESP.getFreeContStack()); Serial.flush(); } while(0)

CaptivePortal::CaptivePortal()
  : m_server(nullptr), m_dns(nullptr),
    m_settings(nullptr), m_logger(nullptr),
    m_done(false), m_restartPending(false), m_doneSince(0),
    m_timeoutS(0), m_startMs(0), m_dnsActive(false) {}

CaptivePortal::~CaptivePortal() {
  delete m_server; m_server = nullptr;
  delete m_dns;    m_dns    = nullptr;
}

void CaptivePortal::Begin(Settings *settings, Logger *logger,
                          String apSSID, int timeoutS) {
  CP_DBG("=== Begin() v4 (ESP8266WebServer) ===");
  CP_HEAP();

  m_settings       = settings;
  m_logger         = logger;
  m_timeoutS       = timeoutS;
  m_startMs        = millis();
  m_done           = false;
  m_restartPending = false;
  m_doneSince      = 0;
  m_dnsActive      = false;
  m_scanHtml       = "";

  m_apSSID = (apSSID.length() == 0)
    ? "LaCrosseGW-" + String(ESP.getChipId(), HEX)
    : apSSID;

  m_logger->println("[CaptivePortal] Starting AP: " + m_apSSID);
  CP_DBGF("AP-SSID: %s", m_apSSID.c_str());

  // ── 1. WiFi reset ───────────────────────────────────────────
  // FIX: ets_delay_us statt yield()/delay() → kein panic(__yield)
  //      wenn SDK Interrupts waehrend WiFi-AP-Beacon deaktiviert
  CP_DBG("WiFi reset");
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true);
  ets_delay_us(200000UL);   // 200 ms – kein yield
  ESP.wdtFeed();

  // ── 2. AP+STA Modus ─────────────────────────────────────────
  // AP+STA: AP sichtbar UND Scan moeglich
  CP_DBG("WiFi mode WIFI_AP_STA");
  WiFi.mode(WIFI_AP_STA);
  ets_delay_us(100000UL);   // 100 ms
  ESP.wdtFeed();

  // ── 3. SoftAP konfigurieren ─────────────────────────────────
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  CP_DBGF("softAP start (SSID=%s)", m_apSSID.c_str());
  bool apOk = WiFi.softAP(m_apSSID.c_str());
  CP_DBGF("softAP result: %s  IP: %s",
          apOk ? "OK" : "FAILED",
          WiFi.softAPIP().toString().c_str());

  ets_delay_us(600000UL);   // 600 ms – AP braucht Zeit bis Beacons laufen
  ESP.wdtFeed();
  CP_HEAP();

  if (!apOk) {
    CP_DBG("ERROR: softAP failed");
    m_logger->println("[CaptivePortal] ERROR: softAP failed");
    ets_delay_us(3000000UL);
    ESP.restart();
    return;
  }

  // ── 4. DNS-Server – leitet alle Domains auf AP-IP ───────────
  // Ermoeglicht Captive-Portal-Popup auf iOS/Android/Windows
  CP_DBG("DNS server start");
  if (!m_dns) m_dns = new DNSServer();
  m_dns->setErrorReplyCode(DNSReplyCode::NoError);
  m_dns->setTTL(300);
  m_dnsActive = m_dns->start(53, "*", apIP);
  CP_DBGF("DNS: %s", m_dnsActive ? "OK" : "FAILED");

  // ── 5. WiFi-Scan (AP+STA Modus: Scan funktioniert) ──────────
  CP_DBG("WiFi scan");
  m_logger->println("[CaptivePortal] Scanning WiFi...");
  WiFi.scanNetworks(true, false);

  uint32_t scanStart = millis();
  while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    ets_delay_us(200000UL);   // 200 ms – kein yield
    ESP.wdtFeed();
    if (millis() - scanStart > 8000UL) { CP_DBG("Scan timeout"); break; }
  }
  CP_DBGF("Scan: %d networks", WiFi.scanComplete());
  m_scanHtml = buildScanHtml();
  WiFi.scanDelete();

  // ── 6. Webserver starten ────────────────────────────────────
  if (!m_server) m_server = new ESP8266WebServer(80);
  registerRoutes();
  m_server->begin();

  CP_DBG("Webserver started – http://192.168.4.1");
  CP_HEAP();
  m_logger->println("[CaptivePortal] Webserver ready – http://192.168.4.1");
}

void CaptivePortal::registerRoutes() {
  // Captive-Portal-Detect-Endpunkte fuer iOS / Android / Windows
  auto redirect = [this]() {
    m_server->sendHeader(F("Location"), F("http://192.168.4.1/"));
    m_server->send(302, "text/plain", "");
  };
  m_server->on(F("/generate_204"),              redirect);
  m_server->on(F("/hotspot-detect.html"),       redirect);
  m_server->on(F("/fwlink"),                    redirect);
  m_server->on(F("/connecttest.txt"),           redirect);
  m_server->on(F("/ncsi.txt"),                  redirect);

  // Hauptseite
  m_server->on(F("/"), HTTP_GET, [this]() {
    CP_DBG("GET /");
    String saved = m_settings->Get("ctSSID", "---");
    if (saved == "---") saved = "";

    String body;
    body.reserve(600);
    body = F("<h2>WLAN auswaehlen</h2>");
    if (saved.length() > 0) {
      body += F("<div class='ok'>&#10003; Gespeichert: <b>");
      body += saved;
      body += F("</b></div>");
    }
    if (m_scanHtml.length() > 0) {
      body += F("<p>Netzwerk antippen:</p>");
      body += m_scanHtml;
      body += F("<hr>");
    }
    body += F("<h2>Zugangsdaten</h2>"
              "<form method='POST' action='/save'>"
              "<label>SSID<input type='text' name='ssid' required value='");
    body += saved;
    body += F("'></label>"
              "<label>Passwort<input type='password' name='pass'></label>"
              "<button type='submit'>Speichern &amp; Verbinden</button>"
              "</form>"
              "<p class='note'>Gateway-IP: 192.168.4.1</p>");
    m_server->send(200, "text/html", buildPage(body));
    CP_HEAP();
  });

  // Formular speichern
  m_server->on(F("/save"), HTTP_POST, [this]() {
    CP_DBG("POST /save");
    String ssid = m_server->arg("ssid");
    String pass = m_server->arg("pass");
    ssid.trim();
    CP_DBGF("SSID='%s' pass-len=%d", ssid.c_str(), (int)pass.length());

    if (ssid.length() == 0) {
      m_server->send(400, "text/html", buildPage(
        F("<div class='err'>&#9888; Bitte SSID eingeben!</div>"
          "<a href='/'>&#8592; Zurueck</a>")));
      return;
    }

    m_settings->Add("ctSSID", ssid);
    m_settings->Add("ctPASS", pass);
    m_settings->Write();
    CP_DBG("Settings saved");

    String body = F("<div class='ok'>&#10003; Gespeichert – starte neu...</div><p>Verbinde mit <b>");
    body += ssid;
    body += F("</b>.</p>");
    m_server->send(200, "text/html", buildPage(body));

    m_restartPending = true;
    m_doneSince      = millis();
    m_done           = true;
    CP_DBG("Done=true, restart pending");
  });

  m_server->onNotFound([this]() {
    m_server->sendHeader(F("Location"), F("http://192.168.4.1/"));
    m_server->send(302, "text/plain", "");
  });
}

void CaptivePortal::Handle() {
  // DNS-Anfragen verarbeiten (Captive-Portal-Popup iOS/Android)
  if (m_dnsActive && m_dns) m_dns->processNextRequest();

  // HTTP-Anfragen synchron verarbeiten
  if (m_server) m_server->handleClient();

  if (m_done) {
    if (m_restartPending && m_doneSince > 0 &&
        (millis() - m_doneSince) > 1500UL) {
      CP_DBG("Restarting...");
      m_logger->println("[CaptivePortal] Restarting...");
      End();
      ESP.restart();
    }
    return;
  }

  if (m_timeoutS > 0 &&
      (millis() - m_startMs) > (uint32_t)m_timeoutS * 1000UL) {
    CP_DBGF("Timeout nach %ds", m_timeoutS);
    m_logger->println("[CaptivePortal] Timeout");
    m_restartPending = false;
    m_doneSince      = millis();
    m_done           = true;
  }
}

bool CaptivePortal::IsDone() {
  return m_done && !m_restartPending;
}

void CaptivePortal::End() {
  CP_DBG("End()");
  if (m_dnsActive && m_dns) { m_dns->stop(); m_dnsActive = false; }
  delete m_dns;    m_dns    = nullptr;
  if (m_server)    { m_server->stop(); }
  delete m_server; m_server = nullptr;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ets_delay_us(100000UL);
  ESP.wdtFeed();
  m_logger->println("[CaptivePortal] Closed");
}

String CaptivePortal::buildScanHtml() {
  int n = WiFi.scanComplete();
  if (n <= 0) return "";
  String html = F("<ul class='nets'>");
  for (int i = 0; i < n; i++) {
    int r = WiFi.RSSI(i);
    int b = (r >= -55) ? 4 : (r >= -65) ? 3 : (r >= -75) ? 2 : 1;
    bool sec = WiFi.encryptionType(i) != ENC_TYPE_NONE;
    html += F("<li onclick=\"document.querySelector('[name=ssid]').value='");
    html += WiFi.SSID(i);
    html += F("'\">");
    html += WiFi.SSID(i);
    if (sec) html += F(" &#x1F512;");
    html += F(" <small>");
    for (int j = 0; j < 4; j++) html += (j < b) ? '|' : '.';
    html += " ("; html += r; html += F(" dBm)</small></li>");
  }
  html += F("</ul>");
  return html;
}

String CaptivePortal::buildPage(const String &body) {
  String h;
  h.reserve(1200);
  h = F("<!DOCTYPE html><html lang='de'><head>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>LaCrosse Gateway Setup</title><style>"
    "body{font-family:Arial,sans-serif;background:#f0f4f8;margin:0;color:#222}"
    "header{background:#0d5c8a;color:#fff;padding:12px 16px}"
    "header h1{margin:0;font-size:1.1rem}"
    "main{max-width:460px;margin:14px auto;background:#fff;border-radius:8px;"
    "box-shadow:0 2px 10px rgba(0,0,0,.1);padding:16px}"
    "h2{margin:0 0 10px;color:#0d5c8a;font-size:.95rem;border-bottom:1px solid #eee;padding-bottom:5px}"
    "label{display:block;margin-top:10px;font-size:.9rem;font-weight:bold}"
    "input{width:100%;padding:8px;margin-top:3px;border:1px solid #ccc;"
    "border-radius:4px;box-sizing:border-box;font-size:.9rem}"
    "button{margin-top:12px;background:#0d5c8a;color:#fff;border:none;"
    "padding:10px;border-radius:4px;width:100%;font-size:.95rem;cursor:pointer}"
    ".nets{list-style:none;padding:0;margin:6px 0}"
    ".nets li{padding:6px 8px;border-radius:4px;cursor:pointer;font-size:.9rem}"
    ".nets li:hover{background:#e8f4fb}"
    ".nets small{color:#888;font-size:.75rem}"
    ".ok{color:#1a6b2e;background:#d6f5dc;padding:8px;border-radius:4px;margin-bottom:8px;font-size:.9rem}"
    ".err{color:#8b1a1a;background:#fde0e0;padding:8px;border-radius:4px;margin-bottom:8px;font-size:.9rem}"
    ".note{font-size:.75rem;color:#aaa;text-align:center;margin-top:10px}"
    "hr{border:none;border-top:1px solid #eee;margin:12px 0}"
    "</style></head><body>"
    "<header><h1>&#x1F4F6; LaCrosse Gateway &#8212; WLAN-Setup</h1></header>"
    "<main>");
  h += body;
  h += F("</main></body></html>");
  return h;
}
