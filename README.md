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