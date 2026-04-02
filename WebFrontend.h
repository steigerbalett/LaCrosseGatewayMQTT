#ifndef _WEBFRONTEND_h
#define _WEBFRONTEND_h

#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "StateManager.h"
#include "Logger.h"
#include "Settings.h"
#include <functional>

typedef std::function<void(String)>   WFCommandCallbackType;
typedef std::function<String(void)>   WFHardwareCallbackType;

class WebFrontend {
public:
  WebFrontend(int port);

  void Begin(StateManager *stateManager, Logger *logger);
  void Handle();

  ESP8266WebServer *WebServer();
  void SetCommandCallback(WFCommandCallbackType callback);
  void SetHardwareCallback(WFHardwareCallbackType callback);
  void SetPassword(String password);

private:
  ESP8266WebServer    m_webserver;
  int                 m_port;
  String              m_password;
  StateManager       *m_stateManager = nullptr;
  Logger             *m_logger       = nullptr;
  WFCommandCallbackType  m_commandCallback  = nullptr;
  WFHardwareCallbackType m_hardwareCallback = nullptr;

  volatile int    m_otaPct  = 0;
  volatile bool   m_otaDone = false;
  String          m_otaMsg  = "";

  bool   IsAuthentified();
  String GetTop();
  String GetNavigation();
  String GetBottom();
  String GetDisplayName();
  String GetRedirectToRoot(String message = "");
  String BuildHardwareRow(String text1, String text2, String text3);
  String BuildRadioCard(Settings &settings, byte radioNbr);
  String _resolveDirectURL(const String &url);
  String SaveSelectedKeys(const char** keys, byte count, bool reboot);
};

#endif
