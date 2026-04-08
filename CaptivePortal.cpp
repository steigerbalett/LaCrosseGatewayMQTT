#include "CaptivePortal.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// ============================================================
//  CaptivePortal.cpp  –  v3  (AP+STA, DNS, debug)
//
//  FIXES vs v2:
//   - WiFi.mode(WIFI_AP_STA) statt WIFI_AP:
//       * AP sichtbar für alle Geräte
//       * WiFi-Scan funktioniert (braucht STA-Kontext)
//   - DNSServer: leitet alle DNS-Anfragen auf 192.168.4.1
//       * Captive-Portal-Popup erscheint automatisch auf iOS/Android
//   - WiFi-Scan erst NACH AP-Start mit ausreichend Wartezeit
//   - ets_delay_us() bleibt in der Init-Phase (kein yield-panic)
// ============================================================

#define CP_DBG(msg)       do { Serial.print(F("[CP] ")); Serial.println(F(msg)); Serial.flush(); } while(0)
#define CP_DBGF(fmt, ...) do { Serial.printf("[CP] " fmt "\n", ##__VA_ARGS__); Serial.flush(); } while(0)
#define CP_HEAP()         do { Serial.printf("[CP] Heap=%u ContStack=%u\n", \
                                ESP.getFreeHeap(), ESP.getFreeContStack()); Serial.flush(); } while(0)

static DNSServer* s_dnsServer = nullptr;   // Heap-allokiert in Begin(), kein globaler Konstruktor
static bool       s_dnsActive = false;

CaptivePortal::CaptivePortal()
  : m_server(nullptr), m_settings(nullptr), m_logger(nullptr),
    m_done(false), m_restartPending(false), m_doneSince(0),
    m_timeoutS(0), m_startMs(0) {}

CaptivePortal::~CaptivePortal() {
  if (m_server) { delete m_server; m_server = nullptr; }
}

