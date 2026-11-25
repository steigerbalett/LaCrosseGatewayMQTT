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

### Home Assistant

Mit „MQTT-Discovery senden“ (Standard: an) legt Home Assistant jede EC3000 automatisch als Gerät
„EC3000 &lt;ID&gt;“ an: Leistung, Energie (für das Energie-Dashboard), An/Aus und als Diagnose
Maximalleistung, An-Zeit, Gesamtzeit, Resets und RSSI.
