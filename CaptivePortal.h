#ifndef CAPTIVEPORTAL_H
#define CAPTIVEPORTAL_H

// =============================================================
//  CaptivePortal.h  –  Fixed & Debug-Version
//  Fix: Verhindert reentrant-yield-Panic (core_esp8266_main:191)
//  durch Ersetzen von delay() durch ESP.wdtFeed() + optimistic_yield
//  Debug-Ausgaben an allen kritischen Punkten
// =============================================================

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

// ---- Debug-Makro (auf Serial, immer aktiv) ----
#define CP_DEBUG(msg)        do { Serial.print(F("[CP] ")); Serial.println(F(msg)); Serial.flush(); } while(0)
#define CP_DEBUGF(fmt, ...)  do { Serial.printf("[CP] " fmt "\n", ##__VA_ARGS__); Serial.flush(); } while(0)
#define CP_HEAP()            do { Serial.printf("[CP] Free Heap: %u  ContStack: %u\n", \
                                  ESP.getFreeHeap(), ESP.getFreeContStack()); Serial.flush(); } while(0)

// ---- Konfiguration ----
static const byte    DNS_PORT          = 53;
static const char*   AP_DEFAULT_PREFIX = "LaCrosseGateway_";
static const uint32_t CP_TIMEOUT_MS    = 300000UL; // 5 Minuten, dann Neustart

// ---- EEPROM-Layout (muss mit Hauptprogramm übereinstimmen) ----
// Passe Offsets ggf. an dein EEPROM-Layout an:
#define EEPROM_SSID_OFFSET   0
#define EEPROM_PASS_OFFSET   64
#define EEPROM_SSID_LEN      64
#define EEPROM_PASS_LEN      64

// ============================================================
class CaptivePortal {
public:
  CaptivePortal() : _server(80), _started(false) {}

  // ---- Haupt-Entry: blockiert bis Konfiguration gespeichert oder Timeout ----
  bool begin(const String& apName) {
    CP_DEBUG("=== CaptivePortal::begin() ===");
    CP_HEAP();

    _apName = apName;

    // ---- WiFi komplett zurücksetzen ----
    CP_DEBUG("WiFi disconnect + mode off");
    WiFi.persistent(false);
    WiFi.disconnect(true);
    // FIX: kurze busy-wait OHNE yield/delay um reentrant-panic zu vermeiden
    uint32_t t = millis();
    while (millis() - t < 100) { ESP.wdtFeed(); }

    CP_DEBUG("Setting WiFi mode AP");
    WiFi.mode(WIFI_AP);
    // FIX: nach mode()-Wechsel kurze Pause ohne delay()
    t = millis();
    while (millis() - t < 200) { ESP.wdtFeed(); }

    CP_DEBUGF("Starting SoftAP: %s", apName.c_str());
    bool apOk = WiFi.softAP(apName.c_str());
    CP_DEBUGF("softAP result: %s", apOk ? "OK" : "FAILED");
    CP_HEAP();

    if (!apOk) {
      CP_DEBUG("ERROR: softAP failed – restarting in 3s");
      _safeDelay(3000);
      ESP.restart();
      return false;
    }

    IPAddress apIP = WiFi.softAPIP();
    CP_DEBUGF("AP-IP: %s", apIP.toString().c_str());

    // ---- DNS-Server: alle Anfragen zur AP-IP umleiten ----
    CP_DEBUG("Starting DNS server");
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(DNS_PORT, "*", apIP);
    CP_DEBUG("DNS server started");
    CP_HEAP();

    // ---- Webserver einrichten ----
    _setupWebServer();

    CP_DEBUG("Starting HTTP server");
    _server.begin();
    CP_DEBUG("HTTP server started");
    CP_HEAP();

    _started   = true;
    _startTime = millis();

    CP_DEBUG("Entering CaptivePortal loop – waiting for config...");
    return _run();
  }

  bool isStarted() const { return _started; }

private:
  ESP8266WebServer _server;
  DNSServer        _dnsServer;
  String           _apName;
  bool             _started;
  uint32_t         _startTime;
  bool             _configSaved = false;

  // --------------------------------------------------------
  //  Haupt-Loop  –  KEINE delay() / yield() hier drin!
  //  FIX: ESP.wdtFeed() + optimistic_yield(10000) statt delay()
  // --------------------------------------------------------
  bool _run() {
    uint32_t lastHeapPrint = 0;
    uint32_t loopCount     = 0;

    while (!_configSaved) {
      // ---- Watchdog füttern (FIX: kein yield() → kein reentrant panic) ----
      ESP.wdtFeed();

      // ---- DNS + HTTP verarbeiten ----
      _dnsServer.processNextRequest();
      _server.handleClient();

      // ---- Kooperatives Yielden (sicher, da kein delay-Stack) ----
      optimistic_yield(10000);

      // ---- Timeout-Check ----
      if (millis() - _startTime > CP_TIMEOUT_MS) {
        CP_DEBUG("CaptivePortal TIMEOUT – restarting");
        CP_HEAP();
        _safeDelay(500);
        ESP.restart();
        return false;
      }

      // ---- Heap-Status alle 10 Sekunden ----
      if (millis() - lastHeapPrint > 10000) {
        lastHeapPrint = millis();
        loopCount++;
        CP_DEBUGF("Loop #%lu  Uptime: %lus  Clients: %d",
                  loopCount,
                  (millis() - _startTime) / 1000,
                  WiFi.softAPgetStationNum());
        CP_HEAP();
      }
    }

    CP_DEBUG("Config saved – leaving CaptivePortal loop");
    _server.stop();
    _dnsServer.stop();
    return true;
  }

