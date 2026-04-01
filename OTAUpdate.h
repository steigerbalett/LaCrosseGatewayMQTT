#ifndef _OTAUPDATE_h
#define _OTAUPDATE_h

#pragma once
#include <Arduino.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <functional>

class Logger;

using OTAProgressCallback = std::function<void(int percent)>;
using OTAStatusCallback   = std::function<void(const String &msg)>;

class OTAUpdate {
public:
  void SetDebugMode(boolean mode);
  void SetProgressCallback(OTAProgressCallback cb) { _progressCb = cb; }
  void SetStatusCallback(OTAStatusCallback cb)     { _statusCb   = cb; }

  static String Start(Logger *logger);
  String StartFromGitHub(const String &owner,
                         const String &repo,
                         const String &assetName);

private:
  boolean m_debug = false;
  OTAProgressCallback _progressCb = nullptr;
  OTAStatusCallback   _statusCb   = nullptr;

  void _registerCallbacks();
  String _resolveGitHubAssetUrl(const String &owner,
                                const String &repo,
                                const String &assetName);
  String _followRedirect(const String &url);
};

#endif