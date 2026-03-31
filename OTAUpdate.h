
#ifndef _OTAUPDATE_h
#define _OTAUPDATE_h

#pragma once
#include <Arduino.h>
#include <ESP8266HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "Logger.h"

class Logger;

class OTAUpdate {
public:
  void SetDebugMode(boolean mode);
  String Start(Logger *logger);
  String StartFromGitHub(Logger *logger,
                         const String &owner,
                         const String &repo,
                         const String &assetName);

private:
  boolean m_debug = false;
  String  _resolveGitHubAssetUrl(const String &owner,
                                  const String &repo,
                                  const String &assetName);
};

#endif