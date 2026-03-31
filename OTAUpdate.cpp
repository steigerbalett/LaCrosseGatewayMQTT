#include "OTAUpdate.h"
#include "Settings.h"
#include "Logger.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

void OTAUpdate::SetDebugMode(boolean mode) {
  m_debug = mode;
}

String OTAUpdate::Start(Logger *logger) {
  String result = "";

  WiFiClient client;

  Settings s;
  s.Read(logger);
  String otaServer = s.Get("otaServer", "");
  uint otaPort = s.GetInt("otaPort", 0);
  String otaURL = s.Get("otaURL", "");

  t_httpUpdate_return updateResult = ESPhttpUpdate.update(client, otaServer, otaPort, otaURL);
  switch (updateResult) {
  case HTTP_UPDATE_FAILED:
    result += "FAILED";
    break;

  case HTTP_UPDATE_NO_UPDATES:
    result += "Was up to date";
    break;

  case HTTP_UPDATE_OK:
    result += "OK";
    break;
  }

  return result;
}

String OTAUpdate::_resolveGitHubAssetUrl(const String &owner,
                                          const String &repo,
                                          const String &assetName) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  const char *host = "api.github.com";
  if (!client.connect(host, 443)) {
    return "";
  }

  String path = "/repos/" + owner + "/" + repo + "/releases/latest";
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Accept: application/vnd.github+json\r\n" +
               "Connection: close\r\n\r\n");

  // HTTP-Header überspringen
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
    yield();
  }

  String body = "";
  unsigned long timeout = millis() + 8000;
  while (client.connected() && millis() < timeout) {
    while (client.available()) {
      body += (char)client.read();
      timeout = millis() + 8000;
    }
    yield();
  }
  client.stop();

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, body);
  if (err) return "";

  JsonArray assets = doc["assets"].as<JsonArray>();
  for (JsonObject asset : assets) {
    String name = asset["name"].as<String>();
    if (name == assetName) {
      return asset["browser_download_url"].as<String>();
    }
  }
  return "";
}

String OTAUpdate::StartFromGitHub(const String &owner,
                                   const String &repo,
                                   const String &assetName) {
  String result = "";

  String downloadUrl = _resolveGitHubAssetUrl(owner, repo, assetName);

  yield();
  delay(100);

  if (downloadUrl.isEmpty()) {
    return "FAILED: Asset nicht gefunden oder API-Fehler";
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  secureClient.setTimeout(30);

  ESPhttpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  ESPhttpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return updateResult =
      ESPhttpUpdate.update(secureClient, downloadUrl);

  switch (updateResult) {
    case HTTP_UPDATE_FAILED:
      result += "FAILED: " + ESPhttpUpdate.getLastErrorString();
      break;
    case HTTP_UPDATE_NO_UPDATES:
      result += "Was up to date";
      break;
    case HTTP_UPDATE_OK:
      result += "OK";
      break;
  }
  return result;
}
