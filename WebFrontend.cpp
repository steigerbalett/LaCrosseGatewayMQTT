#include "WebFrontend.h"
#include <EEPROM.h>
#include "Settings.h"
#include "HTML.h"
#include "OTAUpdate.h"
#include "Help.h"
#include "ESP8266WiFiType.h"
#include "ESPTools.h"

// ═══════════════════════════════════════════════════
// HILFSFUNKTION
// ═══════════════════════════════════════════════════
String WifiModeToString(WiFiMode_t mode) {
  switch (mode) {
  case WiFiMode_t::WIFI_AP:    return "Accespoint";
  case WiFiMode_t::WIFI_AP_STA: return "Accespoint + Station";
  case WiFiMode_t::WIFI_OFF:   return "Off";
  case WiFiMode_t::WIFI_STA:   return "Station";
  default:                     return "";
  }
}

// ═══════════════════════════════════════════════════
// CSS
// ═══════════════════════════════════════════════════
const char LGWMQTT_CSS[] PROGMEM =
  ":root{"
    "--pri:#03a9f4;--acc:#ff9800;"
    "--bg:#111;--bg2:#1c1c1c;--card:#1c1c1c;"
    "--txt:#e1e1e1;--txt2:#9b9b9b;--dis:#6f6f6f;"
    "--div:#2f2f2f;"
    "--ok:#4caf50;--warn:#ff9800;--err:#f44336;--info:#2196f3;"
  "}"
  "[data-theme='light']{"
    "--pri:#1976d2;--acc:#f57c00;"
    "--bg:#fafafa;--bg2:#fff;--card:#fff;"
    "--txt:#212121;--txt2:#757575;--dis:#9e9e9e;"
    "--div:#e0e0e0;"
    "--ok:#2e7d32;--warn:#f57c00;--err:#c62828;--info:#1976d2;"
  "}"
  "*{box-sizing:border-box}"
  "body{font-family:'Roboto',Arial,sans-serif;margin:0;padding:12px;"
    "background:var(--bg);color:var(--txt);line-height:1.4;"
    "transition:background .3s,color .3s}"
  ".hdr{display:flex;justify-content:space-between;align-items:center;"
    "margin-bottom:16px;padding-bottom:8px;border-bottom:1px solid var(--div)}"
  "h1{color:var(--txt);font-size:28px;font-weight:400;margin:0}"
  "h2{color:var(--txt);font-size:18px;font-weight:500;margin:16px 0 12px}"
  "h3{color:var(--txt);font-size:15px;font-weight:500;margin:16px 0 8px}"
  ".theme-btn{background:var(--card);border:1px solid var(--div);border-radius:24px;"
    "padding:6px 12px;cursor:pointer;display:flex;align-items:center;gap:6px;"
    "font-size:13px;color:var(--txt);transition:all .3s}"
  ".theme-btn:hover{border-color:var(--pri)}"
  ".card-grid{display:grid;grid-template-columns:1fr;gap:12px;margin:12px 0}"
  ".card{background:var(--card);border-radius:8px;padding:12px;margin:0;"
    "box-shadow:0 2px 4px rgba(0,0,0,.1);transition:background .3s}"
  ".card-full{grid-column:1/-1}"
  "nav a{display:inline-block;padding:8px 14px;text-decoration:none;"
    "color:var(--txt2);font-size:13px;border-radius:4px;margin:2px}"
  "nav a:hover{background:rgba(3,169,244,.1);color:var(--pri)}"
  "table{border-collapse:collapse;width:100%;background:var(--card);"
    "border-radius:8px;overflow:hidden;box-shadow:0 2px 4px rgba(0,0,0,.1);font-size:14px}"
  "thead{background:var(--bg2)}"
  "th{padding:10px 8px;text-align:left;font-weight:500;color:var(--txt);"
    "text-transform:uppercase;font-size:11px;letter-spacing:.5px;border-bottom:1px solid var(--div)}"
  "td{padding:9px 8px;border-bottom:1px solid var(--div);color:var(--txt);font-size:14px}"
  "tbody tr:hover{background:rgba(3,169,244,.08)}"
  "tbody tr:last-child td{border-bottom:none}"
  "label{display:block;margin:10px 0 5px;color:var(--txt);font-weight:500;font-size:13px}"
  "input:not([type=checkbox]):not([type=radio]):not([type=submit]),"
  "select,textarea{"
    "width:100%;padding:9px;margin:3px 0 10px;border:1px solid var(--div);"
    "border-radius:4px;background:var(--bg2)!important;color:var(--txt)!important;"
    "font-size:13px;font-family:inherit;-webkit-appearance:none;appearance:none;"
    "transition:all .3s}"
  "input[type=submit],button{"
    "background:var(--pri);color:#fff;padding:9px 18px;margin:4px 4px 0 0;"
    "border:none;border-radius:4px;cursor:pointer;font-size:13px;"
    "font-weight:500;text-transform:uppercase;letter-spacing:.5px;transition:background .2s}"
  "input[type=submit]:hover,button:hover{background:#0288d1}"
  ".badge{display:inline-block;padding:3px 10px;border-radius:12px;"
    "font-size:11px;font-weight:500;text-transform:uppercase}"
  ".ok{background:rgba(76,175,80,.15);color:var(--ok)}"
  ".err{background:rgba(244,67,54,.15);color:var(--err)}"
  ".warn{background:rgba(255,152,0,.15);color:var(--warn)}"
  ".batt-weak{color:var(--err);font-weight:500}"
  ".batt-ok{color:var(--ok);font-weight:500}"
  ".logbox{height:260px;border:1px solid var(--div);border-radius:4px;"
    "overflow-y:scroll;padding:6px;background:var(--bg2);font-family:monospace;font-size:12px}"
  ".logLine{padding:1px 0;color:var(--txt2)}"
  ".dataLine{color:var(--ok)}"
  "a{color:var(--pri);text-decoration:none}"
  "a:hover{color:var(--acc);text-decoration:underline}"
  "p{color:var(--txt);margin:6px 0}"
  ".info{color:var(--txt2);font-size:12px;margin:4px 0}"
  ".footer{margin-top:24px;padding-top:12px;border-top:1px solid var(--div);"
    "color:var(--txt2);font-size:12px;text-align:center}"
  "@media(min-width:768px){.card-grid{grid-template-columns:repeat(2,1fr)}}"
  "@media(min-width:1200px){.card-grid{grid-template-columns:repeat(3,1fr)}}";

