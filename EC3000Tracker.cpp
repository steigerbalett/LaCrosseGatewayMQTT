#include "EC3000Tracker.h"

EC3000Tracker::Device *EC3000Tracker::Find(word id) {
  for (byte i = 0; i < MAX_DEVICES; i++) {
    if (m_devices[i].Used && m_devices[i].ID == id) {
      return &m_devices[i];
    }
  }
  return nullptr;
}

EC3000Tracker::Device *EC3000Tracker::Allocate(word id) {
  Device *slot = nullptr;
  for (byte i = 0; i < MAX_DEVICES; i++) {
    if (!m_devices[i].Used) {
      slot = &m_devices[i];
      break;
    }
    if (slot == nullptr || (millis() - m_devices[i].LastSeen) > (millis() - slot->LastSeen)) {
      slot = &m_devices[i];
    }
  }
  *slot = Device();
  slot->Used = true;
  slot->ID = id;
  return slot;
}

EC3000Tracker::Device *EC3000Tracker::Update(const EC3000::Frame &frame, int rssi, byte rfm) {
  Device *device = Find(frame.ID);
  if (device == nullptr) {
    device = Allocate(frame.ID);
  }
  device->Last = frame;
  device->RSSI = rssi;
  device->Rfm = rfm;
  device->LastSeen = millis();
  return device;
}

bool EC3000Tracker::ShouldPublish(Device *device, unsigned long intervalMs) {
  return !device->Published || (millis() - device->LastPublish) >= intervalMs;
}

void EC3000Tracker::SetPublished(Device *device) {
  device->Published = true;
  device->LastPublish = millis();
}
