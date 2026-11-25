#ifndef _EC3000TRACKER_h
#define _EC3000TRACKER_h

#include "Arduino.h"
#include "EC3000.h"

// Keeps the last values of every EC3000 energy monitor that was received,
// checks new frames for plausibility and limits the MQTT publish rate.
//
// A frame with a valid CRC can still be wrong. A frame is discarded if
//  - on seconds > total seconds
//  - state is off but power > 0
//  - power > MAX_POWER_W
//  - the device clock (total seconds) runs faster than the real time
//  - consumption goes backwards or rises more than MAX_POWER_W allows
//  - the reset counter goes backwards or jumps by more than MAX_RESET_STEP
// A manual reset of the device (all counters back to 0) is accepted.
// A new ID is only confirmed after a second matching frame, so a single
// broken frame does not create a ghost device.
class EC3000Tracker {
public:
  static const byte MAX_DEVICES = 16;
  static constexpr float MAX_POWER_W = 3700.0;      // 16 A at 230 V
  static const byte MAX_RESET_STEP = 5;
  static const byte REBASE_AFTER_DISCARDS = 3;      // consecutive discards until a new baseline is taken

  struct Device {
    bool Used;
    bool Confirmed;
    word ID;
    EC3000::Frame Last;
    int RSSI;
    byte Rfm;
    unsigned long LastSeen;
    unsigned long LastPublish;
    bool Published;
    bool Discovered;         // Home Assistant discovery sent on the current MQTT connection
    byte Discards;           // consecutive discarded frames
    unsigned long Accepted;  // accepted frames since boot
    unsigned long Rejected;  // discarded frames since boot
  };

  // Checks the frame and stores it if it is plausible.
  // Returns the device slot (never nullptr), accepted tells if the frame was taken,
  // reason (if not empty) describes why it was discarded.
  Device *Update(const EC3000::Frame &frame, int rssi, byte rfm, bool &accepted, String &reason);

  // true if the device is confirmed and was never published or the interval has passed
  bool ShouldPublish(Device *device, unsigned long intervalMs);
  void SetPublished(Device *device);
  // after a new MQTT connection the discovery has to be sent again
  void ResetDiscovery();

  // Names for IDs, format "BA3D=Waschmaschine;BF2C=Trockner".
  // With onlyKnown only listed IDs are tracked, all others are ignored.
  void SetNames(const String &names, bool onlyKnown);
  String GetName(word id);           // "" if the ID has no name
  bool IsAllowed(word id);
  static String CleanName(const String &name);

  byte GetCapacity() { return MAX_DEVICES; }
  Device *GetAt(byte index) { return index < MAX_DEVICES ? &m_devices[index] : nullptr; }
  Device *Find(word id);

  // Pure check of a new frame against the last accepted one (used by Update)
  static String CheckFrame(const EC3000::Frame &frame);
  static String CheckSequence(const EC3000::Frame &last, const EC3000::Frame &frame, unsigned long elapsedMs);

private:
  struct NameEntry {
    word ID;
    String Name;
  };
  static const byte MAX_NAMES = 24;
  NameEntry m_names[MAX_NAMES];
  byte m_nameCount = 0;
  bool m_onlyKnown = false;
  Device m_devices[MAX_DEVICES] = {};
  Device *Allocate(word id);
  void Store(Device *device, const EC3000::Frame &frame, int rssi, byte rfm);
};

#endif
