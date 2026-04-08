#include "CaptivePortal.h"
#include <ESPAsyncWebServer.h>

// Debug-Makros (immer aktiv – nach Diagnose auskommentierbar)
#define CP_DBG(msg)           do { Serial.print(F("[CP] ")); Serial.println(F(msg)); Serial.flush(); } while(0)
#define CP_DBGF(fmt, ...)     do { Serial.printf("[CP] " fmt "\n", ##__VA_ARGS__); Serial.flush(); } while(0)
#define CP_HEAP()             do { Serial.printf("[CP] Heap=%u ContStack=%u\n", \
                                    ESP.getFreeHeap(), ESP.getFreeContStack()); Serial.flush(); } while(0)

CaptivePortal::CaptivePortal()
  : m_server(nullptr), m_settings(nullptr), m_logger(nullptr),
    m_done(false), m_restartPending(false), m_doneSince(0),
    m_timeoutS(0), m_startMs(0) {}

CaptivePortal::~CaptivePortal() {
  if (m_server) { delete m_server; m_server = nullptr; }
}

void CaptivePortal::Begin(Settings *settings, Logger *logger,
                          String apSSID, int timeoutS) {
  CP_DBG("=== Begin() ===");
  CP_HEAP();

  m_settings       = settings;
  m_logger         = logger;
  m_timeoutS       = timeoutS;
  m_startMs        = millis();
  m_done           = false;
  m_restartPending = false;
  m_doneSince      = 0;

  m_apSSID = (apSSID.length() == 0)
    ? "LaCrosseGW-" + String(ESP.getChipId(), HEX)
    : apSSID;

  m_logger->println("[CaptivePortal] Starting AP: " + m_apSSID);
  CP_DBGF("AP-SSID: %s", m_apSSID.c_str());

  // ----------------------------------------------------------
  //  WiFi reset
  //  FIX: ets_delay_us() statt yield()/delay() – kein panic()
  //       wenn SDK Interrupts deaktiviert
  // ----------------------------------------------------------
  CP_DBG("WiFi persistent(false) + disconnect");
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true);
  // SAFE WAIT – KEIN yield():
  ets_delay_us(100000UL);   // 100 ms
  ESP.wdtFeed();

  CP_DBG("WiFi mode AP");
  WiFi.mode(WIFI_AP);
  ets_delay_us(50000UL);    // 50 ms
  ESP.wdtFeed();

  // ----------------------------------------------------------
  //  SoftAP starten
  // ----------------------------------------------------------
  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, subnet);

  CP_DBG("softAP start");
  bool apOk = WiFi.softAP(m_apSSID.c_str());
  CP_DBGF("softAP result: %s", apOk ? "OK" : "FAILED");

  // SAFE WAIT nach softAP():
  ets_delay_us(500000UL);   // 500 ms – kein yield!
  ESP.wdtFeed();

  CP_HEAP();

  if (!apOk) {
    CP_DBG("ERROR: softAP failed");
    m_logger->println("[CaptivePortal] ERROR: softAP failed");
    ets_delay_us(2000000UL);
    ESP.restart();
    return;
  }

  CP_DBGF("AP-IP: %s", WiFi.softAPIP().toString().c_str());

  // ----------------------------------------------------------
  //  WiFi-Netzwerke scannen (async)
  //  FIX: delay(200)+yield() durch ets_delay_us() ersetzen
  // ----------------------------------------------------------
  CP_DBG("WiFi scan start (async)");
  m_logger->println("[CaptivePortal] Scanning WiFi networks...");
  WiFi.scanNetworks(true, false);  // true = async

  uint32_t scanDeadline = millis() + 10000UL;
  while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    // SAFE WAIT – kein yield():
    ets_delay_us(200000UL); // 200 ms
    ESP.wdtFeed();
    if (millis() > scanDeadline) {
      CP_DBG("Scan timeout");
      break;
    }
  }
  int nNetworks = WiFi.scanComplete();
  CP_DBGF("Scan done: %d networks found", nNetworks);
  m_scanHtml = buildScanHtml();
  WiFi.scanDelete();

  // ----------------------------------------------------------
  //  AsyncWebServer einrichten
  // ----------------------------------------------------------
  if (m_server) { delete m_server; }
  m_server = new AsyncWebServer(80);

  CP_DBG("Registering routes");

  m_server->on("/", HTTP_GET, [this](AsyncWebServerRequest *req) {
    CP_DBG("GET /");
    String saved_ssid = m_settings->Get("ctSSID", "---");
    if (saved_ssid == "---") saved_ssid = "";

    String body = F("<h2>WLAN-Netzwerk auswaehlen</h2>");
    if (saved_ssid.length() > 0) {
      body += F("<div class='msg-ok'>&#10003; Gespeichertes Netzwerk: <strong>");
      body += saved_ssid;
      body += F("</strong></div>");
    }
    body += F("<p>Tippe auf ein Netzwerk oder gib die SSID manuell ein.</p>");
    body += m_scanHtml;
    body += F("<hr><h2>Zugangsdaten eingeben</h2>"
              "<form method='POST' action='/save'>"
              "<label for='ssid'>SSID</label>"
              "<input type='text' id='ssid' name='ssid' placeholder='Netzwerkname' required value='");
    body += saved_ssid;
    body += F("'>"
              "<label for='pass'>Passwort</label>"
              "<input type='password' id='pass' name='pass' placeholder='WLAN-Passwort'>"
              "<input type='submit' value='Speichern &amp; Verbinden'>"
              "</form>");
    req->send(200, "text/html", buildPage(body));
    CP_HEAP();
  });

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
        F("<div class='msg-err'>&#9888; Bitte eine SSID eingeben!</div>"
          "<a href='/'>&#8592; Zurueck</a>")));
      return;
    }

    m_logger->println("[CaptivePortal] Saving credentials: " + ssid);
    m_settings->Add("ctSSID", ssid);
    m_settings->Add("ctPASS", pass);
    m_settings->Write();
    CP_DBG("EEPROM written");

    String body = F("<div class='msg-ok'>&#10003; Gespeichert! Geraet startet neu...</div><p>Verbinde mit <strong>");
    body += ssid;
    body += F("</strong>.<br>Dieser Hotspot wird jetzt getrennt.</p>");
    req->send(200, "text/html", buildPage(body));

    m_restartPending = true;
    m_doneSince      = millis();
    m_done           = true;
    CP_DBG("m_done=true, restart pending");
  });

  m_server->onNotFound([](AsyncWebServerRequest *req) {
    req->redirect("http://192.168.4.1/");
  });

  m_server->begin();
  CP_DBG("Webserver started – open http://192.168.4.1");
  CP_HEAP();
  m_logger->println("[CaptivePortal] Webserver started – open http://192.168.4.1");
}

