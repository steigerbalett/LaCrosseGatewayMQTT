#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Settings.h"
#include "Logger.h"

class AsyncWebServer;

class CaptivePortal {
public:
  CaptivePortal();
  ~CaptivePortal();

  void Begin(Settings *settings, Logger *logger,
             String apSSID = "", int timeoutS = 0);
  void Handle();
  bool IsDone();
  void End();

private:
  AsyncWebServer *m_server;
  Settings       *m_settings;
  Logger         *m_logger;
  String          m_apSSID;
  int             m_timeoutS;
  uint32_t        m_startMs;
  bool            m_done;
  bool            m_restartOnDone;
  uint32_t        m_doneSince;
  String          m_scanHtml;

  String buildPage(const String &body);
  String scanNetworks();
};