// ═══════════════════════════════════════════════════
// Theme-Toggle
// ═══════════════════════════════════════════════════
const char LGWMQTT_JS_THEME[] PROGMEM =
  "<script>"
  "function toggleTheme(){"
    "var b=document.body,i=document.getElementById('ti'),t=document.getElementById('tt');"
    "if(b.getAttribute('data-theme')==='light'){"
      "b.setAttribute('data-theme','dark');"
      "i.textContent='\\uD83C\\uDF19';t.textContent='Dark Mode';"
      "localStorage.setItem('theme','dark');"
    "}else{"
      "b.setAttribute('data-theme','light');"
      "i.textContent='\\u2600\\uFE0F';t.textContent='Light Mode';"
      "localStorage.setItem('theme','light');"
    "}"
  "}"
  "window.addEventListener('DOMContentLoaded',function(){"
    "var s=localStorage.getItem('theme')||'dark';"
    "var i=document.getElementById('ti'),t=document.getElementById('tt');"
    "document.body.setAttribute('data-theme',s);"
    "if(s==='light'){i.textContent='\\u2600\\uFE0F';t.textContent='Light Mode';}"
    "else{i.textContent='\\uD83C\\uDF19';t.textContent='Dark Mode';}"
  "});"
  "</script>";

// ═══════════════════════════════════════════════════
// Log-Seite
// ═══════════════════════════════════════════════════
const char on_log[] PROGMEM =
"<script>"
"  function sendCommand() {"
"    var cmd = document.getElementById('commandText').value;"
"    var request = new XMLHttpRequest();"
"    request.open('GET', 'command?cmd=' + encodeURIComponent(cmd), true);"
"    request.send();"
"  };"
"  function clearList(what) {"
"    document.getElementById(what + 'Div').innerHTML = '';"
"    filter(what);"
"  };"
"  function filter(what) {"
"    var el = document.getElementById(what + 'DivFilter');"
"    var text0 = el.value.toLowerCase();"
"    var elements = document.getElementsByClassName(what + 'Line');"
"    var ct = 0;"
"    for (var i = 0; i < elements.length; i++) {"
"      if (elements[i].innerHTML.toLowerCase().indexOf(text0) == -1) {"
"        elements[i].style.display = 'none';"
"      } else { elements[i].style.display = 'block'; ct++; }"
"    }"
"    document.getElementById(what + 'RowCount').innerHTML = ct + ' rows';"
"  };"
"  function run() {"
"    document.getElementById('logDivFilter').onkeyup = function() { filter('log'); };"
"    document.getElementById('dataDivFilter').onkeyup = function() { filter('data'); };"
"    getLogData();"
"  };"
"  function getLogData() {"
"    if (document.getElementById('enabled').checked == true) {"
"      var request = new XMLHttpRequest();"
"      request.onreadystatechange = function () {"
"        if (this.readyState == 4 && this.status == 200 && this.responseText != null && this.responseText != '') {"
"          var lines = this.responseText.split('\\n');"
"          for (var i = 0; i < lines.length; i++) {"
"            var txt = lines[i];"
"            if (txt != '') {"
"              if (txt == 'SYS: ***CLEARLOG***') {"
"                clearList('data'); clearList('log');"
"              } else {"
"                var targetDiv = 'logDiv', scrollCheckBox = 'scrollLogDiv', prefix = 'log';"
"                if (txt.startsWith('DATA:')) {"
"                  prefix = 'data'; targetDiv = 'dataDiv'; scrollCheckBox = 'scrollDataDiv';"
"                  txt = txt.substring(5);"
"                }"
"                if (txt.startsWith('SYS:')) { txt = txt.substring(4); }"
"                txt = new Date().toLocaleTimeString('de-DE') + ': ' + txt;"
"                document.getElementById(targetDiv).innerHTML += \"<div class='\" + prefix + \"Line'>\" + txt + '</div>';"
"                filter(prefix);"
"                if (document.getElementById(scrollCheckBox).checked == true) {"
"                  var objDiv = document.getElementById(targetDiv);"
"                  objDiv.scrollTop = objDiv.scrollHeight;"
"                }"
"              }"
"            }"
"          }"
"        }"
"      };"
"      request.open('GET', 'getLogData?nc=' + Math.random(), true);"
"      request.send();"
"    }"
"    setTimeout('getLogData()', 500);"
"  };"
"</script>"
"<body onload='run()'>"
// Log-Seite in Cards
"<div class='card' style='margin-bottom:12px'>"
"  <h3>&#128196; Befehlseingabe</h3>"
"  Command: <input id='commandText' size='80' onkeydown=\"if(event.keyCode==13)sendCommand()\">"
"  <button type='button' onclick=\"sendCommand();\">Senden</button>"
"  &nbsp;&nbsp;<input type='checkbox' id='enabled' value='true' checked> Logging aktiv"
"</div>"
"<div class='card' style='margin-bottom:12px'>"
"  <h3>&#128225; LGW to FHEM</h3>"
"  <input type='checkbox' id='scrollDataDiv' value='true' checked> Scrollen"
"  <button type='button' onclick=\"clearList('data');\">Leeren</button>"
"  &nbsp;Filter: <input id='dataDivFilter'>"
"  <span id='dataRowCount' class='info'></span>"
"  <div id='dataDiv' class='logbox' style='margin-top:8px'></div>"
"</div>"
"<div class='card'>"
"  <h3>&#128202; Debug Log</h3>"
"  <input type='checkbox' id='scrollLogDiv' value='true' checked> Scrollen"
"  <button type='button' onclick=\"clearList('log');\">Leeren</button>"
"  &nbsp;Filter: <input id='logDivFilter'>"
"  <span id='logRowCount' class='info'></span>"
"  <div id='logDiv' class='logbox' style='margin-top:8px'></div>"
"</div>"
"</body>";

