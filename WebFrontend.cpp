#include "WebFrontend.h"
#include <EEPROM.h>
#include "Settings.h"
#include <ESP8266httpUpdate.h>
#include "HTML.h"
#include "OTAUpdate.h"
#include "Help.h"
#include "ESP8266WiFiType.h"
#include "ESPTools.h"
#include <WiFiClientSecureBearSSL.h>

// ═══════════════════════════════════════════════════
// HILFSFUNKTION
// ═══════════════════════════════════════════════════
String WifiModeToString(WiFiMode_t mode) {
  switch (mode) {
  case WiFiMode_t::WIFI_AP:     return "Accespoint";
  case WiFiMode_t::WIFI_AP_STA: return "Accespoint + Station";
  case WiFiMode_t::WIFI_OFF:    return "Off";
  case WiFiMode_t::WIFI_STA:    return "Station";
  default:                      return "";
  }
}

// ═══════════════════════════════════════════════════
// Favicon als inline SVG Data-URI
// ═══════════════════════════════════════════════════
const char LGWMQTT_FAVICON[] PROGMEM =
  "<link rel='icon' type='image/svg+xml' "
  "href=\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E"
  "%3Ccircle cx='32' cy='32' r='30' fill='%2303a9f4'/%3E"
  "%3Ctext x='32' y='44' font-size='36' text-anchor='middle' fill='white' font-family='Arial'%3E"
  "L%3C/text%3E%3C/svg%3E\">";

// ═══════════════════════════════════════════════════
// Gemeinsames CSS
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
// Theme-Toggle JavaScript
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
// Log-Seite JavaScript + HTML
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
// NEU: BuildRadioCard() – Konfigurationsblock für
//      ein einzelnes Radio (Typ, Freq, DataRate,
//      Toggle-Bitraten, Toggle-Intervall)
// ═══════════════════════════════════════════════════
String WebFrontend::BuildRadioCard(Settings &settings, byte radioNbr) {
  String p = "Radio" + String(radioNbr);
  String result;

  result += F("<div class='card' style='margin-bottom:12px'>");
  result += F("<h2>&#128225; Radio #");
  result += String(radioNbr);
  result += F("</h2>");
  result += F("<p class='info'>Typ, Frequenz, feste Datenrate und optionaler Toggle zwischen mehreren Bitraten. "
              "Toggle-Maske: 1=17.241 / 2=9.579 / 4=8.842 kbps (kombinierbar)</p>");
  result += F("<table>");

  // Typ
  result += F("<tr><td><label>Typ:</label></td><td>");
  result += F("<select name='");
  result += p; result += F("Type'>");
  String typeVal = settings.Get(p + "Type", radioNbr == 1 ? "RFM69" : "---");
  result += GetOption("---",   typeVal);
  result += GetOption("RFM69", typeVal);
  result += GetOption("RFM95", typeVal);
  result += F("</select>");
  result += F(" <span class='info'>---&nbsp;=&nbsp;nicht verwendet</span></td></tr>");

  // Frequenz
  result += F("<tr><td><label>Frequenz (kHz):</label></td><td>");
  result += F("<input name='");
  result += p; result += F("Freq' size='12' maxlength='10' value='");
  result += settings.Get(p + "Freq", radioNbr == 1 ? "868310" : "");
  result += F("'> <span class='info'>z.B. 868310</span></td></tr>");

  // Feste Datenrate
  result += F("<tr><td><label>Feste Datenrate:</label></td><td>");
  result += F("<select name='");
  result += p; result += F("DataRate'>");
  String dr = settings.Get(p + "DataRate", "17.241");
  result += GetOption("17.241", dr);
  result += GetOption("9.579",  dr);
  result += GetOption("8.842",  dr);
  result += GetOption("6.631",  dr);
  result += GetOption("4.800",  dr);
  result += F("</select>");
  result += F(" <span class='info'>Wird genutzt wenn Toggle deaktiviert ist</span></td></tr>");

  // Toggle-Bitraten (Checkboxen, bitcodierte Maske)
  int toggleMask = settings.GetInt(p + "ToggleMask", 0);
  result += F("<tr><td><label>Toggle-Bitraten:</label></td><td>");
  result += F("<input name='"); result += p; result += F("Toggle17241' type='checkbox' value='true' ");
  result += (toggleMask & 1) ? F("checked") : F("");
  result += F("> 17.241 kbps&nbsp;&nbsp;");
  result += F("<input name='"); result += p; result += F("Toggle9579' type='checkbox' value='true' ");
  result += (toggleMask & 2) ? F("checked") : F("");
  result += F("> 9.579 kbps&nbsp;&nbsp;");
  result += F("<input name='"); result += p; result += F("Toggle8842' type='checkbox' value='true' ");
  result += (toggleMask & 4) ? F("checked") : F("");
  result += F("> 8.842 kbps</td></tr>");

  // Toggle-Intervall
  result += F("<tr><td><label>Toggle-Intervall (s):</label></td><td>");
  result += F("<input name='");
  result += p; result += F("ToggleInterval' size='6' maxlength='5' value='");
  result += settings.Get(p + "ToggleInterval", radioNbr == 1 ? "30" : "0");
  result += F("'> <span class='info'>0&nbsp;=&nbsp;deaktiviert</span></td></tr>");

  result += F("</table></div>");
  return result;
}

