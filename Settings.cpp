#include "Settings.h"
#include "EEPROM.h"
#include "Logger.h"

bool Settings::m_debug = false;

Settings::Settings() {
}

void Settings::Read(Logger *logger) {
  m_data.Clear();

  EEPROM.begin(EEPROM_SIZE);
  String rawData;
  rawData.reserve(EEPROM_SIZE);
  int i;
  for (i = 0; i < EEPROM_SIZE; i++) {
      rawData += (char)EEPROM.read(i);
      if (i % 256 == 0) yield();
  }
  EEPROM.end();

  logger->print("Read bytes from EEPROM: ");
  logger->println(i);

  if (rawData[0] == 1) {
    String key = "";
    String value = "";
    bool keyDone = false;
    bool valueDone = false;
    for (uint j = 1; j < rawData.length(); j++) {
      if (!keyDone) {
        if (rawData[j] != 2) {
          key += (char)rawData[j];
        } else {
          keyDone = true;
          continue;
        }
      }

      if (keyDone && !valueDone) {
        if (rawData[j] != 1) {
          value += (char)rawData[j];
        } else {
          valueDone = true;
        }
      }

      if (keyDone && valueDone) {
        keyDone = false;
        valueDone = false;
        if (key.length() > 0) {
          m_data.Put(key, value);
        }
        key = "";
        value = "";
      }
    }
  }
}

String Settings::Write() {
  String result;
  String rawData;

  rawData += (char)1;
  for (uint i = 0; i < m_data.Size(); i++) {
    rawData += m_data.GetKeyAt(i);
    rawData += (char)2;
    rawData += m_data.GetValueAt(i);
    rawData += (char)1;
  }

  result += rawData.length();
  result += " Byte (max. ";
  result += EEPROM_SIZE;
  result += ") and ";
  result += m_data.Size();
  result += " values (max. ";
  result += m_data.GetCapacity();
  result += ")";

  EEPROM.begin(EEPROM_SIZE);
  int dataLen = (int)rawData.length();
  for (int i = 0; i < EEPROM_SIZE; i++) {
      EEPROM.write(i, i < dataLen ? (uint8_t)rawData[i] : 0);
      if (i % 256 == 0) yield();
  }
  EEPROM.commit();
  EEPROM.end();

  return result;
}

void Settings::Dump() {
  m_data.Dump();
}

String Settings::Get(String key, String defaultValue) {
  return m_data.Get(key, defaultValue);
}

int Settings::GetInt(String key, int defaultValue) {
  return atoi(Get(key, String(defaultValue)).c_str());
}

unsigned long Settings::GetUnsignedLong(String key, unsigned long defaultValue) {
  return strtoul(Get(key, String(defaultValue)).c_str(), NULL, DEC);
}

bool Settings::GetBool(String key) {
  return Get(key, "false").equals("true");
}

bool Settings::GetBool(String key, bool value) {
  return Get(key, value ? "true" : "false").equals("true");
}

void Settings::Add(String key, String value) {
  if (m_data.ContainsKey(key)) {
    m_data.Remove(key);
  }
  if (key.length() > 0 && m_data.Size() < CAPACITY) {
    m_data.Put(key, value);
  }
}

bool Settings::Change(String key, String value) {
  bool result = false;
  if (m_data.ContainsKey(key)) {
    m_data.Remove(key);
    m_data.Put(key, value);
    result = true;
  }
  return result;
}

void Settings::Remove(String key) {
  return m_data.Remove(key);
}

byte Settings::GetByte(String key, byte defaultValue) {
  String strVal = Get(key, (String)defaultValue);
  strVal.toLowerCase();
  if (strVal.startsWith("0x")) {
    return (byte)strtol(strVal.substring(2).c_str(), NULL, HEX);
  } else {
    return (byte)strtol(strVal.c_str(), NULL, DEC);
  }
}

String Settings::ToString() {
  String result = "SETUP ";
  for (uint i = 0; i < m_data.Size(); i++) {
    result += m_data.GetKeyAt(i);
    result += " ";
    result += m_data.GetValueAt(i);
    result += "; ";
  }
  return result;
}

bool Settings::FromString(String settings) {
  bool result = false;

  settings.trim();
  if (!settings.endsWith(";")) {
    settings += ";";
  }

  byte step = 0;
  String key = "";
  String value = "";
  for (uint i = 0; i < settings.length(); i++) {
    char cc = settings[i];
    if (step == 0) {
      if (cc == ' ' && key.length() > 0) {
        step = 1;
      } else {
        key += cc;
      }
    } else if (step == 1) {
      if (cc == ';') {
        step = 0;
        key.trim();
        value.trim();
        if (key.length() > 0 && value.length() > 0) {
          Add(key, value);
          key = "";
          value = "";
        }
      } else {
        value += cc;
      }
    }
  }

  return result;
}

void Settings::SaveRadioSettings(byte radioIndex, unsigned long freqKHz,
                                  String dataRate, byte toggleMask,
                                  uint16_t toggleInterval) {
  String p = "Radio" + String(radioIndex + 1);
  Add(p + "Freq",           String(freqKHz));
  Add(p + "DataRate",       dataRate);
  Add(p + "ToggleMask",     String(toggleMask));
  Add(p + "ToggleInterval", String(toggleInterval));
}

void Settings::LoadRadioSettingsFrom(byte radioIndex, unsigned long &freqKHz,
                                      String &dataRate, byte &toggleMask,
                                      uint16_t &toggleInterval) {
  String p = "Radio" + String(radioIndex + 1);
  freqKHz        = (unsigned long)GetInt(p + "Freq",           freqKHz);
  dataRate       = Get(p + "DataRate",                         dataRate);
  toggleMask     = (byte)GetInt(p + "ToggleMask",              toggleMask);
  toggleInterval = (uint16_t)GetInt(p + "ToggleInterval",      toggleInterval);
}
