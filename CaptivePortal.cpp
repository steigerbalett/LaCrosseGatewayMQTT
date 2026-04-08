#include "CaptivePortal.h"
#include <ESPAsyncWebServer.h>

// ets_delay_us: Hardware Busy-Wait direkt im SDK.
// Kein yield(), kein OS-Callback – Interrupts bleiben aktiv.
// Verhindert "Panic __yield" bei WiFi-AP-Initialisierung.
extern "C" { void ets_delay_us(uint32_t us); }

// Busy-Wait ohne yield() – sicher während WiFi-AP-Init
static inline void safe_delay_ms(uint32_t ms) {
  while (ms >= 10) { ets_delay_us(10000); ms -= 10; }
  if (ms > 0) ets_delay_us(ms * 1000);
}

// ─────────────────────────────────────────────────────────────────────────────
CaptivePortal::CaptivePortal()
  : m_server(nullptr), m_settings(nullptr), m_logger(nullptr),
    m_done(false), m_timeoutS(0), m_startMs(0) {}

CaptivePortal::~CaptivePortal() {
  if (m_server) { delete m_server; m_server = nullptr; }
}

// ─────────────────────────────────────────────────────────────────────────────
void CaptivePortal::Begin(Settings *settings, Logger *logger,
                          String apSSID, int timeoutS) {
  m_settings = settings;
  m_logger   = logger;
  m_timeoutS = timeoutS;
  m_startMs  = millis();
  m_done     = false;

  m_apSSID = (apSSID.length() == 0)
    ? "LaCrosseGW-" + String(ESP.getChipId(), HEX)
    : apSSID;

  m_logger->println("[CaptivePortal] Starting AP: " + m_apSSID);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true);
  safe_delay_ms(100);

  // FIX: WIFI_AP statt WIFI_AP_STA !
  // WIFI_AP_STA aktiviert den Station-Reconnect-Mechanismus.
  // Dieser feuert im sys-Kontext Callbacks, die yield() aufrufen.
  // yield() aus sys-Kontext → re-entrant __yield() → cont_can_yield() = false
  // → panic(). Reines WIFI_AP hat keine Station-Callbacks.
  WiFi.mode(WIFI_AP);
  safe_delay_ms(50);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(m_apSSID.c_str());

  uint32_t t0 = millis();
  while (millis() - t0 < 500) {
    safe_delay_ms(10);
    ESP.wdtFeed();
  }

  // FIX: WiFi.scanNetworks() VOR m_server->begin() aufrufen und cachen!
  // WiFi.scanNetworks() ist blockierend und ruft intern yield() auf.
  // Aus einem AsyncWebServer-Handler (sys-Kontext) ist das verboten → panic().
  // Lösung: synchroner Scan hier, Ergebnis in m_scanHtml speichern.
  m_logger->println("[CaptivePortal] Scanning WiFi networks...");
  m_scanHtml = scanNetworks();

  if (m_server) { delete m_server; }
  m_server = new AsyncWebServer(80);

  // ── HTTP-Routen ──────────────────────────────────────────────────────────

  m_server->on("/", HTTP_GET, [this](AsyncWebServerRequest *req) {
    String saved_ssid = m_settings->Get("ctSSID", "---");
    if (saved_ssid == "---") saved_ssid = "";

    String body = F("<h2>WLAN-Netzwerk ausw&#228;hlen</h2>");
    if (saved_ssid.length() > 0) {
      body += F("<div class='msg-ok'>&#10003; Gespeichertes Netzwerk: <strong>");
      body += saved_ssid;
      body += F("</strong></div>");
    }
    body += F("<p>Tippe auf ein Netzwerk oder gib die SSID manuell ein.</p>");
    body += m_scanHtml;   // gecacht – KEIN scanNetworks() im async-Handler!
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
  });

  m_server->on("/save", HTTP_POST, [this](AsyncWebServerRequest *req) {
    String ssid = req->hasParam("ssid", true)
                  ? req->getParam("ssid", true)->value() : "";
    String pass = req->hasParam("pass", true)
                  ? req->getParam("pass", true)->value() : "";
    ssid.trim();

    if (ssid.length() == 0) {
      req->send(400, "text/html", buildPage(
        F("<div class='msg-err'>&#9888; Bitte eine SSID eingeben!</div>"
          "<a href='/'>&#8592; Zur&#252;ck</a>")));
      return;
    }

    m_logger->println("[CaptivePortal] Saving credentials for: " + ssid);
    m_settings->Add("ctSSID", ssid);
    m_settings->Add("ctPASS", pass);
    m_settings->Write();

    String body = F("<div class='msg-ok'>&#10003; Gespeichert! Ger&#228;t startet neu...</div><p>Verbinde mit <strong>");
    body += ssid;
    body += F("</strong>.<br>Dieser Hotspot wird jetzt getrennt.</p>");
    req->send(200, "text/html", buildPage(body));
    m_done = true;
  });

  m_server->onNotFound([](AsyncWebServerRequest *req) {
    req->redirect("http://192.168.4.1/");
  });

  m_server->begin();
  m_logger->println("[CaptivePortal] Webserver started on 192.168.4.1");
}

// ─────────────────────────────────────────────────────────────────────────────
void CaptivePortal::Handle() {
  if (m_done) return;
  if (m_timeoutS > 0 &&
      (millis() - m_startMs) > (uint32_t)m_timeoutS * 1000UL) {
    m_logger->println("[CaptivePortal] Timeout");
    m_done = true;
  }
}

bool CaptivePortal::IsDone() { return m_done; }

// ─────────────────────────────────────────────────────────────────────────────
void CaptivePortal::End() {
  if (m_server) { m_server->end(); delete m_server; m_server = nullptr; }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  safe_delay_ms(100);
  m_logger->println("[CaptivePortal] Closed");
}

// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
String CaptivePortal::scanNetworks() {
  int n = WiFi.scanNetworks();
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
    html += bs; html += " "; html += rssi; html += F(" dBm</span></li>");
  }
  html += F("</ul>");
  return html;
}