// ═══════════════════════════════════════════════════
// GetTop() – Favicon + Dark/Light CSS + Header
// ═══════════════════════════════════════════════════
String WebFrontend::GetTop() {
  String result;
  result += F("<!DOCTYPE HTML><html lang='de'>");
  result += F("<meta charset='utf-8'/>");
  result += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  result += F("<head><title>");
  result += GetDisplayName();
  result += F("</title>");
  result += FPSTR(LGWMQTT_FAVICON);
  result += F("<style>");
  result += FPSTR(LGWMQTT_CSS);
  result += F("</style>");
  result += FPSTR(LGWMQTT_JS_THEME);
  result += F("</head>");
  result += F("<body>");
  result += F("<div class='hdr'>");
  result += F("<h1>&#127921; LaCrosseGateway");
  result += F("<span style='font-size:14px;font-weight:400;margin-left:12px;color:var(--txt2)'>V");
  result += m_stateManager->GetVersion();
  result += F(" &mdash; ");
  result += GetDisplayName();
  result += F("</span></h1>");
  result += F("<div class='theme-btn' onclick='toggleTheme()'>");
  result += F("<span id='ti'>&#127769;</span>");
  result += F("<span id='tt'>Dark Mode</span>");
  result += F("</div>");
  result += F("</div>");
  return result;
}

// ═══════════════════════════════════════════════════
// GetNavigation()
// ═══════════════════════════════════════════════════
String WebFrontend::GetNavigation() {
  String result = F("<nav style='margin-bottom:16px;padding-bottom:8px;border-bottom:1px solid var(--div)'>");
  result += F("<a href='/'>&#127968; Home</a>");
  result += F("<a href='setup'>&#9881;&#65039; Setup</a>");
  result += F("<a href='hardware'>&#128296; Hardware</a>");
  result += F("<a href='ota'>&#8593;&#65039; OTA-Update</a>");
  result += F("<a href='update'>&#128190; BIN-Update</a>");
  result += F("<a href='log'>&#128196; Log</a>");
  result += F("<a href='help'>&#10067; Help</a>");
  if (m_password.length() > 0)
    result += F("<a href='login?DISCONNECT=YES'>&#128274; Logout</a>");
  result += F("<a href='reset' style='color:var(--err)'>&#128260; Reboot</a>");
  result += F("</nav>");
  return result;
}

// ═══════════════════════════════════════════════════
// GetBottom()
// ═══════════════════════════════════════════════════
String WebFrontend::GetBottom() {
  String result;
  result += F("<div class='footer'>");
  result += F("<p>LaCrosseGateway &mdash; ESP8266 MQTT Web Frontend</p>");
  result += F("</div>");
  result += F("</body></html>");
  return result;
}

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

String GetLatestGithubVersion(Logger* logger) {
  if (WiFi.status() != WL_CONNECTED) return "";
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://raw.githubusercontent.com/steigerbalett/LaCrosseGatewayMQTT/main/version.txt")) {
    if (logger) logger->println(F("GitHub-Version: http.begin fehlgeschlagen"));
    return "";
  }
  int code = http.GET();
  String version = "";
  if (code == HTTP_CODE_OK) {
    version = http.getString();
    version.trim();
  } else {
    if (logger) logger->println(F("GitHub-Version: Fehler, Code=") + String(code));
  }
  http.end();
  return version;
}