// ═══════════════════════════════════════════════════
// Konstruktor & Hilfsmethoden
// ═══════════════════════════════════════════════════
WebFrontend::WebFrontend(int port) : m_webserver(port) {
  m_port = port;
  m_password = "";
  m_commandCallback = nullptr;
  m_hardwareCallback = nullptr;
}

ESP8266WebServer *WebFrontend::WebServer() { return &m_webserver; }
void WebFrontend::SetCommandCallback(CommandCallbackType callback) { m_commandCallback = callback; }
void WebFrontend::SetHardwareCallback(HardwareCallbackType callback) { m_hardwareCallback = callback; }
void WebFrontend::SetPassword(String password) { m_password = password; }

bool WebFrontend::IsAuthentified() {
  bool result = false;
  if (m_password.length() > 0) {
    if (m_webserver.hasHeader("Cookie")) {
      String cookie = m_webserver.header("Cookie");
      if (cookie.indexOf("ESPSESSIONID=1") != -1) result = true;
    }
    if (!result) {
      String header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
      m_webserver.sendContent(header);
    }
  } else { result = true; }
  return result;
}

String GetOption(String option, String defaultValue) {
  String result = F("<option value='");
  result += option;
  if (defaultValue == option) result += F("' selected>"); else result += F("'>");
  result += option;
  result += F("</option>");
  return result;
}