void CaptivePortal::Begin(Settings *settings, Logger *logger,
                          String apSSID, int timeoutS) {
  CP_DBG("=== Begin() v3 ===");
  CP_HEAP();

  m_settings       = settings;
  m_logger         = logger;
  m_timeoutS       = timeoutS;
  m_startMs        = millis();
  m_done           = false;
  m_restartPending = false;
  m_doneSince      = 0;
  s_dnsActive      = false;

  m_apSSID = (apSSID.length() == 0)
    ? "LaCrosseGW-" + String(ESP.getChipId(), HEX)
    : apSSID;

  m_logger->println("[CaptivePortal] Starting AP: " + m_apSSID);
  CP_DBGF("AP-SSID: %s", m_apSSID.c_str());

  // ── 1. WiFi komplett zurücksetzen ──────────────────────────
  CP_DBG("WiFi reset");
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true);
  // FIX: ets_delay_us statt delay/yield → kein panic bei deaktiv. Interrupts
  ets_delay_us(200000UL);   // 200 ms
  ESP.wdtFeed();

  // ── 2. AP+STA Modus – WICHTIG: AP sichtbar + Scan möglich ──
  CP_DBG("WiFi mode WIFI_AP_STA");
  WiFi.mode(WIFI_AP_STA);    // <-- FIX: nicht WIFI_AP allein!
  ets_delay_us(100000UL);    // 100 ms
  ESP.wdtFeed();

  // ── 3. SoftAP konfigurieren und starten ────────────────────
  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, subnet);

  CP_DBGF("softAP start (SSID=%s)", m_apSSID.c_str());
  bool apOk = WiFi.softAP(m_apSSID.c_str());
  CP_DBGF("softAP result: %s", apOk ? "OK" : "FAILED");

  // Längere Wartezeit: AP braucht ~500ms bis Beacons laufen
  ets_delay_us(600000UL);    // 600 ms
  ESP.wdtFeed();

  CP_HEAP();
  CP_DBGF("AP-IP: %s", WiFi.softAPIP().toString().c_str());

  if (!apOk) {
    CP_DBG("ERROR: softAP failed – restart in 3s");
    m_logger->println("[CaptivePortal] ERROR: softAP failed");
    ets_delay_us(3000000UL);
    ESP.restart();
    return;
  }

  // ── 4. DNS-Server: alle Domains → AP-IP ────────────────────
  //    Ermöglicht Captive-Portal-Popup auf iOS/Android/Windows
  CP_DBG("Starting DNS server (port 53)");
  if (!s_dnsServer) s_dnsServer = new DNSServer();  // Heap-Alloc, kein globaler Konstruktor
  s_dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  s_dnsServer->setTTL(300);
  bool dnsOk = s_dnsServer->start(53, "*", apIP);
  s_dnsActive = dnsOk;
  CP_DBGF("DNS server: %s", dnsOk ? "OK" : "FAILED");

  // ── 5. WiFi-Netzwerke scannen (für SSID-Auswahl im Portal) ─
  CP_DBG("WiFi scan start");
  m_logger->println("[CaptivePortal] Scanning WiFi networks...");
  WiFi.scanNetworks(true, false);   // async=true, showHidden=false

  // Im AP+STA Modus sollte Scan funktionieren.
  // FIX: ets_delay_us statt yield()/delay()
  uint32_t scanStart = millis();
  while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    ets_delay_us(200000UL);   // 200 ms
    ESP.wdtFeed();
    if (millis() - scanStart > 8000UL) {
      CP_DBG("Scan timeout after 8s");
      break;
    }
  }
  int nNetworks = WiFi.scanComplete();
  CP_DBGF("Scan result: %d networks", nNetworks);
  m_scanHtml = buildScanHtml();
  WiFi.scanDelete();

  // ── 6. AsyncWebServer einrichten ───────────────────────────
  if (m_server) { delete m_server; }
  m_server = new AsyncWebServer(80);

  CP_DBG("Registering HTTP routes");

  // Captive-Portal-Detect-Endpunkte (iOS, Android, Windows)
  auto captiveRedirect = [](AsyncWebServerRequest *req) {
    req->redirect("http://192.168.4.1/");
  };
  m_server->on("/generate_204",         HTTP_GET, captiveRedirect);
  m_server->on("/hotspot-detect.html",  HTTP_GET, captiveRedirect);
  m_server->on("/fwlink",               HTTP_GET, captiveRedirect);
  m_server->on("/connecttest.txt",      HTTP_GET, captiveRedirect);
  m_server->on("/ncsi.txt",             HTTP_GET, captiveRedirect);
  m_server->on("/check_network_status.txt", HTTP_GET, captiveRedirect);

  // Haupt-Konfigurationsseite
  m_server->on("/", HTTP_GET, [this](AsyncWebServerRequest *req) {
    CP_DBG("GET /");
    String saved_ssid = m_settings->Get("ctSSID", "---");
    if (saved_ssid == "---") saved_ssid = "";

    String body;
    body.reserve(800);
    body = F("<h2>WLAN-Netzwerk auswaehlen</h2>");
    if (saved_ssid.length() > 0) {
      body += F("<div class='msg-ok'>&#10003; Gespeichertes Netz: <strong>");
      body += saved_ssid;
      body += F("</strong></div>");
    }
    if (m_scanHtml.length() > 0) {
      body += F("<p>Tippe auf ein Netzwerk zum Auswaehlen:</p>");
      body += m_scanHtml;
      body += F("<hr>");
    }
    body += F("<h2>Zugangsdaten</h2>"
              "<form method='POST' action='/save'>"
              "<label for='ssid'>SSID</label>"
              "<input type='text' id='ssid' name='ssid' "
              "placeholder='Netzwerkname' required value='");
    body += saved_ssid;
    body += F("'>"
              "<label for='pass'>Passwort</label>"
              "<input type='password' id='pass' name='pass' "
              "placeholder='WLAN-Passwort'>"
              "<input type='submit' value='Speichern &amp; Verbinden'>"
              "</form>"
              "<p style='font-size:.8em;color:#888;margin-top:16px'>"
              "Gateway-IP: 192.168.4.1</p>");
    req->send(200, "text/html", buildPage(body));
    CP_HEAP();
  });

  // Formular speichern
  m_server->on("/save", HTTP_POST, [this](AsyncWebServerRequest *req) {
    CP_DBG("POST /save");
    String ssid = req->hasParam("ssid", true)
                  ? req->getParam("ssid", true)->value() : "";
    String pass = req->hasParam("pass", true)
                  ? req->getParam("pass", true)->value() : "";
    ssid.trim();
    CP_DBGF("SSID='%s' pass-len=%d", ssid.c_str(), (int)pass.length());

    if (ssid.length() == 0) {
      req->send(400, "text/html", buildPage(
        F("<div class='msg-err'>&#9888; Bitte SSID eingeben!</div>"
          "<a href='/'>&#8592; Zurueck</a>")));
      return;
    }

    m_logger->println("[CaptivePortal] Saving: " + ssid);
    m_settings->Add("ctSSID", ssid);
    m_settings->Add("ctPASS", pass);
    m_settings->Write();
    CP_DBG("Settings saved to EEPROM");

    String body = F("<div class='msg-ok'>&#10003; Gespeichert!</div>"
                    "<p>Verbinde mit <strong>");
    body += ssid;
    body += F("</strong>...<br>Hotspot wird getrennt.</p>");
    req->send(200, "text/html", buildPage(body));

    m_restartPending = true;
    m_doneSince      = millis();
    m_done           = true;
    CP_DBG("Done=true, restart pending");
  });

  // Alles andere → Redirect auf Startseite
  m_server->onNotFound([](AsyncWebServerRequest *req) {
    req->redirect("http://192.168.4.1/");
  });

  m_server->begin();
  CP_DBG("Webserver started");
  CP_HEAP();
  m_logger->println("[CaptivePortal] Webserver ready – http://192.168.4.1");
}

