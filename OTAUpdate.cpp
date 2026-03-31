#include "OTAUpdate.h"
#include "Settings.h"
#include "Logger.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// define firmware HTTPSRV firmware FHEM/firmware firmware
// OTA-Server: 192.168.11.11
// OTA-Port:   8083
// OTA-url:    /fhem/firmware/LaCrosseGateway.bin

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

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String body = "";
  while (client.available()) {
    body += client.readString();
  }
  client.stop();

  DynamicJsonDocument doc(16384);
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

String OTAUpdate::StartFromGitHub(Logger *logger,
                                   const String &owner,
                                   const String &repo,
                                   const String &assetName) {
  String result = "";

  if (m_debug && logger) {
    logger->Log("OTA-GitHub: Suche Asset " + assetName +
                " in " + owner + "/" + repo);
  }

  String downloadUrl = _resolveGitHubAssetUrl(owner, repo, assetName);
  if (downloadUrl.isEmpty()) {
    return "FAILED: Asset nicht gefunden oder API-Fehler";
  }

  if (m_debug && logger) {
    logger->Log("OTA-GitHub: URL=" + downloadUrl);
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  ESPhttpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
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