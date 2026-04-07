#ifndef _CAPTIVEPORTAL_h
#define _CAPTIVEPORTAL_h

/*
 * CaptivePortal.h – ESPAsyncWebServer-Version
 *
 * WICHTIG: ESPAsyncWebServer.h wird NICHT hier inkludiert, sondern nur in
 * CaptivePortal.cpp. Grund: ESP8266WebServer.h (via LaCrosseGateway.ino) und
 * ESPAsyncWebServer.h definieren beide HTTP_GET/HTTP_POST/... → Enum-Konflikt.
 * Lösung: Forward-Declaration + Pointer-Member, vollständige Definition nur in .cpp
 */

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "Settings.h"
#include "Logger.h"

// Forward-Declaration – kein #include nötig für den Pointer-Member
class AsyncWebServer;

class CaptivePortal {
public:
  CaptivePortal();
  ~CaptivePortal();

  void Begin(Settings *settings, Logger *logger,
             String apSSID = "", int timeoutS = 300);

  // Timeout-Check aufrufen (kein handleClient() nötig bei AsyncWebServer)
  void Handle();

  bool IsDone();
  void End();

private:
  Settings       *m_settings;
  Logger         *m_logger;
  AsyncWebServer *m_server;   // Pointer: ESPAsyncWebServer.h nur in .cpp eingebunden
  bool            m_done;
  int             m_timeoutS;
  unsigned long   m_startMs;
  String          m_apSSID;

  String buildPage(const String &body);
  String scanNetworks();
};

#endif
