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
  // free slot first, then an unconfirmed one, then the one not seen for the longest time
  Device *slot = nullptr;
  unsigned long now = millis();
  for (byte i = 0; i < MAX_DEVICES; i++) {
    Device *d = &m_devices[i];
    if (!d->Used) {
      slot = d;
      break;
    }
    if (slot == nullptr
        || (!d->Confirmed && slot->Confirmed)
        || (d->Confirmed == slot->Confirmed && (now - d->LastSeen) > (now - slot->LastSeen))) {
      slot = d;
    }
  }
  *slot = Device();
  slot->Used = true;
  slot->ID = id;
  return slot;
}

void EC3000Tracker::Store(Device *device, const EC3000::Frame &frame, int rssi, byte rfm) {
  device->Last = frame;
  device->RSSI = rssi;
  device->Rfm = rfm;
  device->LastSeen = millis();
  device->Discards = 0;
  device->Accepted++;

  if (device->HistoryCount == 0 || millis() - device->LastHistory >= HISTORY_STEP_MS) {
    float power = frame.Power < 0 ? 0 : frame.Power;
    device->History[(device->HistoryHead + device->HistoryCount) % HISTORY_LEN] = (word)(power * 10 + 0.5);
    if (device->HistoryCount < HISTORY_LEN) {
      device->HistoryCount++;
    }
    else {
      device->HistoryHead = (device->HistoryHead + 1) % HISTORY_LEN;
    }
    device->LastHistory = millis();
  }
}

byte EC3000Tracker::GetHistory(Device *device, float *values) {
  for (byte i = 0; i < device->HistoryCount; i++) {
    values[i] = device->History[(device->HistoryHead + i) % HISTORY_LEN] / 10.0;
  }
  return device->HistoryCount;
}

String EC3000Tracker::CheckFrame(const EC3000::Frame &frame) {
  String reason;
  if (frame.OnSeconds > frame.TotalSeconds) {
    reason += F("OnSeconds > TotalSeconds; ");
  }
  if (!frame.IsOn && frame.Power > 0) {
    reason += F("off but power > 0; ");
  }
  if (frame.Power > MAX_POWER_W) {
    reason += F("power too high; ");
  }
  return reason;
}

String EC3000Tracker::CheckSequence(const EC3000::Frame &last, const EC3000::Frame &frame, unsigned long elapsedMs) {
  String reason;
  // allowed rise of the counters: real time since the last accepted frame plus some slack
  float elapsedS = elapsedMs / 1000.0 + 60.0;

  if (frame.TotalSeconds < last.TotalSeconds) {
    // manual reset of the device: everything starts at 0 again
    float maxKwh = MAX_POWER_W * frame.TotalSeconds / 3600000.0 + 0.01;
    if (frame.TotalSeconds > elapsedS || frame.Consumption > maxKwh) {
      reason += F("total seconds went back; ");
    }
    return reason;
  }

  unsigned long deviceS = frame.TotalSeconds - last.TotalSeconds;
  if (deviceS > elapsedS) {
    reason += F("total seconds jumped; ");
  }

  double delta = frame.Consumption - last.Consumption;
  double maxDelta = MAX_POWER_W * elapsedS / 3600000.0 + 0.002;
  if (delta < -0.0005) {
    reason += F("consumption went back; ");
  }
  else if (delta > maxDelta) {
    reason += F("consumption jumped; ");
  }

  if (frame.NumberOfResets < last.NumberOfResets) {
    reason += F("resets went back; ");
  }
  else if (frame.NumberOfResets > last.NumberOfResets + MAX_RESET_STEP) {
    reason += F("resets jumped; ");
  }
  return reason;
}

EC3000Tracker::Device *EC3000Tracker::Update(const EC3000::Frame &frame, int rssi, byte rfm, bool &accepted, String &reason) {
  accepted = false;
  Device *device = Find(frame.ID);
  bool isNew = device == nullptr;
  if (isNew) {
    device = Allocate(frame.ID);
  }

  reason = CheckFrame(frame);
  if (reason.length() == 0 && !isNew) {
    reason = CheckSequence(device->Last, frame, millis() - device->LastSeen);
  }

  if (reason.length() == 0) {
    if (!isNew) {
      device->Confirmed = true;
    }
    Store(device, frame, rssi, rfm);
    accepted = true;
    return device;
  }

  device->Rejected++;
  if (isNew) {
    // nothing to compare with: forget the slot again
    device->Used = false;
    return device;
  }

  device->Discards++;
  if (device->Discards >= REBASE_AFTER_DISCARDS && CheckFrame(frame).length() == 0) {
    // the stored values were probably wrong: start again with this frame (unconfirmed)
    device->Confirmed = false;
    Store(device, frame, rssi, rfm);
    reason += F("new baseline; ");
  }
  return device;
}

bool EC3000Tracker::ShouldPublish(Device *device, unsigned long intervalMs) {
  return device->Confirmed && (!device->Published || (millis() - device->LastPublish) >= intervalMs);
}

void EC3000Tracker::SetPublished(Device *device) {
  device->Published = true;
  device->LastPublish = millis();
}

void EC3000Tracker::ResetDiscovery() {
  for (byte i = 0; i < MAX_DEVICES; i++) {
    m_devices[i].Discovered = false;
  }
}

// Removes characters that would break JSON, HTML attributes or the name list
String EC3000Tracker::CleanName(const String &name) {
  String result;
  for (unsigned int i = 0; i < name.length(); i++) {
    char c = name[i];
    if (c == '"' || c == '\\' || c == '\'' || c == '<' || c == '>' || c == ';' || c == '=' || c == '&' || (byte)c < 32) {
      continue;
    }
    result += c;
  }
  result.trim();
  return result;
}

void EC3000Tracker::SetNames(const String &names, bool onlyKnown) {
  m_nameCount = 0;
  int start = 0;
  while (start < (int)names.length() && m_nameCount < MAX_NAMES) {
    int end = names.indexOf(';', start);
    if (end < 0) {
      end = names.length();
    }
    String entry = names.substring(start, end);
    start = end + 1;

    int eq = entry.indexOf('=');
    String idStr = eq < 0 ? entry : entry.substring(0, eq);
    idStr.trim();
    if (idStr.length() == 0 || idStr.length() > 4) {
      continue;
    }
    char *rest;
    unsigned long id = strtoul(idStr.c_str(), &rest, 16);
    if (*rest != '\0' || id == 0 || id > 0xFFFF) {
      continue;
    }
    m_names[m_nameCount].ID = (word)id;
    m_names[m_nameCount].Name = eq < 0 ? String() : CleanName(entry.substring(eq + 1));
    m_nameCount++;
  }
  m_onlyKnown = onlyKnown && m_nameCount > 0;
}

String EC3000Tracker::GetName(word id) {
  for (byte i = 0; i < m_nameCount; i++) {
    if (m_names[i].ID == id) {
      return m_names[i].Name;
    }
  }
  return String();
}

bool EC3000Tracker::IsAllowed(word id) {
  if (!m_onlyKnown) {
    return true;
  }
  for (byte i = 0; i < m_nameCount; i++) {
    if (m_names[i].ID == id) {
      return true;
    }
  }
  return false;
}
