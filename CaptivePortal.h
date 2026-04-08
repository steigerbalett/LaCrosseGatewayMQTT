#pragma once
// ============================================================
//  CaptivePortal.h  –  v4  (ESP8266WebServer statt AsyncWebServer)
//
//  WARUM der Wechsel:
//    ESPAsyncWebServer + ESPAsyncTCP verbrauchen ~4-6 KB BSS/IRAM extra.
//    Das Projekt nutzt ESP8266WebServer bereits überall (WebFrontend, OTA,
//    SerialBridge, Nextion, SubProcessor) – einheitliche Basis, kein
//    HTTP_GET Enum-Konflikt, deutlich weniger Speicher.
// ============================================================
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "Settings.h"
#include "Logger.h"

class CaptivePortal {
public:
  CaptivePortal();
  ~CaptivePortal();

  void Begin(Settings *settings, Logger *logger,
             String apSSID = "", int timeoutS = 0);
  void Handle();          // Muss im Loop aufgerufen werden (handleClient + DNS)
  bool IsDone();
  void End();

private:
  // Heap-allokiert in Begin() → kein globaler Konstruktor vor setup()
  ESP8266WebServer *m_server;
  DNSServer        *m_dns;

  Settings         *m_settings;
  Logger           *m_logger;
  String            m_apSSID;
  int               m_timeoutS;
  uint32_t          m_startMs;
  bool              m_done;
  bool              m_restartPending;
  uint32_t          m_doneSince;
  bool              m_dnsActive;
  String            m_scanHtml;

  String buildPage(const String &body);
  String buildScanHtml();
  void   registerRoutes();
};
