# LaCrosseGatewayMQTT
 LaCrosseGateway from FHEM with MQTT support added

Used board in my setup: Lolin D1 mini from ESP8266 Boards, version 3.0.2

The code has been taken from FHEM and MQTT support has just been added.
I.e. all the great stuff comes from the original developers of the gateway.
All the big thanks have to go to HCS and all the others.
Find all the related board info in the FHEM forum at:
https://forum.fhem.de/index.php/topic,43672.0.html
And the wiki:
http://www.fhemwiki.de/wiki/LaCrosseGateway

| Was              | Wie                                          | FHEM-Äquivalent          |
| ---------------- | -------------------------------------------- | ------------------------ |
| lgwMode          | Dropdown 0/1/2                               | attr LGW mode            |
| lgwChannel       | Zahlenfeld                                   | attr LGW channel         |
| lgwFreq          | Zahlenfeld (kHz)                             | attr LGW frequency       |
| lgwPower         | Dropdown 0–20 dBm                            | attr LGW power           |
| lgwDataRate      | Dropdown (Baud)                              | attr LGW datarate        |
| lgwRssiThreshold | Zahlenfeld (dBm)                             | attr LGW rssiThreshold   |
| lgwEncryptKey    | Textfeld 16 Byte Hex                         | attr LGW encryptKey      |
| lgwWatchdog      | Zahlenfeld (s)                               | attr LGW watchdog        |
| SendMode         | Dropdown 0/1/2                               | attr LGW sendMode        |
| SendRetries      | Zahlenfeld                                   | attr LGW sendRetries     |
| SendHumidity     | Checkbox                                     | attr LGW sendHumidity    |
| SendBatteryBeep  | Checkbox                                     | attr LGW sendBatteryBeep |
| AsDataFull       | Checkbox                                     | attr LGW asDataFull      |
| ToggleLed        | Checkbox                                     | attr LGW toggleLed       |

## MQTT

`<topic>` ist das Topic aus der Setup-Seite (MQTT-Einstellungen).

### LaCrosse-Sensoren

Werden unabhängig vom Analyse-Modus (`<n>z`) veröffentlicht, höchstens einmal pro Intervall und Sensor:

| Topic | Inhalt |
| ----- | ------ |
| `<topic>/<ID>/temp` | Temperatur |
| `<topic>/<ID>/hum` | Luftfeuchte |
| `<topic>/<ID>/newBat`, `<topic>/<ID>/weakBat` | Batterie-Flags 0/1 |
| `<topic>/<ID>/timeStamp` | Empfangszeit |
| `<topic>/lacrosse_ids`, `<topic>/lacrosse_newid` | bekannte IDs als Bitfeld, zuletzt neu erkannte ID |

### EC3000 (Energy Count 3000)

Ein RFM69 mit Datenrate 20000 empfängt die EC3000-Steckdosen. Jede Steckdose wird höchstens einmal pro Intervall veröffentlicht:

```
<topic>/EC3000/<ID>   {"power":12.3,"energy":1.234,"max_power":45.6,"total_seconds":693790,
                       "on_seconds":85,"resets":3,"state":"ON","rssi":-71,"rfm":1}
```

Bevor ein Paket veröffentlicht wird, prüft das Gateway es gegen das letzte gültige Paket derselben ID.
Verworfen wird es, wenn die An-Zeit größer als die Gesamtzeit ist, die Steckdose aus ist aber Leistung meldet,
die Leistung über 3700 W liegt, die Gesamtzeit schneller als die echte Zeit läuft, der Verbrauch rückwärts läuft
oder stärker steigt als mit 3700 W möglich, oder der Reset-Zähler rückwärts läuft oder um mehr als 5 springt.
Ein manueller Reset der Steckdose (alle Zähler auf 0) wird erkannt. Eine neue ID wird erst nach dem zweiten
passenden Paket veröffentlicht. Verworfene Pakete stehen mit Grund im Log und unter `<topic>/EC3000/debug`.
Die FHEM-Ausgabe (`OK 22 ...`) über Serial und TCP bleibt unverändert.

