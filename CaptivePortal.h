#ifndef _CAPTIVEPORTAL_h
#define _CAPTIVEPORTAL_h

/*
 * CaptivePortal.h
 * ---------------
 * Stellt beim ersten Start (oder wenn keine WiFi-Credentials gespeichert sind)
 * einen Access Point mit Captive Portal bereit.
 *
 * Features:
 *  - WiFi-Scan: Zeigt alle erreichbaren SSIDs mit Signalstärke + Sicherheit
 *  - Formular zur SSID/Passwort-Eingabe
 *  - Speichert Credentials in den Settings (EEPROM)
 *  - Leitet nach erfolgreicher Verbindung weiter
 *
 * Verwendung:
 *   CaptivePortal portal;
 *   portal.Begin(&settings, &logger);    // startet AP + Webserver
 *   while (!portal.IsDone()) {
 *     portal.Handle();                   // in loop() aufrufen
 *   }
 *   portal.End();                        // AP beenden
 */

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "Settings.h"
#include "Logger.h"

class CaptivePortal {
public:
  CaptivePortal();

  // Startet den AP und Webserver
  // apSSID:   SSID des temporären AP (default: "LaCrosseGW-<ChipID>")
  // timeoutS: Automatischer Timeout in Sekunden (0 = kein Timeout)
  void Begin(Settings *settings, Logger *logger,
             String apSSID = "", int timeoutS = 300);

  // Muss in loop() aufgerufen werden
  void Handle();

  // Gibt true zurück wenn die Einrichtung abgeschlossen wurde
  bool IsDone();

  // Beendet AP und Webserver
  void End();

private:
  Settings        *m_settings;
  Logger          *m_logger;
  ESP8266WebServer m_server;
  bool             m_done;
  int              m_timeoutS;
  unsigned long    m_startMs;
  String           m_apSSID;

  void handleRoot();
  void handleScan();
  void handleSave();
  void handleNotFound();

  String buildPage(const String &body);
  String scanNetworks();
};

#endif
