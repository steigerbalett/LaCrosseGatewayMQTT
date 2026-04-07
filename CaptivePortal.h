#ifndef _CAPTIVEPORTAL_h
#define _CAPTIVEPORTAL_h

/*
 * CaptivePortal.h – ESPAsyncWebServer-Version
 * -----------------------------------------------
 * WARUM AsyncWebServer statt ESP8266WebServer?
 *
 * ESP8266WebServer + softAP: Der synchrone TCP-Listener ruft intern yield()
 * aus einem lwIP-Critical-Section (sys-Kontext, Interrupts gesperrt) auf.
 * Resultat: Panic core_esp8266_main.cpp:191 __yield – unvermeidbar, da der
 * Aufruf aus precompilierten WiFi-SDK-Binaries kommt (libnet80211.a, libpp.a).
 * Weder --wrap=__yield noch Symbol-Override können diese Calls abfangen.
 *
 * ESPAsyncWebServer löst das Problem: Es nutzt ESPAsyncTCP (lwIP async callbacks)
 * und ruft yield() intern NICHT auf. Callbacks feuern im Kontext des loop()-Tasks,
 * nicht im sys-Kontext.
 */

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include <ESPAsyncWebServer.h>
#include "Settings.h"
#include "Logger.h"

class CaptivePortal {
public:
  CaptivePortal();

  // Startet den AP und Webserver
  // apSSID:   SSID des temporären AP
  // timeoutS: Automatischer Timeout in Sekunden (0 = kein Timeout)
  void Begin(Settings *settings, Logger *logger,
             String apSSID = "", int timeoutS = 300);

  // Timeout-Check; bei AsyncWebServer kein handleClient() mehr nötig
  void Handle();

  // Gibt true zurück wenn die Einrichtung abgeschlossen wurde
  bool IsDone();

  // Beendet AP und Webserver
  void End();

private:
  Settings      *m_settings;
  Logger        *m_logger;
  AsyncWebServer m_server;  // ESPAsyncWebServer – kein yield() im sys-Kontext!
  bool           m_done;
  int            m_timeoutS;
  unsigned long  m_startMs;
  String         m_apSSID;

  String buildPage(const String &body);
  String scanNetworks();
};

#endif
