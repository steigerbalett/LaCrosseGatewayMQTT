#pragma once
// =============================================================
//  CaptivePortal.h  –  Interface (original + Debug-Erweiterungen)
//  KEIN ESP8266WebServer.h Include hier – verhindert HTTP_GET-Konflikt
//  mit ESPAsyncWebServer in CaptivePortal.cpp
// =============================================================
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Settings.h"
#include "Logger.h"

// Forward-Deklaration statt #include <ESPAsyncWebServer.h> im Header
// → verhindert HTTP_GET/HTTP_POST enum-Konflikt mit ESP8266WebServer.h
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
  bool            m_restartPending;
  uint32_t        m_doneSince;
  String          m_scanHtml;

  String buildPage(const String &body);
  String buildScanHtml();
};
