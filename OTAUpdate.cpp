#include "OTAUpdate.h"
#include "Settings.h"
#include "Logger.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

void OTAUpdate::SetDebugMode(boolean mode) {
  m_debug = mode;
}

// ─── Callbacks bei ESPhttpUpdate registrieren ──────────────────────────────
void OTAUpdate::_registerCallbacks() {
  // Referenz auf this für Lambdas
  OTAProgressCallback *pcb = &_progressCb;
  OTAStatusCallback   *scb = &_statusCb;

  ESPhttpUpdate.onStart([scb]() {
    Serial.println(F("[OTA] Update gestartet..."));
    if (*scb) (*scb)("Update gestartet...");
  });

  ESPhttpUpdate.onProgress([pcb](int cur, int total) {
    if (total > 0) {
      int pct = (cur * 100) / total;
      // Seriellen Fortschrittsbalken zeichnen (50 Zeichen breit)
      int filled = pct / 2;
      Serial.print(F("\r["));
      for (int i = 0; i < 50; i++) Serial.print(i < filled ? '=' : ' ');
      Serial.print(F("] "));
      Serial.print(pct);
      Serial.print(F("% ("));
      Serial.print(cur);
      Serial.print(F("/"));
      Serial.print(total);
      Serial.print(F(" bytes)"));
      // Display/OLED-Callback
      if (*pcb) (*pcb)(pct);
    }
  });

  ESPhttpUpdate.onEnd([scb]() {
    Serial.println(F("\n[OTA] Update abgeschlossen!"));
    if (*scb) (*scb)("Update abgeschlossen!");
  });

  ESPhttpUpdate.onError([scb](int err) {
    String msg = "[OTA] Fehler: " + ESPhttpUpdate.getLastErrorString();
    Serial.println(msg);
    if (*scb) (*scb)(msg);
  });
}

// ─── Klassisches BIN-Update (lokaler Server aus Settings) ─────────────────
String OTAUpdate::Start(Logger *logger) {
  String result = "";

  WiFiClient client;

  Settings s;
  s.Read(logger);
  String otaServer = s.Get("otaServer", "");
  uint   otaPort   = s.GetInt("otaPort", 0);
  String otaURL    = s.Get("otaURL", "");

  // Callbacks registrieren (auch für lokales BIN-Update)
  // Da Start() static ist, können wir nur Serial nutzen.
  ESPhttpUpdate.onStart([]() {
    Serial.println(F("[OTA] BIN-Update gestartet..."));
  });
  ESPhttpUpdate.onProgress([](int cur, int total) {
    if (total > 0) {
      int pct    = (cur * 100) / total;
      int filled = pct / 2;
      Serial.print(F("\r["));
      for (int i = 0; i < 50; i++) Serial.print(i < filled ? '=' : ' ');
      Serial.print(F("] "));
      Serial.print(pct);
      Serial.print(F("%"));
    }
  });
  ESPhttpUpdate.onEnd([]() {
    Serial.println(F("\n[OTA] BIN-Update fertig."));
  });
  ESPhttpUpdate.onError([](int err) {
    Serial.print(F("[OTA] BIN-Fehler: "));
    Serial.println(ESPhttpUpdate.getLastErrorString());
  });

  ESPhttpUpdate.rebootOnUpdate(false);  // FIX: Fehlerbehandlung im Sketch

  t_httpUpdate_return updateResult =
      ESPhttpUpdate.update(client, otaServer, otaPort, otaURL);

  switch (updateResult) {
    case HTTP_UPDATE_FAILED:
      result = "FAILED: " + ESPhttpUpdate.getLastErrorString();
      break;
    case HTTP_UPDATE_NO_UPDATES:
      result = "Was up to date";
      break;
    case HTTP_UPDATE_OK:
      result = "OK";
      Serial.println(F("[OTA] Neustart..."));
      delay(500);
      ESP.restart();  // Manuell neustarten nach erfolgreichem Update
      break;
  }
  return result;
}

