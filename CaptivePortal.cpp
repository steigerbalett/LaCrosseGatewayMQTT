#include "CaptivePortal.h"

CaptivePortal::CaptivePortal() : m_server(80), m_done(false), m_timeoutS(0),
                                  m_startMs(0) {
}

// -----------------------------------------------------------------------
void CaptivePortal::Begin(Settings *settings, Logger *logger,
                           String apSSID, int timeoutS) {
  m_settings  = settings;
  m_logger    = logger;
  m_timeoutS  = timeoutS;
  m_startMs   = millis();
  m_done      = false;

  if (apSSID.length() == 0) {
    m_apSSID = "LaCrosseGW-" + String(ESP.getChipId(), HEX);
  } else {
    m_apSSID = apSSID;
  }

  m_logger->println("[CaptivePortal] Starting AP: " + m_apSSID);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true);   // Bestehende STA-Verbindung komplett trennen
  delay(100);
  WiFi.mode(WIFI_AP_STA);  // WIFI_AP_STA statt WIFI_AP – wie WiFiManager!

  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(m_apSSID.c_str());
  delay(500);

  // Hinweis: DNS Catch-All deaktiviert (verursacht sys-Stack-Crash auf ESP8266).
  // Browser-Captive-Portal-Erkennung funktioniert nicht, aber Nutzer koennen
  // manuell http://192.168.4.1 aufrufen.

  // HTTP-Routen
  m_server.on("/",         [this]() { handleRoot(); });
  m_server.on("/scan",     [this]() { handleScan(); });
  m_server.on("/save",     HTTP_POST, [this]() { handleSave(); });
  m_server.onNotFound(     [this]() { handleNotFound(); });

  m_server.begin();
  m_logger->println("[CaptivePortal] Webserver started on 192.168.4.1");
}

// -----------------------------------------------------------------------
void CaptivePortal::Handle() {
  if (m_done) return;
  m_server.handleClient();

  if (m_timeoutS > 0 && (millis() - m_startMs) > (uint32_t)m_timeoutS * 1000UL) {
    m_logger->println("[CaptivePortal] Timeout – closing portal");
    m_done = true;
  }
}

// -----------------------------------------------------------------------
bool CaptivePortal::IsDone() {
  return m_done;
}

// -----------------------------------------------------------------------
void CaptivePortal::End() {
  m_server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  delay(100);
  m_logger->println("[CaptivePortal] Closed");
}

// -----------------------------------------------------------------------
// HTML-Hilfsfunktion: vollständige Seite
// -----------------------------------------------------------------------
String CaptivePortal::buildPage(const String &body) {
  String html = F("<!DOCTYPE html><html lang='de'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>LaCrosse Gateway – WLAN-Einrichtung</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#f4f4f4;margin:0;padding:0;color:#222}"
    "header{background:#1a6a8a;color:#fff;padding:16px 24px}"
    "header h1{margin:0;font-size:1.3rem}"
    "main{max-width:520px;margin:24px auto;background:#fff;border-radius:8px;"
    "box-shadow:0 2px 12px rgba(0,0,0,.1);padding:24px}"
    "h2{margin-top:0;color:#1a6a8a;font-size:1.1rem}"
    "label{display:block;margin-top:14px;font-weight:bold;font-size:.9rem}"
    "input[type=text],input[type=password],select{"
    "width:100%;padding:9px 10px;margin-top:5px;border:1px solid #ccc;"
    "border-radius:5px;font-size:.95rem;box-sizing:border-box}"
    "button,input[type=submit]{"
    "margin-top:18px;background:#1a6a8a;color:#fff;border:none;"
    "padding:10px 22px;border-radius:5px;cursor:pointer;font-size:.95rem}"
    "button:hover,input[type=submit]:hover{background:#155472}"
    ".net-list{list-style:none;padding:0;margin:12px 0}"
    ".net-item{display:flex;align-items:center;justify-content:space-between;"
    "padding:8px 10px;border-radius:5px;cursor:pointer;transition:background .15s}"
    ".net-item:hover{background:#e8f4fb}"
    ".net-item.selected{background:#d0eaf5;border:1px solid #1a6a8a}"
    ".signal{font-size:.78rem;color:#888}"
    ".lock-icon{font-size:.85rem;color:#e07b00}"
    ".msg-ok{color:#217a30;background:#d6f5dc;padding:10px 14px;border-radius:5px;margin-bottom:14px}"
    ".msg-err{color:#9a2020;background:#fde0e0;padding:10px 14px;border-radius:5px;margin-bottom:14px}"
    ".scan-btn{background:#eee;color:#333;font-size:.85rem;padding:7px 14px;margin-top:0}"
    ".scan-btn:hover{background:#ddd}"
    "hr{border:none;border-top:1px solid #eee;margin:20px 0}"
    "</style></head><body>"
    "<header><h1>&#x1F4F6; LaCrosse Gateway – WLAN-Einrichtung</h1></header>"
    "<main>");
  html += body;
  html += F("</main><script>"
    "function selectNet(el,ssid){"
    "document.querySelectorAll('.net-item').forEach(e=>e.classList.remove('selected'));"
    "el.classList.add('selected');"
    "document.getElementById('ssid').value=ssid;}"
    "</script></body></html>");
  return html;
}