// ═══════════════════════════════════════════════════
// GetTop() – Dark/Light CSS + Header
// ═══════════════════════════════════════════════════
String WebFrontend::GetTop() {
  String result;
  result += F("<!DOCTYPE HTML><html lang='de'>");
  result += F("<meta charset='utf-8'/>");
  result += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  result += F("<head><title>");
  result += GetDisplayName();
  result += F("</title>");
  result += F("<style>");
  result += FPSTR(LGWMQTT_CSS);
  result += F("</style>");
  result += FPSTR(LGWMQTT_JS_THEME);
  result += F("</head>");
  result += F("<body>");
  // Header-Zeile
  result += F("<div class='hdr'>");
  result += F("<h1>&#127921; LaCrosseGateway");
  result += F("<span style='font-size:14px;font-weight:400;margin-left:12px;color:var(--txt2)'>V");
  result += m_stateManager->GetVersion();
  result += F(" &mdash; ");
  result += GetDisplayName();
  result += F("</span></h1>");
  // Theme-Toggle-Button
  result += F("<div class='theme-btn' onclick='toggleTheme()'>");
  result += F("<span id='ti'>&#127769;</span>");
  result += F("<span id='tt'>Dark Mode</span>");
  result += F("</div>");
  result += F("</div>");
  return result;
}

// ═══════════════════════════════════════════════════
// GetNavigation() – moderne Navbar
// ═══════════════════════════════════════════════════
String WebFrontend::GetNavigation() {
  String result = F("<nav style='margin-bottom:16px;padding-bottom:8px;border-bottom:1px solid var(--div)'>");
  result += F("<a href='/'>&#127968; Home</a>");
  result += F("<a href='setup'>&#9881;&#65039; Setup</a>");
  result += F("<a href='hardware'>&#128296; Hardware</a>");
  result += F("<a href='ota'>&#8593;&#65039; OTA-Update</a>");
  result += F("<a href='log'>&#128196; Log</a>");
  result += F("<a href='help'>&#10067; Help</a>");
  if (m_password.length() > 0)
    result += F("<a href='login?DISCONNECT=YES'>&#128274; Logout</a>");
  result += F("<a href='reset' style='color:var(--err)'>&#128260; Reboot</a>");
  result += F("</nav>");
  return result;
}