void CaptivePortal::Handle() {
  // DNS-Anfragen verarbeiten (für Captive-Portal-Popup)
  if (s_dnsActive) {
    s_dnsServer->processNextRequest();
  }

  if (m_done) {
    if (m_restartPending && m_doneSince > 0 &&
        (millis() - m_doneSince) > 1500UL) {
      CP_DBG("Restarting...");
      m_logger->println("[CaptivePortal] Restarting...");
      if (s_dnsActive && s_dnsServer) { s_dnsServer->stop(); delete s_dnsServer; s_dnsServer = nullptr; s_dnsActive = false; }
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

bool CaptivePortal::IsDone() { return m_done; }

void CaptivePortal::End() {
  CP_DBG("End()");
  if (s_dnsActive && s_dnsServer) { s_dnsServer->stop(); s_dnsActive = false; }
  delete s_dnsServer; s_dnsServer = nullptr;
  if (m_server)    { m_server->end(); delete m_server; m_server = nullptr; }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ets_delay_us(100000UL);
  ESP.wdtFeed();
  m_logger->println("[CaptivePortal] Closed");
}

String CaptivePortal::buildScanHtml() {
  int n = WiFi.scanComplete();
  if (n <= 0) {
    CP_DBGF("buildScanHtml: n=%d (no networks)", n);
    return "";   // Leerer String → kein Scan-Block in der UI
  }
  String html = F("<ul class='net-list'>");
  for (int i = 0; i < n; i++) {
    int rssi = WiFi.RSSI(i);
    int bars = (rssi >= -55) ? 4 : (rssi >= -65) ? 3 : (rssi >= -75) ? 2 : 1;
    String bs;
    for (int b = 0; b < 4; b++) bs += (b < bars) ? "&#9608;" : "&#9617;";
    bool sec = (WiFi.encryptionType(i) != ENC_TYPE_NONE);

    html += F("<li class='net-item' onclick=\"sel(this,'");
    html += WiFi.SSID(i);
    html += F("')\">");
    html += WiFi.SSID(i);
    if (sec) html += F(" &#x1F512;");
    html += F(" <span class='sig'>");
    html += bs;
    html += " (";
    html += rssi;
    html += F(" dBm)</span></li>");
  }
  html += F("</ul>");
  return html;
}

String CaptivePortal::buildPage(const String &body) {
  String html;
  html.reserve(1400);
  html = F("<!DOCTYPE html><html lang='de'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>LaCrosse Gateway Setup</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#f0f4f8;margin:0;color:#1a1a1a}"
    "header{background:#0d5c8a;color:#fff;padding:14px 20px}"
    "header h1{margin:0;font-size:1.15rem}"
    "main{max-width:480px;margin:16px auto;background:#fff;border-radius:10px;"
    "box-shadow:0 2px 12px rgba(0,0,0,.12);padding:20px}"
    "h2{margin-top:0;color:#0d5c8a;font-size:1rem;border-bottom:1px solid #e0e0e0;padding-bottom:6px}"
    "label{display:block;margin-top:10px;font-weight:bold;font-size:.9rem}"
    "input[type=text],input[type=password]{"
    "width:100%;padding:9px 10px;margin-top:4px;border:1px solid #ccc;"
    "border-radius:5px;font-size:.95rem;box-sizing:border-box}"
    "input[type=submit]{margin-top:14px;background:#0d5c8a;color:#fff;"
    "border:none;padding:11px;border-radius:5px;cursor:pointer;"
    "font-size:.95rem;width:100%;font-weight:bold}"
    "input[type=submit]:active{background:#094870}"
    ".net-list{list-style:none;padding:0;margin:8px 0}"
    ".net-item{padding:8px 10px;border-radius:5px;cursor:pointer;"
    "display:flex;justify-content:space-between;align-items:center;"
    "border:1px solid transparent}"
    ".net-item:hover{background:#e8f4fb;border-color:#b3d8ee}"
    ".net-item.sel{background:#d0eaf5;border-color:#0d5c8a}"
    ".sig{font-size:.75rem;color:#888;white-space:nowrap}"
    ".msg-ok{color:#1a6b2e;background:#d6f5dc;padding:9px 12px;"
    "border-radius:5px;margin-bottom:10px;font-size:.9rem}"
    ".msg-err{color:#8b1a1a;background:#fde0e0;padding:9px 12px;"
    "border-radius:5px;margin-bottom:10px;font-size:.9rem}"
    "hr{border:none;border-top:1px solid #eee;margin:14px 0}"
    "</style></head><body>"
    "<header><h1>&#x1F4F6; LaCrosse Gateway &#8212; WLAN-Setup</h1></header>"
    "<main>");
  html += body;
  html += F("</main>"
    "<script>"
    "function sel(el,s){"
    "document.querySelectorAll('.net-item').forEach(e=>e.classList.remove('sel'));"
    "el.classList.add('sel');"
    "document.getElementById('ssid').value=s;}"
    "</script></body></html>");
  return html;
}