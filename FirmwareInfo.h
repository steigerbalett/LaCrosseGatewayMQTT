#ifndef _FIRMWAREINFO_h
#define _FIRMWAREINFO_h

#include "Arduino.h"
#include "Settings.h"

// Remembers which firmware was flashed last (settings fwVersion, fwPrevVersion, fwFile, fwTime, fwPending)
class FirmwareInfo {
public:
  // at boot: notices a new version, also one flashed over USB/serial
  static void CheckAtBoot(Settings &settings, const char *version);
  // after a successful update, before the restart: source = file name or URL
  static void RecordFlash(const String &source);
  // table rows for the update pages
  static String BuildRows(Settings &settings);
  static String BuildRows();
};

#endif