// ═══════════════════════════════════════════════════
// GetBottom() – Footer + Theme-Init
// ═══════════════════════════════════════════════════
String WebFrontend::GetBottom() {
  String result;
  result += F("<div class='footer'>");
  result += F("<p>LaCrosseGateway &mdash; ESP8266 MQTT Web Frontend</p>");
  result += F("</div>");
  result += F("</body></html>");
  return result;
}

// ═══════════════════════════════════════════════════
// GetRedirectToRoot(), BuildHardwareRow(),
// Handle()
// ═══════════════════════════════════════════════════
String WebFrontend::GetRedirectToRoot(String message) {
  String result;
  result += F("<html><head><meta http-equiv='refresh' content='5; URL=/'></head><body>");
  result += message;
  result += F("<br><br>Reboot, please wait a moment ...</body></html>");
  return result;
}

String WebFrontend::BuildHardwareRow(String text1, String text2, String text3) {
  return "<tr><td>" + text1 + "</td><td>" + text2 + "</td><td>" + text3 + "</td></tr>";
}

String WebFrontend::GetDisplayName() {
  String result;
  result += m_stateManager->GetHostname();
  result += " (";
  result += WiFi.localIP().toString();
  result += ")";
  return result;
}

void WebFrontend::Handle() { m_webserver.handleClient(); }