Unter Setup → „EC3000 Energiezähler“ lassen sich Namen vergeben (`BA3D=Waschmaschine;BF2C=Trockner`).
Mit „Nur diese IDs verwenden“ werden alle anderen IDs für MQTT und die Live-Seite ignoriert, FHEM bekommt weiter alle.
„Automatische Frequenzkorrektur (AFC)“ (Standard: aus) lässt den RFM69 bei jedem Empfangsstart die Frequenz
nachführen. Das kann bei Steckdosen mit schlechtem Empfang helfen; zum Vergleichen RSSI und „verworfen“ auf der Live-Seite ansehen.

### Gateway-Status

| Topic | Inhalt |
| ----- | ------ |
| `<topic>/status` | alle 5 Minuten und nach jedem Verbinden (retained): Version, Laufzeit, WLAN-RSSI, IP, freier Speicher, MQTT-Reconnects, Reset-Grund, Frames/min, Anzahl EC3000 |
| `<topic>/availability` | `online` nach dem Verbinden, `offline` als Last Will |

### Home Assistant

Mit „MQTT-Discovery senden“ (Standard: an) legt Home Assistant jede EC3000 automatisch als Gerät
„EC3000 &lt;ID&gt;“ (oder mit dem Namen aus dem Setup) an: Leistung, Energie (für das Energie-Dashboard),
An/Aus und als Diagnose Maximalleistung, An-Zeit, Gesamtzeit, Resets und RSSI.
Das Gateway selbst erscheint als Gerät mit Diagnose-Sensoren aus `<topic>/status`;
ist es offline, werden alle seine Entitäten „nicht verfügbar“.

## Live-Seite und JSON

Im Menü „Live“ zeigt das Gateway alle 5 Sekunden die LaCrosse-Sensoren und darunter die EC3000 mit Leistungsverlauf der letzten 4 Minuten
Dieselben Daten liefert `http://<gateway>/api/data` als JSON (wie `/state` ohne Login).

## Danke

- **HCS** und allen Mitwirkenden im **FHEM-Forum**
  ([Thread LaCrosseGateway](https://forum.fhem.de/index.php/topic,43672.0.html),
  [FHEM-Wiki](http://www.fhemwiki.de/wiki/LaCrosseGateway)): Von dort stammt das LaCrosseGateway mit
  allen Sensor-Decodern, auch der EC3000-Decoder. Ohne die jahrelange Arbeit, die Tests und die Hinweise
  der FHEM-Gemeinde gäbe es dieses Projekt nicht.
- **Rainerlan** für die erste MQTT-Version ([Rainerlan/LaCrosseGatewayMQTT](https://github.com/Rainerlan/LaCrosseGatewayMQTT)),
  auf der dieses Repository aufbaut.
- **D3R-ST3FAN** für die Display-Seiten.
- **EC3000-Erweiterung:** Die Ideen für EC3000 über MQTT, den Plausibilitätsfilter, die Home-Assistant-Discovery,
  die Live-Seite und den Status-Topic stammen aus der
  [ESP-EC3000-MQTT-Bridge](https://github.com/wunderbaum/ESP-EC3000-MQTT-Bridge) von **wunderbaum**,
  mit Beiträgen von **yustAnotherUser** und **Xento**. Danke dafür! Umgesetzt wurden sie hier neu für den ESP8266.
- Allen, die Fehler melden, testen und Verbesserungen vorschlagen.

## Lizenz

GNU General Public License, Version 2 oder (nach Wahl) jede spätere Version, wie FHEM selbst.
Der vollständige Text steht in [LICENSE](LICENSE). Die Rechte an den übernommenen Teilen liegen bei
ihren jeweiligen Autoren.
