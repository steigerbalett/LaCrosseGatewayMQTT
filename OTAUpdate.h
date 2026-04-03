#ifndef _OTAUPDATE_h
#define _OTAUPDATE_h

#pragma once
#include <Arduino.h>
#include <ESP8266httpUpdate.h>

class Logger;

class OTAUpdate {
public:
  void SetDebugMode(boolean mode);
  static String Start(Logger *logger);

private:
  boolean m_debug = false;
};

#endif