// ═══════════════════════════════════════════════════
// Begin() – Routen-Logik,
// Setup-Formular in Cards verpackt
// ═══════════════════════════════════════════════════
void WebFrontend::Begin(StateManager *stateManager, Logger *logger) {
  m_stateManager = stateManager;
  m_logger = logger;

  const char *headerKeys[] = { "User-Agent", "Cookie" };
  m_webserver.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(char*));

  // ── / (Home) ──────────────────────────────────────
  m_webserver.on("/", [this]() {
    if (IsAuthentified()) {
      String result;
      result += GetTop();
      result += GetNavigation();
      // Status-Card
      result += F("<div class='card' style='margin-bottom:12px'>");
      result += m_stateManager->GetHTML();
      result += F("</div>");
      result += GetBottom();
      m_webserver.send(200, "text/html", result);
    }
  });

  // ── /reset ────────────────────────────────────────
  m_webserver.on("/reset", [this]() {
    if (IsAuthentified()) {
      m_webserver.send(200, "text/html", GetRedirectToRoot());
      delay(1000);
      ESP.restart();
    }
  });

  // ── /command ──────────────────────────────────────
  m_webserver.on("/command", [this]() {
    if (IsAuthentified()) {
      if (m_commandCallback != NULL) {
        String command = m_webserver.arg("cmd");
        m_logger->println("Command from frontend: '" + command + "'");
        m_commandCallback(command);
        m_webserver.send(200, "text/html", "OK");
      }
    }
  });

  // ── /state ────────────────────────────────────────
  m_webserver.on("/state", [this]() {
    m_webserver.send(200, "text/xml", m_stateManager->GetXML());
  });

  // ── /help ─────────────────────────────────────────
  m_webserver.on("/help", [this]() {
    if (IsAuthentified()) {
      String result;
      result += GetTop();
      result += GetNavigation();
      result += F("<div class='card'>");
      result += FPSTR(help);
      result += F("</div>");
      result += GetBottom();
      m_webserver.send(200, "text/html", result);
    }
  });

  // ── /hardware ─────────────────────────────────────
  m_webserver.on("/hardware", [this]() {
    if (IsAuthentified()) {
      uint32_t freeHeap = ESP.getFreeHeap();
      String result;
      m_webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
      m_webserver.send(200);

      result += GetTop();
      result += GetNavigation();
      result += F("<div class='card'><h2>&#128296; Hardware Info</h2><table>");
      m_webserver.sendContent(result); result = "";

      result += BuildHardwareRow("ESP8266", "present :-)", "Core:&nbsp;" + String(ESP.getCoreVersion()) +
        "&nbsp;SDK:&nbsp;" + String(ESP.getSdkVersion()) +
        "&nbsp;Heap:&nbsp;" + String(freeHeap) +
        "&nbsp;Reset:&nbsp;" + ESP.getResetReason());
      m_webserver.sendContent(result); result = "";

      result += BuildHardwareRow("WiFi", String(WiFi.RSSI()) + " dBm",
        "Mode: " + WifiModeToString(WiFi.getMode()) +
        "&nbsp;&nbsp;Connect-Time: " + String(m_stateManager->GetWiFiConnectTime(), 1) + " s");
      m_webserver.sendContent(result); result = "";

      if (m_hardwareCallback != nullptr) {
        String rawData = m_hardwareCallback();
        result += "<tr><td>";
        rawData.replace("\t", "</td><td>");
        rawData.replace("\n", "</td></tr><tr><td>");
        rawData.replace(" ", "&nbsp;");
        result += rawData;
        result += "</td></tr>";
      }
      result += F("</table></div>");
      m_webserver.sendContent(result);
      m_webserver.sendContent(GetBottom());
      m_webserver.sendContent("");
    }
  });

  // ── /ota ──────────────────────────────────────────
  m_webserver.on("/ota", [this]() {
    if (IsAuthentified()) {
      Settings settings;
      settings.Read(m_logger);
      String result;
      result += GetTop();
      result += GetNavigation();
      result += F("<div class='card'><h2>&#8593;&#65039; OTA Update</h2>");
      result += F("<form method='get' action='ota_start'>");
      result += F("<p class='info'>Server: "); result += settings.Get("otaServer", ""); result += F("</p>");
      result += F("<p class='info'>Port: ");   result += settings.Get("otaPort", "");   result += F("</p>");
      result += F("<p class='info'>URL: ");    result += settings.Get("otaURL", "");    result += F("</p>");
      result += F("<br><input type='submit' value='Update and restart'></form>");
      result += F("</div>");
      result += GetBottom();
      m_webserver.send(200, "text/html", result);
    }
  });

  m_webserver.on("/ota_start", [this]() {
    if (IsAuthentified())
      m_webserver.send(200, "text/html", OTAUpdate::Start(m_logger));
  });

  // ── /save ─────────────────────────────────────────
  m_webserver.on("/save", [this]() {
    if (IsAuthentified()) {
      Settings settings;
      bool gotUseWiFi = false;
      for (byte i = 0; i < m_webserver.args(); i++) {
        settings.Add(m_webserver.argName(i), m_webserver.arg(i));
        if (m_webserver.argName(i) == "UseWiFi") gotUseWiFi = true;
      }
      if (!gotUseWiFi) settings.Add("UseWiFi", "false");

      bool saveIt = true;
      if (m_webserver.hasArg("frontPass") && m_webserver.hasArg("frontPass2")) {
        if (!m_webserver.arg("frontPass").equals(m_webserver.arg("frontPass2"))) {
          String content = GetTop();
          content += F("<div class='card' style='border-left:4px solid var(--err)'>");
          content += F("<h3 style='color:var(--err)'>&#10060; Fehler</h3>");
          content += F("<p>Passwords do not match</p></div>");
          content += GetBottom();
          m_webserver.send(200, "text/html", content);
          saveIt = false;
        }
      }

      if (m_webserver.hasArg("HostName")) {
        String hostname = m_webserver.arg("HostName");
        for (byte i = 0; i < hostname.length(); i++) {
          char ch = (char)hostname[i];
          if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '_')) {
            saveIt = false;
            String content = GetTop();
            content += F("<div class='card' style='border-left:4px solid var(--err)'>");
            content += F("<h3 style='color:var(--err)'>&#10060; Fehler</h3>");
            content += F("<p>Allowed characters for hostname: 0...9, a...z, A...Z, - and _</p></div>");
            content += GetBottom();
            m_webserver.send(200, "text/html", content);
            break;
          }
        }
      }

      if (saveIt) {
        String info = settings.Write();
        m_webserver.send(200, "text/html", GetRedirectToRoot("Settings saved<br>" + info));
        delay(1000);
        ESP.restart();
      }
    }
  });

  // ── /setup ────────────────────────────────────────
  m_webserver.on("/setup", [this]() {
    if (IsAuthentified()) {
      Settings settings;
      settings.Read(m_logger);

      m_webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
      m_webserver.send(200);
      m_webserver.sendContent(GetTop() + GetNavigation());

      // --- Card: WLAN ---
      String data;
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128225; WLAN-Einstellungen</h2>");
      data += F("<form method='get' action='save'><table>");
      data += F("<tr><td></td><td><p class='info'>3. Parameter = Timeout (s) bis zu SSID2 gewechselt wird</p></td></tr>");

      data += F("<tr><td><label>SSID / Passwort:</label></td><td>");
      data += F("<input name='ctSSID' size='40' maxlength='32' value='"); data += settings.Get("ctSSID", ""); data += F("'>");
      data += F(" <input type='password' name='ctPASS' size='40' maxlength='63' value='"); data += settings.Get("ctPASS", ""); data += F("'>");
      data += F(" <input name='Timeout1' size='5' maxlength='4' value='"); data += settings.Get("Timeout1", "15"); data += F("'></td></tr>");

      data += F("<tr><td><label>SSID2 / Passwort2:</label></td><td>");
      data += F("<input name='ctSSID2' size='40' maxlength='32' value='"); data += settings.Get("ctSSID2", ""); data += F("'>");
      data += F(" <input type='password' name='ctPASS2' size='40' maxlength='63' value='"); data += settings.Get("ctPASS2", ""); data += F("'>");
      data += F(" <input name='Timeout2' size='5' maxlength='4' value='"); data += settings.Get("Timeout2", "15"); data += F("'></td></tr>");

      data += F("<tr><td><label>Frontend-Passwort:</label></td><td>");
      data += F("<input name='frontPass' type='password' size='28' maxlength='60' value='"); data += settings.Get("frontPass", ""); data += F("'>");
      data += F(" Wiederholen: <input name='frontPass2' type='password' size='28' maxlength='60' value='"); data += settings.Get("frontPass2", ""); data += F("'>");
      data += F(" <span class='info'>(leer = kein Login erforderlich)</span></td></tr>");
      data += F("</table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: MQTT ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128225; MQTT-Einstellungen</h2><table>");
      data += F("<tr><td><label>IP-Adresse:</label></td><td>");
      data += F("<input name='serverIpMqtt' size='24' maxlength='15' value='"); data += settings.Get("serverIpMqtt", ""); data += F("'>");
      data += F(" <label style='display:inline'>Port:</label> <input name='serverPortMqtt' size='8' maxlength='5' value='"); data += settings.Get("serverPortMqtt", "1883"); data += F("'></td></tr>");

      data += F("<tr><td><label>Benutzername:</label></td><td>");
      data += F("<input name='mqttUser' size='36' maxlength='32' value='"); data += settings.Get("mqttUser", ""); data += F("'>");
      data += F(" <label style='display:inline'>Passwort:</label> <input type='password' name='mqttPass' size='36' maxlength='63' value='"); data += settings.Get("mqttPass", ""); data += F("'></td></tr>");

      data += F("<tr><td><label>MQTT Intervall/Topic:</label></td><td>");
      data += F("Intervall: <input name='pubInt' size='5' maxlength='5' value='"); data += settings.Get("pubInt", "20"); data += F("'>");
      data += F(" Topic: <input name='topic' size='24' maxlength='63' value='"); data += settings.Get("topic", "10"); data += F("'>");
      data += F(" Ext1: <input name='ext1' size='5' maxlength='4' value='"); data += settings.Get("ext1", "0"); data += F("'>");
      data += F(" Ext2: <input name='ext2' size='5' maxlength='5' value='"); data += settings.Get("ext2", "0"); data += F("'>");
      data += F(" Ext3: <input name='ext3' size='5' maxlength='5' value='"); data += settings.Get("ext3", "0"); data +=
