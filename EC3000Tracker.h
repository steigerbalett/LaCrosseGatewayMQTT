#ifndef _EC3000TRACKER_h
#define _EC3000TRACKER_h

#include "Arduino.h"
#include "EC3000.h"

// Keeps the last values of every EC3000 energy monitor that was received,
// so they can be published to MQTT with a limited rate.
class EC3000Tracker {
public:
  static const byte MAX_DEVICES = 16;

  struct Device {
    bool Used;
    word ID;
    EC3000::Frame Last;
    int RSSI;
    byte Rfm;
    unsigned long LastSeen;
    unsigned long LastPublish;
    bool Published;
  };

  // Stores the frame and returns its device slot.
  // If the table is full the device that was not seen for the longest time is replaced.
  Device *Update(const EC3000::Frame &frame, int rssi, byte rfm);

  // true if the device was never published or the interval has passed
  bool ShouldPublish(Device *device, unsigned long intervalMs);
  void SetPublished(Device *device);

  byte GetCapacity() { return MAX_DEVICES; }
  Device *GetAt(byte index) { return index < MAX_DEVICES ? &m_devices[index] : nullptr; }
  Device *Find(word id);

private:
  Device m_devices[MAX_DEVICES] = {};
  Device *Allocate(word id);
};

#endif
