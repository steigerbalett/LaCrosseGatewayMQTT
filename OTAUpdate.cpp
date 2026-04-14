#include "OTAUpdate.h"
#include "Settings.h"
#include "Logger.h"
#include <ESP8266WiFi.h>

void OTAUpdate::SetDebugMode(boolean mode) {
  m_debug = mode;
}

String OTAUpdate::Start(Logger *logger) {
  String result = "";
  WiFiClient client;

  Settings s;
  s.Read(logger);
  String otaServer = s.Get("otaServer", "");
  uint   otaPort   = s.GetInt("otaPort", 0);
  String otaURL    = s.Get("otaURL", "");

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

  ESPhttpUpdate.rebootOnUpdate(false);

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
      delay(500);
      ESP.restart();
      break;
  }
  return result;
}