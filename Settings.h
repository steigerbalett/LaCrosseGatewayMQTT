#ifndef _SETTINGS_h
#define _SETTINGS_h

#include "Arduino.h"
#include "HashMap.h"
#include "Logger.h"

#define EEPROM_SIZE 1024
#define CAPACITY    50

class Settings {
public:
  Settings();

  void Read(Logger *logger);
  String Write();
  void Dump();

  String Get(String key, String defaultValue);
  int GetInt(String key, int defaultValue);
  unsigned long GetUnsignedLong(String key, unsigned long defaultValue);
  bool GetBool (String key);
  bool GetBool (String key, bool value);
  void Add(String key, String value);
  void Remove(String key);
  byte GetByte(String key, byte defaultValue);
  String ToString();
  bool FromString(String settings);
  bool Change(String key, String value);

  // Radio-spezifische Helpers
  void SaveRadioSettings(byte radioIndex, unsigned long freqKHz, String dataRate,
                         byte toggleMask, uint16_t toggleInterval);
  void LoadRadioSettingsFrom(byte radioIndex, unsigned long &freqKHz, String &dataRate,
                              byte &toggleMask, uint16_t &toggleInterval);

private:
  HashMap<String, String, CAPACITY> m_data;
  static bool m_debug;
};

#endif
