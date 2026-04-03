# LaCrosseGatewayMQTT – Änderungen (dev-branch Patch)

## Übersicht der Änderungen

### 1. `Settings.h` / `Settings.cpp` – Speicherlogik verbessert

**Probleme im Original:**
- `EEPROM_SIZE` war 1024 Byte – zu klein bei vielen Settings-Einträgen
- Kein typsicherer Zugriff auf Radio-Parameter
- Debug-Ausgabe schrieb rohen EEPROM-Inhalt (Binärzeichen) auf die serielle Schnittstelle

**Fixes & Erweiterungen:**
- `EEPROM_SIZE` auf **2048** erhöht (ESP8266 unterstützt bis 4096)
- `CAPACITY` (HashMap-Größe) auf **80** Einträge erhöht
- Debug-Ausgabe: nur noch Byte-Anzahl loggen, kein Binärmüll auf Serial
- **Neu:** `SaveRadioSettings(byte radioIndex, ...)` – speichert Freq, DataRate, ToggleMask, ToggleInterval für ein bestimmtes Radio direkt
- **Neu:** `LoadRadioSettingsFrom(byte radioIndex, ...)` – liest Radio-Parameter zurück (mit Fallback auf übergebene Defaults)

**Key-Schema im EEPROM (Beispiel Radio 1):**
```
Radio1Freq           → 868310
Radio1DataRate       → 17.241
Radio1ToggleMask     → 3
Radio1ToggleInterval → 30
```

---

### 2. `CaptivePortal.h` / `CaptivePortal.cpp` – **NEU**

Vollständiges Captive Portal für die erste WLAN-Einrichtung.

**Features:**
- Startet beim ersten Boot (oder wenn `WifiSSID` im EEPROM leer) einen temporären AP
- SSID des AP: `LaCrosseGW-<ChipID>` (eindeutig pro Gerät)
- **WiFi-Scan** mit Signalstärkeanzeige (dBm + Balken) und Schloss-Icon für gesicherte Netze
- Klick auf ein Netzwerk → übernimmt SSID automatisch in das Formular
- Speichert `WifiSSID` und `WifiPassword` in Settings (EEPROM)
- Verbindungstest direkt nach dem Speichern (12 s Timeout)
- Bei Erfolg: zeigt IP-Adresse an und setzt `m_done = true` → Hauptprogramm kann weiter starten
- Bei Fehler: Fehlermeldung, zurück zum Formular
- Automatischer Timeout (Standard: 300 s), danach ebenfalls `IsDone() == true`
- Catch-All-DNS: alle Domains → 192.168.4.1 (echter Captive-Portal-Effekt)

---

### 3. `LaCrosseGateway.ino` – Notwendige Anpassungen

Folgende Änderungen müssen in der Haupt-.ino Datei vorgenommen werden:

#### a) Include hinzufügen (nach den bestehenden Includes):
```cpp
#include "CaptivePortal.h"
```

#### b) WiFi-Verbindungslogik anpassen (in `setup()` oder der WiFi-Init-Funktion):
```cpp
// --- WiFi Setup mit Captive Portal ---
Settings bootSettings;
bootSettings.Read(&logger);

String wifiSSID = bootSettings.Get("WifiSSID", "");
String wifiPass = bootSettings.Get("WifiPassword", "");

if (wifiSSID.length() == 0) {
  // Erster Start: Captive Portal öffnen
  logger.println("No WiFi credentials found – starting Captive Portal");
  CaptivePortal portal;
  portal.Begin(&bootSettings, &logger);
  while (!portal.IsDone()) {
    portal.Handle();
    yield();
  }
  portal.End();
  delay(500);
  ESP.restart();  // Neustart mit gespeicherten Credentials
}

// Normaler WiFi-Start
WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
// ... bestehender WiFi-Verbindungs-Code ...
```

#### c) Radio-Settings nach Änderung persistieren:
In der `Dispatch()`-Funktion nach dem Setzen von DataRate, ToggleMask, ToggleInterval und Frequency jeweils speichern:
```cpp
// Beispiel nach einer DataRate-Änderung für Radio 1:
Settings s; s.Read(&logger);
s.SaveRadioSettings(0,
  rfm1.GetFrequency(),
  String(DATA_RATE_R1),
  rfm1.ToggleMode,
  rfm1.ToggleInterval);
s.Write();
```

---

## Migrations-Hinweis

Da `EEPROM_SIZE` von 1024 auf 2048 erhöht wurde, müssen nach dem ersten Flashen die EEPROM-Daten einmalig zurückgesetzt werden:

```
<8266>e    → EEPROM löschen (über Serial-Befehl)
```
Danach läuft das Captive Portal beim ersten Start automatisch.
