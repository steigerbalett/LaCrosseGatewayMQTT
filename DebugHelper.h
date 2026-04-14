#ifndef DEBUGHELPER_H
#define DEBUGHELPER_H

// =============================================================
//  DebugHelper.h  –  Zentrale Debug-Ausgaben für LaCrosseGateway
//  Einbinden mit: #include "DebugHelper.h"
//  Optional aktivieren mit: DebugHelper::setEnabled(true);
// =============================================================

#include <Arduino.h>
#include <user_interface.h>   // für system_get_rst_info()

class DebugHelper {
public:
  inline static bool enabled = false;

  static void setEnabled(bool state) {
    enabled = state;
  }

  static bool isEnabled() {
    return enabled;
  }

  static void begin() {
    if (!enabled) return;

    Serial.println(F("\n\n========================================"));
    Serial.println(F("  LaCrosseGateway DEBUG BUILD"));
    Serial.println(F("========================================"));
    printResetReason();
    printSystemInfo();
  }

  // ---- Reset-Ursache ausgeben ----
  static void printResetReason() {
    if (!enabled) return;

    struct rst_info* ri = system_get_rst_info();
    Serial.print(F("[DBG] Reset reason code: "));
    Serial.println(ri->reason);
    Serial.print(F("[DBG] Reset reason text: "));
    Serial.println(ESP.getResetReason());
    Serial.print(F("[DBG] Reset info:        "));
    Serial.println(ESP.getResetInfo());
    Serial.flush();

    // Letzten Exception-Grund aus RTC-Speicher lesen
    uint32_t exceptionFlag = 0;
    ESP.rtcUserMemoryRead(0, &exceptionFlag, sizeof(exceptionFlag));
    if (exceptionFlag == 0xDEADBEEF) {
      Serial.println(F("[DBG] *** LETZTER BOOT WAR EIN CRASH! ***"));
      uint32_t crashStep = 0;
      ESP.rtcUserMemoryRead(1, &crashStep, sizeof(crashStep));
      Serial.printf("[DBG] Crash nach Schritt: %u\n", crashStep);
      Serial.flush();

      // Flag zurücksetzen
      exceptionFlag = 0;
      ESP.rtcUserMemoryWrite(0, &exceptionFlag, sizeof(exceptionFlag));
    }
  }

  // ---- System-Infos ----
  static void printSystemInfo() {
    if (!enabled) return;

    Serial.printf("[DBG] Free Heap:      %u bytes\n",  ESP.getFreeHeap());
    Serial.printf("[DBG] Free ContStack: %u bytes\n",  ESP.getFreeContStack());
    Serial.printf("[DBG] Flash size:     %u bytes\n",  ESP.getFlashChipRealSize());
    Serial.printf("[DBG] CPU freq:       %u MHz\n",    ESP.getCpuFreqMHz());
    Serial.printf("[DBG] Core version:   %s\n",        ESP.getCoreVersion().c_str());
    Serial.flush();
  }

  // ---- Schritt-Marker: in RTC-RAM speichern, überlebt einen Crash ----
  // Aufruf: DebugHelper::step(1); vor jedem kritischen Abschnitt
  static void step(uint32_t stepNumber, const char* description = nullptr) {
    // Crash-Flag setzen: falls danach ein Reset kommt, wissen wir wo
    uint32_t flag = 0xDEADBEEF;
    ESP.rtcUserMemoryWrite(0, &flag, sizeof(flag));
    ESP.rtcUserMemoryWrite(1, &stepNumber, sizeof(stepNumber));

    if (!enabled) return;

    Serial.printf("[DBG] >>> STEP %u", stepNumber);
    if (description) Serial.printf(": %s", description);
    Serial.printf("  Heap=%u  Stack=%u\n",
                  ESP.getFreeHeap(),
                  ESP.getFreeContStack());
    Serial.flush();
  }

  // ---- Schritt erfolgreich abgeschlossen ----
  static void stepDone(uint32_t stepNumber) {
    if (!enabled) return;

    Serial.printf("[DBG] <<< STEP %u done\n", stepNumber);
    Serial.flush();
  }

  // ---- Heap-Warnung ----
  static void checkHeap(uint32_t minFree = 8000) {
    if (!enabled) return;

    uint32_t free = ESP.getFreeHeap();
    if (free < minFree) {
      Serial.printf("[DBG] *** HEAP LOW: %u bytes! ***\n", free);
      Serial.flush();
    }
  }
};

//bool DebugHelper::enabled = false;

#endif // DEBUGHELPER_H