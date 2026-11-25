#include "StateManager.h"
#ifdef ESP32
#include "WiFi.h"
#else
#include "ESP8266WiFi.h"
#endif

StateManager::StateManager() {
  m_lastKVPUpdate = 0;
  m_lastFullKVPUpdate = 0;
  m_roolOverIsPossible = false;
  m_uptimeDays = 0;
  m_displayStatus = "none";
}


void StateManager::Begin(String version, String identity) {
  m_version = version;
  m_identity = identity;
}

void StateManager::SetLoopStart() {
  m_loopStartTime = micros();
}

void StateManager::SetLoopEnd() {
  m_loopCount++;
  uint32_t currentLoopTime = micros() - m_loopStartTime;
  m_loopTotalTime += currentLoopTime;

  if (currentLoopTime < m_loopMinTime) {
    m_loopMinTime = currentLoopTime;
  }
  if (currentLoopTime > m_loopMaxTime) {
    m_loopMaxTime = currentLoopTime;
  }

  if (millis() < m_loopMeasureStart) {
    m_loopMeasureStart = 0;
  }
  if (millis() > m_loopMeasureStart + 5000) {
    word loopAverage = m_loopTotalTime / m_loopCount;
    m_loopDurationMin = m_loopMinTime;
    m_loopDurationAvg = loopAverage;
    m_loopDurationMax = m_loopMaxTime;
    m_loopTotalTime = 0;
    m_loopCount = 0;
    m_loopMinTime = 64000;
    m_loopMaxTime = 0;
    m_loopMeasureStart = millis();
  }

}

void StateManager::SetDisplayStatus(String status) {
  m_displayStatus = status;
}

void StateManager::SetHostname(String hostname){
  m_hostname = hostname;
}

String StateManager::GetHostname(){
  return m_hostname;
}

unsigned int StateManager::GetFramesPerMinute() {
  return m_framesPerMinute;
}

void StateManager::Handle(byte frames){
  m_receivedFrames += frames;
  unsigned long currentMillis = millis();
  
  // Handle 50 Days rollover for UpTime
  if(currentMillis >= 3000000000) { 
    m_roolOverIsPossible = true;
  }
  if(currentMillis <= 100000 && m_roolOverIsPossible) {
    m_uptimeDays += 50;
    m_roolOverIsPossible = false;
  }
    
  // Calculate frames per minute (sum of the last 12 buckets of 5 seconds)
  if (currentMillis - m_fpmBucketStart >= FPM_BUCKETS * FPM_BUCKET_MS) {
    for (byte i = 0; i < FPM_BUCKETS; i++) {
      m_fpmBuckets[i] = 0;
    }
    m_fpmBucketStart = currentMillis;
  }
  while (currentMillis - m_fpmBucketStart >= FPM_BUCKET_MS) {
    m_fpmBucketStart += FPM_BUCKET_MS;
    m_fpmIndex = (m_fpmIndex + 1) % FPM_BUCKETS;
    m_fpmBuckets[m_fpmIndex] = 0;
  }
  if (m_fpmBuckets[m_fpmIndex] < 0xFFFF - frames) {
    m_fpmBuckets[m_fpmIndex] += frames;
  }
  m_framesPerMinute = 0;
  for (byte i = 0; i < FPM_BUCKETS; i++) {
    m_framesPerMinute += m_fpmBuckets[i];
  }
}

String StateManager::GetVersion() {
  return m_version;
}

String StateManager::GetUpTime() {
  return m_values.Get("UpTimeText","");
}

void StateManager::Update() {
  m_values.Clear();

  unsigned long upTimeSeconds = millis() / 1000;
  String upTimeText(m_uptimeDays + upTimeSeconds / 86400);
  upTimeText += F("Tg. ");
  upTimeText += String(upTimeSeconds / 3600 % 24);
  upTimeText += F("Std. ");
  upTimeText += String((upTimeSeconds / 60) % 60);
  upTimeText += F("Min. ");
  upTimeText += String(upTimeSeconds % 60);
  upTimeText += F("Sek. ");

  
  m_values.Put("UpTimeSeconds", String(upTimeSeconds));
  m_values.Put("UpTimeText", upTimeText);
  m_values.Put("WIFI", WiFi.SSID());
  m_values.Put("MacAddress", WiFi.macAddress());
  m_values.Put("ReceivedFrames", String(m_receivedFrames));
  m_values.Put("FramesPerMinute", String(m_framesPerMinute));
  m_values.Put("RSSI", WiFi.getMode() == WiFiMode_t::WIFI_OFF ? "Off" : String(WiFi.RSSI()));
  m_values.Put("FreeHeap", String(ESP.getFreeHeap()));
  m_values.Put("Version", GetVersion());
  m_values.Put("LD.Min", String((float)m_loopDurationMin / 1000.0));
  m_values.Put("LD.Avg", String((float)m_loopDurationAvg / 1000.0));
  m_values.Put("LD.Max", String((float)m_loopDurationMax / 1000.0));
  m_values.Put("OLED", m_displayStatus);

}

String StateManager::GetHTML() {
  Update();
  String result = "<style>table{border:1px solid black;border-collapse:collapse;}tr,td{padding:4px;border:1px solid; Margin:5px}</style>";
  
  result += F("<table>");
  for (uint i = 0; i < m_values.Size(); i++) {
    result += F("<tr><td>");
    result += m_values.GetKeyAt(i);
    result += F(": </td><td>");
    result += m_values.GetValueAt(i);
    result += F("</td></tr>");
  }

  return result;
}

String StateManager::GetXML() {
  Update();
  String result = "";

  result += F("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  result += F("<LGW>");
  for (uint i = 0; i < m_values.Size(); i++) {
    result += F("<Info Key = \"");
    result += m_values.GetKeyAt(i);
    result += F("\" Value=\"");
    result += m_values.GetValueAt(i);
    result += F("\"/>");
  }
  result += F("</LGW>");
  
  return result;
}

String StateManager::GetKVP(bool full) {
  Update();
  String result = "";

  result += F("OK VALUES LGW ");
  result += m_identity;
  result += " ";

  for (uint i = 0; i < m_values.Size(); i++) {
    String key = m_values.GetKeyAt(i);
    bool isConst = key.equals("MacAddress") || key.equals("ChipID") || key.equals("Version");

    if (full || !isConst) {
      if (i > 0) {
        result += ",";
      }
      result += key;
      result += "=";
      result += m_values.GetValueAt(i);
    }
  }

  return result;
}

String StateManager::GetKVP(uint32_t interval) {
  String result = "";
  
  if (millis() < m_lastKVPUpdate) {
    m_lastKVPUpdate = 0;
  }
  if (millis() > m_lastKVPUpdate + interval * 1000) {
    bool sendAllData = false;
    if (m_lastFullKVPUpdate == 0 || millis() > m_lastFullKVPUpdate + 1800000ul) {
      sendAllData = true;
      m_lastFullKVPUpdate = millis();
    }
    result = GetKVP(sendAllData);
    m_lastKVPUpdate = millis();
  }

  return result;
}

void StateManager::ResetLastFullKVPUpdate() {
  m_lastFullKVPUpdate = 0;
}

void StateManager::SetWiFiConnectTime(float connectTime) {
  m_wifiConnnectTime = connectTime;
}

float StateManager::GetWiFiConnectTime() {
  return m_wifiConnnectTime;
}