void CaptivePortal::Handle() {
  if (m_done) {
    if (m_restartPending && m_doneSince > 0 &&
        (millis() - m_doneSince) > 1500UL) {
      CP_DBG("Restarting...");
      m_logger->println("[CaptivePortal] Restarting...");
      ESP.restart();
    }
    return;
  }

  if (m_timeoutS > 0 &&
      (millis() - m_startMs) > (uint32_t)m_timeoutS * 1000UL) {
    CP_DBG("Timeout");
    m_logger->println("[CaptivePortal] Timeout");
    m_restartPending = false;
    m_doneSince      = millis();
    m_done           = true;
  }
}

bool CaptivePortal::IsDone() { return m_done; }

void CaptivePortal::End() {
  CP_DBG("End()");
  if (m_server) { m_server->end(); delete m_server; m_server = nullptr; }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  ets_delay_us(100000UL);
  ESP.wdtFeed();
  m_logger->println("[CaptivePortal] Closed");
}

String CaptivePortal::buildScanHtml() {
  int n = WiFi.scanComplete();
  if (n <= 0) return F("<p>Keine Netzwerke gefunden.</p>");

  String html = F("<ul class='net-list'>");
  for (int i = 0; i < n; i++) {
    int rssi = WiFi.RSSI(i);
    int bars = (rssi >= -60) ? 4 : (rssi >= -70) ? 3 : (rssi >= -80) ? 2 : 1;
    String bs;
    for (int b = 0; b < 4; b++) bs += (b < bars) ? "&#9608;" : "&#9617;";
    bool sec = (WiFi.encryptionType(i) != ENC_TYPE_NONE);

    html += F("<li class='net-item' onclick=\"sel(this,'");
    html += WiFi.SSID(i);
    html += F("')\">");
    html += WiFi.SSID(i);
    if (sec) html += F(" &#x1F512;");
    html += F("<span class='sig'>");
    html += bs;
    html += " ";
    html += rssi;
    html += F(" dBm</span></li>");
  }
  html += F("</ul>");
  return html;
}

String CaptivePortal::buildPage(const String &body) {
  String html = F("<!DOCTYPE html><html lang='de'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>LaCrosse Gateway</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#f4f4f4;margin:0;color:#222}"
    "header{background:#1a6a8a;color:#fff;padding:14px 20px}"
    "header h1{margin:0;font-size:1.2rem}"
    "main{max-width:520px;margin:20px auto;background:#fff;border-radius:8px;"
    "box-shadow:0 2px 10px rgba(0,0,0,.1);padding:20px}"
    "h2{margin-top:0;color:#1a6a8a;font-size:1.05rem}"
    "label{display:block;margin-top:12px;font-weight:bold;font-size:.9rem}"
    "input[type=text],input[type=password]{"
    "width:100%;padding:8px;margin-top:4px;border:1px solid #ccc;"
    "border-radius:4px;font-size:.95rem;box-sizing:border-box}"
    "input[type=submit]{margin-top:16px;background:#1a6a8a;color:#fff;"
    "border:none;padding:10px 20px;border-radius:4px;cursor:pointer;font-size:.95rem}"
    ".net-list{list-style:none;padding:0;margin:10px 0}"
    ".net-item{display:flex;justify-content:space-between;padding:7px 10px;"
    "border-radius:4px;cursor:pointer}"
    ".net-item:hover{background:#e8f4fb}"
    ".net-item.sel{background:#d0eaf5}"
    ".sig{font-size:.75rem;color:#888}"
    ".msg-ok{color:#1e7a30;background:#d6f5dc;padding:9px 12px;"
    "border-radius:4px;margin-bottom:12px}"
    ".msg-err{color:#9a2020;background:#fde0e0;padding:9px 12px;"
    "border-radius:4px;margin-bottom:12px}"
    "hr{border:none;border-top:1px solid #eee;margin:16px 0}"
    "</style></head><body>"
    "<header><h1>&#x1F4F6; LaCrosse Gateway &#8211; WLAN-Einrichtung</h1></header>"
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