  // --------------------------------------------------------
  //  Webserver-Routen
  // --------------------------------------------------------
  void _setupWebServer() {
    CP_DEBUG("Registering web routes");

    // Captive-Portal-Detect-Endpunkte (iOS, Android, Windows)
    _server.on(F("/generate_204"),     [this]() { _handleRoot(); });
    _server.on(F("/fwlink"),           [this]() { _handleRoot(); });
    _server.on(F("/connecttest.txt"),  [this]() { _handleRoot(); });
    _server.on(F("/hotspot-detect.html"), [this]() { _handleRoot(); });
    _server.on(F("/"),                 [this]() { _handleRoot(); });
    _server.on(F("/save"),             HTTP_POST, [this]() { _handleSave(); });
    _server.onNotFound(               [this]() { _handleRoot(); });

    CP_DEBUG("Web routes registered");
  }

  // ---- Root-Seite: Konfigurationsformular ----
  void _handleRoot() {
    CP_DEBUGF("HTTP request: %s %s",
              (_server.method() == HTTP_GET) ? "GET" : "POST",
              _server.uri().c_str());

    // HTML komplett aus PROGMEM um Heap zu schonen
    String html;
    html.reserve(1200);
    html  = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>LaCrosse Gateway Setup</title>"
              "<style>body{font-family:Arial,sans-serif;margin:20px;background:#f0f0f0;}"
              ".box{background:#fff;padding:20px;border-radius:8px;max-width:400px;margin:auto;}"
              "input{width:100%;padding:8px;margin:6px 0 14px;box-sizing:border-box;}"
              "button{background:#0078d4;color:#fff;border:none;padding:10px 20px;"
              "border-radius:4px;cursor:pointer;width:100%;font-size:1em;}"
              "</style></head><body><div class='box'>"
              "<h2>LaCrosse Gateway</h2>"
              "<p>Bitte WLAN-Zugangsdaten eingeben:</p>"
              "<form method='POST' action='/save'>"
              "<label>SSID</label>"
              "<input type='text' name='ssid' placeholder='WLAN-Name' required>"
              "<label>Passwort</label>"
              "<input type='password' name='pass' placeholder='Passwort'>"
              "<button type='submit'>Speichern &amp; Verbinden</button>"
              "</form></div></body></html>");

    _server.send(200, F("text/html"), html);
    CP_DEBUG("Root page sent");
    CP_HEAP();
  }

  // ---- Speichern-Handler ----
  void _handleSave() {
    CP_DEBUG("=== /save called ===");

    String ssid = _server.arg(F("ssid"));
    String pass = _server.arg(F("pass"));

    CP_DEBUGF("SSID received: '%s' (len=%d)", ssid.c_str(), ssid.length());
    CP_DEBUGF("Pass received:  len=%d", pass.length());

    if (ssid.length() == 0 || ssid.length() >= EEPROM_SSID_LEN) {
      CP_DEBUG("ERROR: SSID invalid length");
      _server.send(400, F("text/plain"), F("Ungültige SSID"));
      return;
    }

    // ---- In EEPROM schreiben ----
    CP_DEBUG("Writing credentials to EEPROM");
    _writeStringToEEPROM(EEPROM_SSID_OFFSET, EEPROM_SSID_LEN, ssid);
    _writeStringToEEPROM(EEPROM_PASS_OFFSET, EEPROM_PASS_LEN, pass);
    EEPROM.commit();
    CP_DEBUG("EEPROM committed");
    CP_HEAP();

    // ---- Erfolgsseite senden ----
    String resp = F("<!DOCTYPE html><html><body><div style='font-family:Arial;"
                    "margin:40px auto;max-width:400px;text-align:center'>"
                    "<h2>&#10003; Gespeichert!</h2>"
                    "<p>Gateway verbindet sich mit <b>");
    resp += ssid;
    resp += F("</b>.<br>Bitte warten...</p></div></body></html>");
    _server.send(200, F("text/html"), resp);

    // FIX: Kurze Pause ohne delay() bevor Neustart
    _safeDelay(1500);

    CP_DEBUG("Restarting ESP...");
    _configSaved = true;
    // Loop verlässt sich nach _configSaved==true, dann ESP.restart() im Hauptprogramm
  }

  // ---- EEPROM-Hilfsfunktion ----
  void _writeStringToEEPROM(int offset, int maxLen, const String& value) {
    int len = min((int)value.length(), maxLen - 1);
    for (int i = 0; i < len; i++) {
      EEPROM.write(offset + i, value[i]);
    }
    EEPROM.write(offset + len, 0); // Null-Terminator
    // Rest mit Nullen füllen
    for (int i = len + 1; i < maxLen; i++) {
      EEPROM.write(offset + i, 0);
    }
  }

  // ---- Sichere Verzögerung OHNE yield() (FIX für reentrant panic) ----
  static void _safeDelay(uint32_t ms) {
    uint32_t start = millis();
    while (millis() - start < ms) {
      ESP.wdtFeed();
      // optimistic_yield ist sicher, da es kein blocking-yield ist
      optimistic_yield(10000);
    }
  }
};

#endif // CAPTIVEPORTAL_H