// ═══════════════════════════════════════════════════
// Begin() – alle Routen
// ═══════════════════════════════════════════════════
void WebFrontend::Begin(StateManager *stateManager, Logger *logger) {
  m_stateManager = stateManager;
  m_logger = logger;

  const char *headerKeys[] = { "User-Agent", "Cookie" };
  m_webserver.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(char*));

  // ── / (Home) ────────────────────────────────────
  m_webserver.on("/", [this]() {
    if (IsAuthentified()) {
      String result;
      result += GetTop();
      result += GetNavigation();
      result += F("<div class='card' style='margin-bottom:12px'>");
      result += m_stateManager->GetHTML();
      result += F("</div>");
      result += GetBottom();
      m_webserver.send(200, "text/html", result);
    }
  });

  // ── /reset ──────────────────────────────────────
  m_webserver.on("/reset", [this]() {
    if (IsAuthentified()) {
      m_webserver.send(200, "text/html", GetRedirectToRoot());
      delay(1000);
      ESP.restart();
    }
  });

  // ── /command ────────────────────────────────────
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

  // ── /state ──────────────────────────────────────
  m_webserver.on("/state", [this]() {
    m_webserver.send(200, "text/xml", m_stateManager->GetXML());
  });

  // ── /help ───────────────────────────────────────
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

  // ── /hardware ───────────────────────────────────
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

  // ── /ota ────────────────────────────────────────
  m_webserver.on("/ota", [this]() {
    if (IsAuthentified()) {
      Settings settings;
      settings.Read(m_logger);
      String localVersion = m_stateManager->GetVersion();

      String result;
      result += GetTop();
      result += GetNavigation();

      // Installierte Version
      result += F("<div class='card' style='margin-bottom:12px'>");
      result += F("<h2>&#127381; Firmware-Info</h2><table>");
      result += F("<tr><td>Installierte Version:</td><td><span class='badge ok'>V");
      result += localVersion;
      result += F("</span></td></tr></table></div>");

      // GitHub Releases per Browser-JS
      result += F("<div class='card' style='margin-bottom:12px'>");
      result += F("<h2>&#128190; GitHub Release installieren</h2>");
      result += F("<p class='info'>Die Liste wird direkt von der GitHub-API geladen (kein HTTPS auf dem ESP n&ouml;tig).</p>");
      result += F("<script>");
      result += F("function loadReleases(){");
      result += F("  var out=document.getElementById('relDiv');");
      result += F("  out.innerHTML='<p class=\\'info\\'>Lade Releases von GitHub...</p>';");
      result += F("  fetch('https://api.github.com/repos/steigerbalett/LaCrosseGatewayMQTT/releases?per_page=20')");
      result += F("  .then(function(r){return r.json();})");
      result += F("  .then(function(data){");
      result += F("    var html='<table><thead><tr><th>Version</th><th>Typ</th><th>Datum</th><th>Datei ausw&auml;hlen &amp; flashen</th></tr></thead><tbody>';");
      result += F("    data.forEach(function(rel){");
      result += F("      var badge=rel.prerelease?'<span class=\\'badge warn\\'>Vorab</span>':'<span class=\\'badge ok\\'>Stabil</span>';");
      result += F("      var date=rel.published_at?rel.published_at.substring(0,10):'';");
      result += F("      var assets=rel.assets.filter(function(a){return a.name.endsWith('.bin');});");
      result += F("      var links='';");
      result += F("      if(assets.length===0){links='<span class=\\'info\\'>keine .bin</span>';}");
      result += F("      else{assets.forEach(function(a){");
      result += F("        links+='<form method=\\'POST\\' action=\\'/ota_gh\\' style=\\'display:inline;margin:2px\\'>'");
      result += F("          +'<input type=\\'hidden\\' name=\\'url\\' value=\\''+a.browser_download_url+'\\'/>'");
      result += F("          +'<button type=\\'submit\\'>&#8595; '+a.name+'</button></form>';");
      result += F("      });}");
      result += F("      html+='<tr><td>'+rel.tag_name+'</td><td>'+badge+'</td><td>'+date+'</td><td>'+links+'</td></tr>';");
      result += F("    });");
      result += F("    html+='</tbody></table>';");
      result += F("    document.getElementById('relDiv').innerHTML=html;");
      result += F("  }).catch(function(e){");
      result += F("    document.getElementById('relDiv').innerHTML='<p style=\\'color:var(--err)\\'>Fehler: '+e+'</p>';");
      result += F("  });");
      result += F("}");
      result += F("window.addEventListener('DOMContentLoaded', loadReleases);");
      result += F("</script>");
      result += F("<div id='relDiv'><p class='info'>JavaScript wird ben&ouml;tigt.</p></div>");
      result += F("</div>");

      // OTA-Server
      result += F("<div class='card' style='margin-bottom:12px'>");
      result += F("<h2>&#8593;&#65039; OTA-Server Update</h2>");
      result += F("<form method='get' action='ota_start'>");
      result += F("<p class='info'>Server: "); result += settings.Get("otaServer", ""); result += F("</p>");
      result += F("<p class='info'>Port: ");   result += settings.Get("otaPort",   ""); result += F("</p>");
      result += F("<p class='info'>URL: ");    result += settings.Get("otaURL",    ""); result += F("</p>");
      result += F("<br><input type='submit' value='OTA-Update starten'></form>");
      result += F("</div>");

      result += GetBottom();
      m_webserver.send(200, "text/html", result);
    }
  });

  // ── /ota_gh (GitHub Release direkt flashen) ─────
  m_webserver.on("/ota_gh", HTTP_POST, [this]() {
    if (!IsAuthentified()) return;
    String url = m_webserver.arg("url");
    if (url.length() == 0) {
      m_webserver.send(400, "text/plain", "Keine URL angegeben");
      return;
    }
    String result;
    result += GetTop();
    result += GetNavigation();
    m_logger->println("OTA GitHub: " + url);

    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(1024, 1024);
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, url);

    switch (ret) {
      case HTTP_UPDATE_OK:
        result += F("<div class='card' style='border-left:4px solid var(--ok)'>");
        result += F("<h3 style='color:var(--ok)'>&#9989; Update erfolgreich!</h3>");
        result += F("<p>Das Ger&auml;t startet neu...</p></div>");
        result += GetBottom();
        m_webserver.send(200, "text/html", result);
        delay(1000);
        ESP.restart();
        break;
      case HTTP_UPDATE_FAILED:
        result += F("<div class='card' style='border-left:4px solid var(--err)'>");
        result += F("<h3 style='color:var(--err)'>&#10060; Update fehlgeschlagen!</h3>");
        result += F("<p>"); result += ESPhttpUpdate.getLastErrorString(); result += F("</p></div>");
        result += GetBottom();
        m_webserver.send(200, "text/html", result);
        break;
      case HTTP_UPDATE_NO_UPDATES:
        result += F("<div class='card' style='border-left:4px solid var(--warn)'>");
        result += F("<h3>Kein Update n&ouml;tig</h3></div>");
        result += GetBottom();
        m_webserver.send(200, "text/html", result);
        break;
    }
  });

  // ── /ota_start ──────────────────────────────────
  m_webserver.on("/ota_start", [this]() {
    if (IsAuthentified())
      m_webserver.send(200, "text/html", OTAUpdate::Start(m_logger));
  });

  // ── /save ───────────────────────────────────────
  m_webserver.on("/save", HTTP_POST, [this]() {
    if (IsAuthentified()) {
      Settings settings;
      bool gotUseWiFi = false;
      for (byte i = 0; i < m_webserver.args(); i++) {
        settings.Add(m_webserver.argName(i), m_webserver.arg(i));
        if (m_webserver.argName(i) == "UseWiFi") gotUseWiFi = true;
      }
      if (!gotUseWiFi) settings.Add("UseWiFi", "false");

      // NEU: Toggle-Masken für alle 5 Radios aus Checkboxen berechnen
      for (byte radioNbr = 1; radioNbr <= 5; radioNbr++) {
        String p = "Radio" + String(radioNbr);
        int mask = 0;
        if (m_webserver.hasArg(p + "Toggle17241")) mask |= 1;
        if (m_webserver.hasArg(p + "Toggle9579"))  mask |= 2;
        if (m_webserver.hasArg(p + "Toggle8842"))  mask |= 4;
        settings.Add(p + "ToggleMask", String(mask));
        if (!m_webserver.hasArg(p + "ToggleInterval"))
          settings.Add(p + "ToggleInterval", "0");
      }

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

  // ── /setup ──────────────────────────────────────
  m_webserver.on("/setup", [this]() {
    if (IsAuthentified()) {
      Settings settings;
      settings.Read(m_logger);

      m_webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
      m_webserver.send(200);
      m_webserver.sendContent(GetTop() + GetNavigation());

      String data;

      // --- Card: WLAN ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128225; WLAN-Einstellungen</h2>");
      data += F("<form method='post' action='save'><table>");
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
      data += F(" Ext3: <input name='ext3' size='5' maxlength='5' value='"); data += settings.Get("ext3", "0"); data += F("'></td></tr>");
      data += F("</table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: Netzwerk ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#127760; Netzwerk (statisch)</h2>");
      data += F("<p class='info'>Wenn IP, Maske oder Gateway leer, wird DHCP verwendet.</p><table>");
      data += F("<tr><td><label>IP-Adresse:</label></td><td>");
      data += F("<input name='staticIP' size='24' maxlength='15' value='"); data += settings.Get("staticIP", ""); data += F("'>");
      data += F(" <label style='display:inline'>Maske:</label> <input name='staticMask' size='24' maxlength='15' value='"); data += settings.Get("staticMask", ""); data += F("'>");
      data += F(" <label style='display:inline'>Gateway:</label> <input name='staticGW' size='24' maxlength='15' value='"); data += settings.Get("staticGW", ""); data += F("'></td></tr>");
      data += F("<tr><td><label>Hostname:</label></td><td>");
      data += F("<input name='HostName' size='24' maxlength='63' value='"); data += settings.Get("HostName", "LaCrosseGateway"); data += F("'>");
      data += F(" <label style='display:inline'>Startup-Delay (s):</label> <input name='StartupDelay' size='5' maxlength='4' value='"); data += settings.Get("StartupDelay", "0"); data += F("'></td></tr>");
      data += F("</table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: Interne Sensoren ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#127909; Interne Sensoren</h2><table>");
      data += F("<tr><td><label>Sensoren:</label></td><td>");
      data += F("ID: <input name='ISID' size='5' maxlength='4' value='"); data += settings.Get("ISID", "0"); data += F("'>");
      data += F(" Intervall: <input name='ISIV' size='5' maxlength='5' value='"); data += settings.Get("ISIV", "10"); data += F("'>");
      data += F(" Hoehe: <input name='Altitude' size='5' maxlength='4' value='"); data += settings.Get("Altitude", "0"); data += F("'>");
      data += F(" T-Korr: <input name='CorrT' size='5' maxlength='5' value='"); data += settings.Get("CorrT", "0"); data += F("'>");
      data += F(" H-Korr: <input name='CorrH' size='5' maxlength='5' value='"); data += settings.Get("CorrH", "0"); data += F("'></td></tr>");
      data += F("</table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: Ports & Serial Bridges ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128268; Ports &amp; Serial Bridges</h2><table>");
      data += F("<tr><td><label>Daten-Ports:</label></td><td>");
      data += F("<input name='DataPort1' maxlength='5' size='8' value='"); data += settings.Get("DataPort1", "81"); data += F("'>&nbsp;");
      data += F("<input name='DataPort2' maxlength='5' size='8' value='"); data += settings.Get("DataPort2", ""); data += F("'>&nbsp;");
      data += F("<input name='DataPort3' maxlength='5' size='8' value='"); data += settings.Get("DataPort3", ""); data += F("'></td></tr>");
      data += F("<tr><td><label>Serial Bridge 1:</label></td><td>");
      data += F("Port: <input name='SerialBridgePort' maxlength='5' size='8' value='"); data += settings.Get("SerialBridgePort", ""); data += F("'>");
      data += F(" Baud: <input name='SerialBridgeBaud' maxlength='6' size='8' value='"); data += settings.Get("SerialBridgeBaud", "57600"); data += F("'></td></tr>");
      data += F("<tr><td><label>Serial Bridge 2:</label></td><td>");
      data += F("Port: <input name='SerialBridge2Port' maxlength='5' size='8' value='"); data += settings.Get("SerialBridge2Port", ""); data += F("'>");
      data += F(" Baud: <input name='SerialBridge2Baud' maxlength='6' size='8' value='"); data += settings.Get("SerialBridge2Baud", "57600"); data += F("'></td></tr>");
      data += F("<tr><td><label>Soft Serial Bridge:</label></td><td>");
      data += F("Port: <input name='SSBridgePort' maxlength='5' size='8' value='"); data += settings.Get("SSBridgePort", ""); data += F("'>");
      data += F(" Baud: <input name='SSBridgeBaud' maxlength='6' size='8' value='"); data += settings.Get("SSBridgeBaud", "9600"); data += F("'>&nbsp;");
      data += F("<input name='IsNextion' type='checkbox' value='true' "); data += settings.Get("IsNextion", "") == "true" ? "checked" : ""; data += F("> Nextion-Display&nbsp;");
      data += F("<input name='AddUnits' type='checkbox' value='true' "); data += settings.Get("AddUnits", "") == "true" ? "checked" : ""; data += F("> Einheiten hinzufuegen");
      data += F("</td></tr></table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: RFM95 ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128225; RFM95</h2><table>");
      data += F("<tr><td><label>RFM95:</label></td><td>");
      data += F("SF: <select name='SF95' style='width:70px'>");
      String sfValue = settings.Get("SF95", "SF7");
      data += GetOption("SF6",  sfValue); data += GetOption("SF7",  sfValue);
      data += GetOption("SF8",  sfValue); data += GetOption("SF9",  sfValue);
      data += GetOption("SF10", sfValue); data += GetOption("SF11", sfValue);
      data += GetOption("SF12", sfValue);
      data += F("</select>&nbsp;BW: <select name='BW95' style='width:70px'>");
      String bwValue = settings.Get("BW95", "125");
      data += GetOption("7.8",   bwValue); data += GetOption("10.4",  bwValue);
      data += GetOption("15.6",  bwValue); data += GetOption("20.8",  bwValue);
      data += GetOption("31.25", bwValue); data += GetOption("41.7",  bwValue);
      data += GetOption("62.6",  bwValue); data += GetOption("125",   bwValue);
      data += GetOption("250",   bwValue); data += GetOption("500",   bwValue);
      data += F("</select></td></tr></table></div>");
      m_webserver.sendContent(data); data = "";

      // ═══════════════════════════════════════════
      // NEU: Cards Radio #1 ... #5
      // ═══════════════════════════════════════════
      for (byte radioNbr = 1; radioNbr <= 5; radioNbr++) {
        data += BuildRadioCard(settings, radioNbr);
        m_webserver.sendContent(data);
        data = "";
      }

      // --- Card: LGW-Betrieb ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128268; LGW-Betrieb (RFM69 / LaCrosse)</h2>");
      data += F("<p class='info'>Diese Einstellungen entsprechen den FHEM-Attributen des LaCrosseGateway-Moduls.</p>");
      data += F("<table>");
      data += F("<tr><td><label>Modus:</label></td><td>");
      data += F("<select name='lgwMode'>");
      String lgwMode = settings.Get("lgwMode", "0");
      data += GetOption("0", lgwMode); data += GetOption("1", lgwMode); data += GetOption("2", lgwMode);
      data += F("</select> <span class='info'>0=normal, 1=PCA301, 2=EM</span></td></tr>");
      data += F("<tr><td><label>Kanal:</label></td><td>");
      data += F("<input name='lgwChannel' size='5' maxlength='3' value='"); data += settings.Get("lgwChannel", "0"); data += F("'> <span class='info'>0–255</span></td></tr>");
      data += F("<tr><td><label>RFM-Frequenz (kHz):</label></td><td>");
      data += F("<input name='lgwFreq' size='12' maxlength='10' value='"); data += settings.Get("lgwFreq", "868300"); data += F("'> <span class='info'>z.B. 868300</span></td></tr>");
      data += F("<tr><td><label>Sendeleistung (dBm):</label></td><td>");
      data += F("<select name='lgwPower'>");
      String lgwPwr = settings.Get("lgwPower", "10");
      for (int p = 0; p <= 20; p += 2) { data += GetOption(String(p), lgwPwr); }
      data += F("</select></td></tr>");
      data += F("<tr><td><label>Datenrate (Baud):</label></td><td>");
      data += F("<select name='lgwDataRate'>");
      String lgwDR = settings.Get("lgwDataRate", "17241");
      data += GetOption("4800",  lgwDR); data += GetOption("9600",  lgwDR);
      data += GetOption("17241", lgwDR); data += GetOption("19200", lgwDR);
      data += GetOption("38400", lgwDR); data += GetOption("57600", lgwDR);
      data += F("</select> <span class='info'>Standard: 17241</span></td></tr>");
      data += F("<tr><td><label>RSSI-Filter (dBm):</label></td><td>");
      data += F("<input name='lgwRssiThreshold' size='7' maxlength='5' value='"); data += settings.Get("lgwRssiThreshold", "-200"); data += F("'> <span class='info'>Pakete darunter ignorieren</span></td></tr>");
      data += F("<tr><td><label>Encrypt-Key (16 Byte Hex):</label></td><td>");
      data += F("<input name='lgwEncryptKey' size='40' maxlength='32' placeholder='leer = keine Verschluesselung' value='"); data += settings.Get("lgwEncryptKey", ""); data += F("'></td></tr>");
      data += F("<tr><td><label>Watchdog-Timeout (s):</label></td><td>");
      data += F("<input name='lgwWatchdog' size='7' maxlength='5' value='"); data += settings.Get("lgwWatchdog", "0"); data += F("'> <span class='info'>0 = deaktiviert</span></td></tr>");
      data += F("</table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: Sende-Verhalten ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128228; Sende-Verhalten</h2><table>");
      data += F("<tr><td><label>SendMode:</label></td><td>");
      data += F("<select name='SendMode'>");
      String sendMode = settings.Get("SendMode", "0");
      data += GetOption("0", sendMode); data += GetOption("1", sendMode); data += GetOption("2", sendMode);
      data += F("</select> <span class='info'>0=kein Senden, 1=alle, 2=nur neue IDs</span></td></tr>");
      data += F("<tr><td><label>SendRetries:</label></td><td>");
      data += F("<input name='SendRetries' size='5' maxlength='3' value='"); data += settings.Get("SendRetries", "3"); data += F("'></td></tr>");
      data += F("<tr><td><label>Optionen:</label></td><td>");
      data += F("<input name='SendHumidity' type='checkbox' value='true' "); data += settings.Get("SendHumidity", "true") == "true" ? "checked" : ""; data += F("> Luftfeuchte&nbsp;&nbsp;");
      data += F("<input name='SendBatteryBeep' type='checkbox' value='true' "); data += settings.Get("SendBatteryBeep", "true") == "true" ? "checked" : ""; data += F("> Batterie-Warnung&nbsp;&nbsp;");
      data += F("<input name='AsDataFull' type='checkbox' value='true' "); data += settings.Get("AsDataFull", "false") == "true" ? "checked" : ""; data += F("> Vollst. Daten&nbsp;&nbsp;");
      data += F("<input name='ToggleLed' type='checkbox' value='true' "); data += settings.Get("ToggleLed", "true") == "true" ? "checked" : ""; data += F("> LED blinken</td></tr>");
      data += F("</table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: Optionen/Flags ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#9881;&#65039; Optionen</h2><table>");
      data += F("<tr><td><label>Flags:</label></td><td>");
      data += F("<input name='UseWiFi' type='checkbox' value='true' "); data += settings.Get("UseWiFi", "true") == "true" ? "checked" : ""; data += F("> WiFi&nbsp;");
      data += F("<input name='UseMDNS' type='checkbox' value='true' "); data += settings.Get("UseMDNS", "") == "true" ? "checked" : ""; data += F("> MDNS&nbsp;");
      data += F("<input name='SendAnalog' type='checkbox' value='true' "); data += settings.Get("SendAnalog", "") == "true" ? "checked" : ""; data += F("> Analog senden&nbsp;");
      data += F("U@1023: <input name='UAnalog1023' maxlength='5' size='7' value='"); data += settings.Get("UAnalog1023", "1000"); data += F("'> mV&nbsp;");
      data += F("<input name='PRD' type='checkbox' value='true' "); data += settings.Get("PRD", "false") == "true" ? "checked" : ""; data += F("> Druck mit Dezimalen");
      data += F("</td></tr></table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: MCP23008 ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128268; MCP23008</h2><table><tr><td><label>IO-Ports:</label></td><td>");
      for (byte nbr = 0; nbr < 8; nbr++) {
        data += F("IO "); data += String(nbr); data += F(": ");
        data += F("<select name='IO"); data += String(nbr); data += F("' style='width:130px'>");
        String value = settings.Get("IO" + String(nbr), "Input");
        data += GetOption("Input", value);   data += GetOption("Output", value);
        data += GetOption("OLED Off", value); data += GetOption("OLED On", value);
        data += GetOption("OLED mode=s",    value); data += GetOption("OLED mode=t",    value);
        data += GetOption("OLED mode=h",    value); data += GetOption("OLED mode=th",   value);
        data += GetOption("OLED mode=thp",  value); data += GetOption("OLED mode=thps", value);
        data += F("</select>&nbsp;");
        if (nbr == 3) data += F("<br>");
        m_webserver.sendContent(data); data = "";
      }
      data += F("</td></tr></table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: OLED ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128250; OLED-Display</h2>");
      data += F("<p class='info'>Werte: 'on', 'off' oder Sekunden bis 'off', 2. Parameter: Modus (th, thp, ...)</p><table>");
      data += F("<tr><td><label>OLED Start:</label></td><td>");
      data += F("On/Off: <input name='oledStart' size='8' maxlength='6' value='"); data += settings.Get("oledStart", "on"); data += F("'>");
      data += F(" Modus: <input name='oledMode' size='12' maxlength='16' value='"); data += settings.Get("oledMode", ""); data += F("'>&nbsp;");
      data += F("<input name='oled13' type='checkbox' value='true' "); data += settings.Get("oled13", "false") == "true" ? "checked" : ""; data += F("> 1.3\"");
      data += F("</td></tr></table></div>");
      m_webserver.sendContent(data); data = "";

      // --- Card: Weitere Einstellungen ---
      data += F("<div class='card' style='margin-bottom:12px'>");
      data += F("<h2>&#128196; Weitere Einstellungen</h2><table>");
      data += F("<tr><td></td><td><p class='info'>KV-Interval: 'off' zum Deaktivieren</p></td></tr>");
      data += F("<tr><td><label>KV-Interval:</label></td><td>");
      data += F("<input name='KVInterval' size='10' maxlength='3' value='"); data += settings.Get("KVInterval", "10"); data += F("'>");
      data += F(" <label style='display:inline'>KV-Identity:</label> <input name='KVIdentity' size='24' maxlength='20' value='"); data += settings.Get("KVIdentity", String(ESP.getChipId())); data += F("'></td></tr>");
      data += F("<tr><td><label>OTA-Server:</label></td><td><input name='otaServer' size='50' maxlength='40' value='"); data += settings.Get("otaServer", ""); data += F("'></td></tr>");
      data += F("<tr><td><label>OTA-Port:</label></td><td><input name='otaPort' size='10' maxlength='5' value='"); data += settings.Get("otaPort", ""); data += F("'></td></tr>");
      data += F("<tr><td><label>OTA-URL:</label></td><td><input name='otaURL' size='50' maxlength='80' value='"); data += settings.Get("otaURL", ""); data += F("'></td></tr>");
      data += F("<tr><td><label>PCA301:</label></td><td><input name='PCA301Plugs' size='50' maxlength='160' value='"); data += settings.Get("PCA301Plugs", ""); data += F("'></td></tr>");
      data += F("<tr><td></td><td><p class='info'>Nur fuer Entwicklung</p></td></tr>");
      data += F("<tr><td><label>Flags:</label></td><td><input name='Flags' size='50' maxlength='80' value='"); data += settings.Get("Flags", ""); data += F("'></td></tr>");
      data += F("</table>");
      data += F("<br><input type='submit' value='Speichern und neu starten'></form>");
      data += F("</div>");
      m_webserver.sendContent(data); data = "";

      m_webserver.sendContent(GetBottom());
      m_webserver.sendContent("");
    }
  });

  // ── /getLogData ──────────────────────────────────
  m_webserver.on("/getLogData", [this]() {
    String data = "";
    if (m_logger->IsEnabled()) {
      while (m_logger->Available())
        data += m_logger->Pop() + "\n";
    } else {
      data += F("SYS: ***CLEARLOG***\n");
      data += F("DATA:Logger is disabled\n");
      data += F("SYS:Logger is disabled\n");
    }
    m_webserver.send(200, "text/html", data);
  });

  // ── /log ────────────────────────────────────────
  m_webserver.on("/log", [this]() {
    if (IsAuthentified()) {
      String result;
      result += GetTop();
      result += GetNavigation();
      result += FPSTR(on_log);
      result += GetBottom();
      m_webserver.send(200, "text/html", result);
    }
  });

  // ── /login ──────────────────────────────────────
  m_webserver.on("/login", [this]() {
    String msg;
    if (m_webserver.hasArg("DISCONNECT")) {
      m_webserver.sendContent(F("HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n"));
      return;
    }
    if (m_webserver.hasArg("PASSWORD")) {
      if (m_webserver.arg("PASSWORD") == m_password) {
        m_webserver.sendContent(F("HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n"));
        return;
      }
      msg = "Login fehlgeschlagen";
    }
    String content;
    content += F("<!DOCTYPE HTML><html><head><meta charset='utf-8'>");
    content += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    content += FPSTR(LGWMQTT_FAVICON);
    content += F("<style>");
    content += FPSTR(LGWMQTT_CSS);
    content += F("</style></head><body>");
    content += F("<div style='display:flex;justify-content:center;align-items:center;min-height:80vh'>");
    content += F("<div class='card' style='min-width:320px;text-align:center'>");
    content += F("<h2>&#127921; LaCrosseGateway V");
    content += m_stateManager->GetVersion();
    content += F("</h2>");
    content += F("<form action='/login' method='POST' style='box-shadow:none;padding:0'>");
    content += F("<label>Passwort:</label>");
    content += F("<input type='password' name='PASSWORD' placeholder='Passwort eingeben'>");
    content += F("<input type='submit' value='Anmelden' style='width:100%;margin-top:8px'>");
    content += F("</form>");
    if (msg.length() > 0) {
      content += F("<p style='color:var(--err);margin-top:12px'>&#10060; ");
      content += msg;
      content += F("</p>");
    }
    content += F("</div></div></body></html>");
    m_webserver.send(200, "text/html", content);
  });

  // ── /update (GET) ────────────────────────────────
  m_webserver.on("/update", HTTP_GET, [this]() {
    if (IsAuthentified()) {
      String result;
      result += GetTop();
      result += GetNavigation();
      result += F("<div class='card'>");
      result += F("<h2>&#128190; Firmware Update (BIN-Upload)</h2>");
      result += F("<p class='info'>Lade eine <code>.bin</code>-Datei von deinem Computer hoch, um die Firmware zu aktualisieren.</p>");
      result += F("<form method='POST' action='/update_do' enctype='multipart/form-data'>");
      result += F("<label>Firmware-Datei (.bin):</label>");
      result += F("<input type='file' name='firmware' accept='.bin' required style='margin-bottom:12px'>");
      result += F("<br><input type='submit' value='&#128190; Firmware flashen'>");
      result += F("</form></div>");
      result += GetBottom();
      m_webserver.send(200, "text/html", result);
    }
  });

  // ── /update_do (POST) ────────────────────────────
  m_webserver.on("/update_do", HTTP_POST,
    [this]() {
      String result;
      result += GetTop();
      result += GetNavigation();
      if (Update.hasError()) {
        result += F("<div class='card' style='border-left:4px solid var(--err)'>");
        result += F("<h3 style='color:var(--err)'>&#10060; Update fehlgeschlagen!</h3>");
        result += F("<p>"); result += Update.getErrorString(); result += F("</p></div>");
        result += GetBottom();
        m_webserver.send(200, "text/html", result);
      } else {
        result += F("<div class='card' style='border-left:4px solid var(--ok)'>");
        result += F("<h3 style='color:var(--ok)'>&#9989; Update erfolgreich!</h3>");
        result += F("<p>Das Ger&auml;t wird jetzt neu gestartet...</p></div>");
        result += GetBottom();
        m_webserver.send(200, "text/html", result);
        delay(1000);
        ESP.restart();
      }
    },
    [this]() {
      HTTPUpload& upload = m_webserver.upload();
      if (upload.status == UPLOAD_FILE_START) {
        m_logger->println("Firmware-Update gestartet: " + upload.filename);
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace, U_FLASH))
          m_logger->println("Update.begin fehlgeschlagen: " + String(Update.getErrorString()));
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          m_logger->println("Update.write fehlgeschlagen: " + String(Update.getErrorString()));
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true))
          m_logger->println("Firmware-Update abgeschlossen. Groesse: " + String(upload.totalSize));
        else
          m_logger->println("Update.end fehlgeschlagen: " + String(Update.getErrorString()));
      }
    }
  );

  m_webserver.onNotFound([this]() {
    m_webserver.send(404, "text/plain", "Not Found");
  });

  m_webserver.begin();
}