// -----------------------------------------------------------------------
// WiFi-Scan als HTML-Liste
// -----------------------------------------------------------------------
String CaptivePortal::scanNetworks() {
  int n = WiFi.scanNetworks();
  if (n == 0) return F("<p>Keine Netzwerke gefunden.</p>");

  String html = F("<ul class='net-list'>");
  for (int i = 0; i < n; i++) {
    int rssi = WiFi.RSSI(i);
    // Signalbalken (1-4 Blöcke)
    int bars = 1;
    if (rssi >= -60) bars = 4;
    else if (rssi >= -70) bars = 3;
    else if (rssi >= -80) bars = 2;
    String barStr = "";
    for (int b = 0; b < 4; b++) barStr += (b < bars) ? "&#9608;" : "&#9617;";

    bool secured = (WiFi.encryptionType(i) != ENC_TYPE_NONE);
    html += F("<li class='net-item' onclick=\"selectNet(this,'");
    html += WiFi.SSID(i);
    html += F("')\">" );
    html += F("<span>");
    html += WiFi.SSID(i);
    if (secured) html += F(" <span class='lock-icon'>&#x1F512;</span>");
    html += F("</span><span class='signal'>");
    html += barStr;
    html += " ";
    html += rssi;
    html += F(" dBm</span></li>");
  }
  html += F("</ul>");
  return html;
}

// -----------------------------------------------------------------------
void CaptivePortal::handleRoot() {
  String saved_ssid = m_settings->Get("ctSSID", "---");
  if (saved_ssid == "---") saved_ssid = "";

  String body = F("<h2>WLAN-Netzwerk auswählen</h2>");

  if (saved_ssid.length() > 0) {
    body += F("<div class='msg-ok'>&#10003; Gespeichertes Netzwerk: <strong>");
    body += saved_ssid;
    body += F("</strong></div>");
  }

  body += F("<p>Tippe auf ein Netzwerk oder gib die SSID manuell ein.</p>"
            "<button class='scan-btn' onclick=\"location.reload()\">&#x21BA; Netzwerke neu scannen</button>");

  body += scanNetworks();

  body += F("<hr><h2>Zugangsdaten eingeben</h2>"
            "<form method='POST' action='/save'>"
            "<label for='ssid'>SSID (Netzwerkname)</label>"
            "<input type='text' id='ssid' name='ssid' placeholder='Netzwerkname' required value='");
  body += saved_ssid;
  body += F("'>"
            "<label for='pass'>Passwort</label>"
            "<input type='password' id='pass' name='pass' placeholder='WLAN-Passwort'>"
            "<input type='submit' value='Speichern &amp; Verbinden'>"
            "</form>");

  m_server.send(200, "text/html", buildPage(body));
}

// -----------------------------------------------------------------------
void CaptivePortal::handleScan() {
  // AJAX-Endpunkt für Nur-Scan (liefert HTML-Fragment)
  m_server.send(200, "text/html", scanNetworks());
}

// -----------------------------------------------------------------------
void CaptivePortal::handleSave() {
  String ssid = m_server.arg("ssid");
  String pass = m_server.arg("pass");

  ssid.trim();

  if (ssid.length() == 0) {
    String body = F("<div class='msg-err'>&#9888; Bitte eine SSID eingeben!</div>"
                    "<a href='/'>&#8592; Zurück</a>");
    m_server.send(400, "text/html", buildPage(body));
    return;
  }

  m_logger->println("[CaptivePortal] Saving credentials for SSID: " + ssid);

  m_settings->Add("ctSSID", ssid);
  m_settings->Add("ctPASS", pass);
  m_settings->Write();

  // Kein Verbindungstest (wuerde WiFi.begin() im AP-Kontext ausloesen →
  // sys-Stack-Crash). Zugangsdaten sind gespeichert, Neustart verbindet.
  String body = F("<div class='msg-ok'>&#10003; Zugangsdaten gespeichert!<br>"
                  "SSID: <strong>");
  body += ssid;
  body += F("</strong><br>Das Gateway startet jetzt neu und verbindet sich...</div>");
  m_server.send(200, "text/html", buildPage(body));
  delay(1500);
  ESP.restart();  // Neustart mit gespeicherten Credentials
}

// -----------------------------------------------------------------------
void CaptivePortal::handleNotFound() {
  // Kein DNS Catch-All aktiv. Weiterleitung zur Startseite.
  m_server.sendHeader("Location", "http://192.168.4.1/", true);
  m_server.send(302, "text/plain", "");
}