// ─── GitHub Release → URL auflösen ─────────────────────────────────────────
String OTAUpdate::_resolveGitHubAssetUrl(const String &owner,
                                          const String &repo,
                                          const String &assetName) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);  // 15 Sekunden

  const char *host = "api.github.com";
  if (!client.connect(host, 443)) {
    Serial.println(F("[OTA] Verbindung zu api.github.com fehlgeschlagen"));
    return "";
  }

  String path = "/repos/" + owner + "/" + repo + "/releases/latest";
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Accept: application/vnd.github+json\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
    yield();
  }

  String body = "";
  unsigned long timeout = millis() + 10000;
  while (client.connected() && millis() < timeout) {
    while (client.available()) {
      body += (char)client.read();
      timeout = millis() + 10000;
    }
    yield();
  }
  client.stop();

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.print(F("[OTA] JSON-Fehler: "));
    Serial.println(err.c_str());
    return "";
  }

  JsonArray assets = doc["assets"].as<JsonArray>();
  for (JsonObject asset : assets) {
    String name = asset["name"].as<String>();
    if (name == assetName) {
      return asset["browser_download_url"].as<String>();
    }
  }
  Serial.println(F("[OTA] Asset nicht in Release gefunden"));
  return "";
}

// ─── GitHub-Redirect auflösen (302 → direkter Download-Link) ──────────────
// GitHub leitet browser_download_url auf objects.githubusercontent.com um.
// WiFiClientSecure kann diesen HTTPS→HTTPS cross-host Redirect oft nicht
// automatisch folgen. Wir lösen ihn manuell auf.
String OTAUpdate::_followRedirect(const String &url) {
  // URL parsen: Host & Pfad trennen
  String urlCopy = url;
  if (urlCopy.startsWith("https://")) urlCopy = urlCopy.substring(8);
  int slashPos = urlCopy.indexOf('/');
  if (slashPos < 0) return url;

  String host = urlCopy.substring(0, slashPos);
  String path = urlCopy.substring(slashPos);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);

  if (!client.connect(host.c_str(), 443)) {
    return url;  // Fallback: original URL zurückgeben
  }

  client.print(String("HEAD ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  String location = "";
  unsigned long t = millis() + 5000;
  while (client.connected() && millis() < t) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.startsWith("Location: ") || line.startsWith("location: ")) {
        location = line.substring(10);
      }
      if (line.length() == 0) break;
    }
    yield();
  }
  client.stop();

  return location.isEmpty() ? url : location;
}

// ─── GitHub OTA-Update mit Fortschrittsbalken ──────────────────────────────
String OTAUpdate::StartFromGitHub(const String &owner,
                                   const String &repo,
                                   const String &assetName) {
  if (_statusCb) _statusCb("Suche Update...");
  Serial.println(F("[OTA] Suche GitHub Release..."));

  String downloadUrl = _resolveGitHubAssetUrl(owner, repo, assetName);
  yield();
  delay(200);

  if (downloadUrl.isEmpty()) {
    return "FAILED: Asset nicht gefunden oder API-Fehler";
  }

  // Redirect auflösen (GitHub → objects.githubusercontent.com)
  if (_statusCb) _statusCb("Lade Download-URL...");
  String directUrl = _followRedirect(downloadUrl);
  Serial.print(F("[OTA] Download-URL: "));
  Serial.println(directUrl);
  yield();
  delay(200);

  // Callbacks registrieren
  _registerCallbacks();

  // FIX: rebootOnUpdate(false) damit wir den Fehler sauber auswerten können
  ESPhttpUpdate.rebootOnUpdate(false);
  ESPhttpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  secureClient.setTimeout(60);  // FIX: 60 Sekunden für große Binaries

  t_httpUpdate_return updateResult =
      ESPhttpUpdate.update(secureClient, directUrl);

  String result = "";
  switch (updateResult) {
    case HTTP_UPDATE_FAILED:
      result = "FAILED: " + ESPhttpUpdate.getLastErrorString();
      if (_statusCb) _statusCb(result);
      break;
    case HTTP_UPDATE_NO_UPDATES:
      result = "Was up to date";
      if (_statusCb) _statusCb("Bereits aktuell");
      break;
    case HTTP_UPDATE_OK:
      result = "OK";
      if (_statusCb) _statusCb("Update OK – Neustart...");
      Serial.println(F("[OTA] Neustart in 1 Sekunde..."));
      delay(1000);
      ESP.restart();  // Manueller Neustart nach erfolgreichem Update
      break;
  }
  return result;
}