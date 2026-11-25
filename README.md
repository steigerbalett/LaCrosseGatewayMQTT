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

## Betrieb ohne FHEM

Unter Setup → „Radio #1…#5“ hat jedes Funkmodul eine **Betriebsart**:

| Betriebsart | Wirkung |
| ----------- | ------- |
| LaCrosse & Co. | Datenrate(n) per Checkbox wie bisher (17.241, 9.579, 8.842, 6.631, 4.800, mehrere = Toggle) |
| EC3000 | feste Datenrate 20.000 kbps für die Energy-Count-3000-Steckdosen (Frequenz z. B. 868300) |
| PCA301 | PCA301-Empfang und Abfrage wie der FHEM-Befehl `<radio>,<freq>,<intervall>i` (Frequenz 868950, Abfrage-Intervall einstellbar) |

Damit laufen LaCrosse, EC3000 und PCA301 samt MQTT und Home Assistant ohne FHEM.
Für alles andere gibt es das Feld **Startbefehle** (wie das FHEM-Attribut `initCommands`), das nach jedem Start
ausgeführt wird, z. B. `20000#3r 3,868950,60i`. Schickt FHEM selbst Befehle für dieselben Funkmodule, gewinnt
der letzte Befehl; dann die passenden FHEM-`initCommands` entfernen.

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

Unter Setup → „LaCrosse-Sensoren“ lassen sich Namen vergeben (`1A=Wohnzimmer, 2B=Keller`).
Sie erscheinen in Home Assistant und auf der Live-Seite.
Mit „Name statt ID im MQTT-Topic“ (Standard: aus) gehen benannte Sensoren an `<topic>/<Name>/temp` usw.
statt an `<topic>/<ID>/temp`; Leerzeichen, `/`, `+` und `#` im Namen werden zu `_`.

**Batteriewechsel:** Ein LaCrosse-Sensor sucht sich mit neuen Batterien eine neue zufällige ID.
Auf der Live-Seite gibt es hinter jedem Sensor den Button „Change Battery“. Danach wird eine neue ID,
die innerhalb von 5 Minuten auftaucht (oder höchstens 5 Minuten vorher zum ersten Mal empfangen wurde) und
das Flag „neue Batterie“ meldet, auf die bisherige ID abgebildet. MQTT, Home Assistant und die Live-Seite
sehen dann weiter die alte ID; die Live-Seite zeigt die Funk-ID in Klammern. Die Zuordnungen stehen im
Setup unter „Batteriewechsel“ (`neue=alte`, z. B. `3C=1A`) und überstehen einen Neustart.
FHEM bekommt weiterhin die echte Funk-ID.

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

Unter Setup → „EC3000 Energiezähler“ lassen sich Namen vergeben (`BA3D=Waschmaschine, BF2C=Trockner`).
Mit „Nur diese IDs verwenden“ werden alle anderen IDs für MQTT und die Live-Seite ignoriert, FHEM bekommt weiter alle.
„Automatische Frequenzkorrektur (AFC)“ (Standard: aus) lässt den RFM69 bei jedem Empfangsstart die Frequenz
nachführen. Das kann bei Steckdosen mit schlechtem Empfang helfen; zum Vergleichen RSSI und „verworfen“ auf der Live-Seite ansehen.

### PCA301 (Funk-Schaltsteckdosen)

Sobald FHEM den PCA301-Empfang initialisiert hat (`<n>x`-Befehl), werden die Messwerte jeder gepairten Steckdose
nach jeder Abfrage veröffentlicht, geschaltet wird über `set` mit `ON` oder `OFF`:

```
<topic>/PCA301/<ID>       {"power":49.8,"energy":1.23,"state":"ON","channel":1}
<topic>/PCA301/<ID>/set   ON | OFF
```

`<ID>` ist die 6-stellige Hex-Adresse der Steckdose (z. B. `03A094`). Geschaltet werden kann eine Steckdose,
sobald seit dem Start mindestens einmal Werte von ihr kamen. Die FHEM-Ausgabe (`OK 24 ...`) bleibt unverändert.

### Gateway-Status

| Topic | Inhalt |
| ----- | ------ |
| `<topic>/status` | alle 5 Minuten und nach jedem Verbinden (retained): Version, Laufzeit, WLAN-RSSI, IP, freier Speicher, MQTT-Reconnects, Reset-Grund, Frames/min, Anzahl EC3000 |
| `<topic>/availability` | `online` nach dem Verbinden, `offline` als Last Will |

### Home Assistant

Mit „MQTT-Discovery senden“ (Standard: an) legt Home Assistant jeden LaCrosse-Sensor automatisch als Gerät
„LaCrosse &lt;ID&gt;“ (oder mit dem Namen aus dem Setup) an: Temperatur, Luftfeuchte (nur bei Sensoren mit Feuchte),
Batterie schwach und als Diagnose „New battery“. Die Werte kommen aus den bisherigen Topics `<topic>/<ID>/...`,
FHEM und vorhandene MQTT-Auswertungen ändern sich nicht. Ohne „Change Battery“ bekommt ein LaCrosse-Sensor nach jedem
Batteriewechsel eine neue ID und damit ein neues Gerät in Home Assistant. Ein Gerät, das nicht mehr sendet,
wird nach dreimal dem Sende-Intervall (mindestens 5 Minuten) „nicht verfügbar“ und kann in Home Assistant
unter Einstellungen → Geräte gelöscht werden.

Jede PCA301 erscheint als Gerät „PCA301 &lt;ID&gt;“ mit Schalter, Leistung und Energie.

Ebenso legt Home Assistant jede EC3000 automatisch als Gerät
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
