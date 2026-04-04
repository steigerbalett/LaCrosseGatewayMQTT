#define PROGNAME         "LaCrosseITPlusReader.Gateway"
#define PROGVERS         "1.36.49"

#define RFM1_SS          15
#define RFM2_SS          2
#define RFM3_SS          0
#define LED_PIN          16

// Arduino / ESP8266 libs
#include "Arduino.h"
#include "SPI.h"
#include "Wire.h"
#include "EEPROM.h"
#include "IPAddress.h"
#include "ESP8266WiFiType.h"
#include "ESP8266WiFi.h"
#include <WiFiManager.h>
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include "pins_arduino.h"
#include "ESP8266HTTPClient.h"
#include "ESP8266httpUpdate.h"
#include "WiFiUdp.h"
#include "ESP8266mDNS.h"
#include "ArduinoOTA.h"
#include "Ticker.h"

#include <PubSubClient.h>         //MQTT server library

#include <time.h>

// Other libs
#include "ArrayList.h"
#include "BMP180.h"
#include "RFMxx.h"
#include "SensorBase.h"
#include "LaCrosse.h"
#include "EMT7110.h"
#include "TX38IT.h"
#include "WSBase.h"
#include "WS1080.h"
#include "TX22IT.h"
#include "WT440XH.h"
#include "CustomSensor.h"
#include "LevelSenderLib.h"
#include "UniversalSensor.h"
#include "ESPTools.h"
#include "TypedQueue.h"
#include "HashMap.h"
#include "HTML.h"
#include "DHTxx.h"
#include "I2CBase.h"
#include "BMP280.h"
#include "BME280.h"
#include "BME680.h"
#include "BH1750.h"
#include "LM75.h"
#include "EC3000.h"
#include "ESP8266OTA.h"
#include "SC16IS750.h"
#include "SubProcessor.h"
#include "OLED.h"
#include "OLEDFonts.h"
#include "MCP23008.h"
#include "DigitalPorts.h"
#include "SHT75.h"
#include "PCA301Plug.h"
#include "PCA301.h"
#include "PCA301PlugList.h"
#include "ESP8266SoftSerial.h"
#include "XBM.h"
#include "AlarmHandler.h"
#include "Logger.h"
#include "SerialPortFlasher.h"
#include "AccessPoint.h"
#include "WebFrontend.h"
#include "DataPort.h"
#include "Settings.h"
#include "StateManager.h"
#include "OTAUpdate.h"
#include "OwnSensors.h"
#include "SerialBridge.h"
#include "SoftSerialBridge.h"
#include "Nextion.h"
#include "Display.h"
#include "DisplayAreas.h"
#include "DisplayValues.h"
#include "HardwarePageBuilder.h"
#include "Watchdog.h"
#include "AnalogPort.h"

extern "C" {
  #include "ets_sys.h"
  #include "os_type.h"
  #include "osapi.h"
  #include "mem.h"
  #include "user_interface.h"
  #include "cont.h"
  #include <lwip/dns.h>
}

/* Configuration of NTP */
#define MY_NTP_SERVER "de.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"

// The following settings can also be set from FHEM
#define ENABLE_ACTIVITY_LED    1         // <n>a       set to 0 if the blue LED bothers
                                         // <n,d>b     Alert n beeps for d seconds
unsigned long DATA_RATE_S1   = 17241ul;  // <n>c       use one of the possible data rates (for transmit on RFM #1)
bool DEBUG                   = 0;        // <n>d       set to 1 to see debug messages
                                         // <8266>e    Clear EEPROM
                                         // <8377>e    Reboot
unsigned long INITIAL_FREQ   = 868310;   // <n>f       initial frequency in kHz (5 kHz steps, 860480 ... 879515) - initial value was: 868300
                                         // <n>g       get Information: 1 = settings
                                         // <n>h       Altitude
                                         // <n,f,i>i   Init PCA for Radio #<n> to <m>MHz and <i>s Interval
byte TOGGLE_MODE_R1          = 3;        // <n>m       bits 1: 17.241 kbps, 2 : 9.579 kbps, 4 : 8.842 kbps, 8 : 20.000 kbps (for RFM #1)
byte TOGGLE_MODE_R2          = 3;        // <n>M       bits 1: 17.241 kbps, 2 : 9.579 kbps, 4 : 8.842 kbps, 8 : 20.000 kbps (for RFM #2)
byte TOGGLE_MODE_R3          = 3;        // <n>#<x>m   bits 1: 17.241 kbps, 2 : 9.579 kbps, 4 : 8.842 kbps, 8 : 20.000 kbps (for RFM #3)
byte TOGGLE_MODE_R4          = 3;        // <n>#<x>m   bits 1: 17.241 kbps, 2 : 9.579 kbps, 4 : 8.842 kbps, 8 : 20.000 kbps (for RFM #4)
byte TOGGLE_MODE_R5          = 3;        // <n>#<x>m   bits 1: 17.241 kbps, 2 : 9.579 kbps, 4 : 8.842 kbps, 8 : 20.000 kbps (for RFM #5)

                                         // <n>o       set HF-parameter e.g. 50305o for RFM12 or 1,4o for RFM69
byte PASS_PAYLOAD            = 0;        // <n>p       transmitted the payload on the serial port 1: all, 2: only undecoded data
unsigned long DATA_RATE_R1   = 17241ul;  // <n>r       use one of the possible data rates (for RFM #1)
unsigned long DATA_RATE_R2   = 9579ul;   // <n>R       use one of the possible data rates (for RFM #2)
unsigned long DATA_RATE_R3   = 8842ul;   // <n>#<x>r   use one of the possible data rates (for RFM #3)
unsigned long DATA_RATE_R4   = 20000ul;  // <n>#<x>r   use one of the possible data rates (for RFM #4)
unsigned long DATA_RATE_R5   = 111ul;    // <n>#<x>r   use one of the possible data rates (for RFM #5)


                                         // <x,x,...>s Send to PCA301 (must be 10 byte)
                                         // <x,x,...>S Send to CustomSensor

uint16_t TOGGLE_INTERVAL_R1  = 30;        // <n>t       0=no toggle, else interval in seconds (for RFM #1)
uint16_t TOGGLE_INTERVAL_R2  = 0;        // <n>T       0=no toggle, else interval in seconds (for RFM #2)
uint16_t TOGGLE_INTERVAL_R3  = 0;        // <n>#<x>t   0=no toggle, else interval in seconds (for RFM #3)
uint16_t TOGGLE_INTERVAL_R4  = 0;        // <n>#<x>t   0=no toggle, else interval in seconds (for RFM #4)
uint16_t TOGGLE_INTERVAL_R5  = 0;        // <n>#<x>t   0=no toggle, else interval in seconds (for RFM #5)

                                         // <xxxx>u    Send xxxx to SubProcessor
                                         // v          show version
bool USE_WIFI                = 1;        // <n>w       0=no wifi
                                         // x          test command 
uint16_t ANALYZE_FRAMES      = 1;        // <n>z       set to 1 to display analyzed frame data instead of the normal data
uint32_t KVINTERVAL          = 10;       //            interval for KV-transmision
uint32_t OSINTERVAL          = 10;       //            interval for own sensors transmision

bool USE_SERIAL              = 1;        //            0=do not send sensor data on the serial
bool DEBUG_PCA301            = 0;        //            debug PCA301

// wifi settings
#define FRONTEND_PORT          80        // Port for the Web frontend
#define DATA_PORT1             81        // Port for data
#define DATA_PORT2             82        // Port for data
#define DATA_PORT3             83        // Port for data
#define OTA_PORT               8266
IPAddress AP_IP              = IPAddress(192, 168, 4, 1);
IPAddress AP_SUBNET          = IPAddress (255, 255, 255, 0);

byte AddOnPinHandler(byte command, byte pin, byte value);

IPAddress ipAddr;
unsigned char macAddr[6];
unsigned char bssid[6];
int channel = 0;
int rssi = -999;

Logger logger;


// --- Variables -------------------------------------------------------------------------------------------------------
#define TIME_SEC_BETWEEN_PUBLISH 20

#define USE_MQTT_Pubsub
#ifdef USE_MQTT_Pubsub
  WiFiClient espClient;
  PubSubClient client(espClient);
  unsigned long lastMsg = 0;
  #define MSG_BUFFER_SIZE  (200)
  char msg[MSG_BUFFER_SIZE];
  int value = 0;
  unsigned long secBetweenPublish = TIME_SEC_BETWEEN_PUBLISH;

  char mqtt_server[40] = "";
  int mqtt_port = 1883;
  char mqtt_user[40] = "";
  char mqtt_password[64] = "";
  String mqttTopicBaseGW = "lacrossegateway";
#endif

static String getLocalTimeString() {
  time_t now = time(nullptr);
  struct tm *ti = localtime(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", ti);
  return String(buf);
}

static void LogBssid(uint8_t *b) {
  char obuf[4];
  for (int i = 0; i < 6; i++) {
    itoa(b[i], obuf, 16);
    logger.print(String(obuf));
    if (i < 5) logger.print(":");
  }
  logger.println("");
}

struct RadioToggleState {
  bool     enabled;
  String   type;
  long     freqKHz;
  String   fixedDataRate;
  uint8_t  toggleMask;
  uint32_t toggleIntervalS;
  uint8_t  toggleIndex;
  uint32_t lastToggleMs;
};
RadioToggleState g_radio[5];

byte scanWifi(String ctSSID) {
  memset(bssid, 0, 6);
  rssi = -999;
  byte numSsid = WiFi.scanNetworks();
  int corrSsidCnt = 0;
  uint8_t *thisBssid;
  
  logger.println("Number of all SSIDs: " + String(numSsid));
  for (int thisNet = 0; thisNet<numSsid; thisNet++) {
    logger.println("SSID found: " + WiFi.SSID(thisNet));
    if (!strcmp(WiFi.SSID(thisNet).c_str(), ctSSID.c_str())) {
      corrSsidCnt++;
      
      thisBssid = WiFi.BSSID(thisNet);
      logger.print("SSID " + ctSSID + " found on ch: " + String(WiFi.channel(thisNet)) + ", rssi: " + String(WiFi.RSSI(thisNet))+ ", bssid: ");
      LogBssid(WiFi.BSSID(thisNet));
      logger.println(F(" ,thisNet: ") + String(thisNet));
      logger.print("Last rssi: " + String(rssi));
  
      if (WiFi.RSSI(thisNet) > rssi) {
        logger.println("New bssid copied!");
        memcpy(bssid, WiFi.BSSID(thisNet), 6);
        channel = WiFi.channel(thisNet);
        rssi = WiFi.RSSI(thisNet);
      }
    }
  }
  return corrSsidCnt;
}

unsigned long lastToggleR1 = 0;
unsigned long lastToggleR2 = 0;
unsigned long lastToggleR3 = 0;
unsigned long lastToggleR4 = 0;
unsigned long lastToggleR5 = 0;
unsigned long commandData[32];
byte commandDataPointer = 0;
ESPTools esp(LED_PIN);
SC16IS750 sc16is750 = SC16IS750(SC16IS750_MODE_I2C, 0x90);
SC16IS750 sc16is750_2 = SC16IS750(SC16IS750_MODE_I2C, 0x92);

RFMxx rfm1(13, 12, 14, RFM1_SS);
RFMxx rfm2(13, 12, 14, RFM2_SS);
RFMxx rfm3(13, 12, 14, RFM3_SS);
RFMxx rfm4(13, 12, 14, 0, -1, AddOnPinHandler);
RFMxx rfm5(13, 12, 14, 1, -1, AddOnPinHandler);
RFMxx *rfms[5] = { &rfm1, &rfm2, &rfm3, &rfm4, &rfm5 };

AlarmHandler alarm(7, AddOnPinHandler);

OwnSensors ownSensors;
WebFrontend frontend(FRONTEND_PORT);
DataPort dataPort1;
DataPort dataPort2;
DataPort dataPort3;
AccessPoint accessPoint (AP_IP, AP_IP, AP_SUBNET, "LaCrosseGateway");
StateManager stateManager;
PCA301 pca301;
ESP8266OTA ota;
unsigned int lastOtaProgress;
SubProcessor subProcessor(&sc16is750, 5, "addon.hex");
SerialBridge serialBridge(&sc16is750, 5, "addon.hex");
SubProcessor subProcessor2(&sc16is750_2, 5, "addon2.hex");
SerialBridge serialBridge2(&sc16is750_2, 5, "addon2.hex");
bool useSerialBridge = false;
bool useSerialBridge2 = false;
SoftSerialBridge softSerialBridge;
Nextion nextion;
DHTxx dht;
Ticker ticker;
Display display;
DigitalPorts digitalPorts;
unsigned long lastSensorMeasurement = 0;
unsigned long lastSensorTransmission = 0;
bool dataPort1Connected = false;
bool dataPort2Connected = false;
bool dataPort3Connected = false;
bool bridge1Connected = false;
bool bridge2Connected = false;
bool bridge3Connected = false;
Watchdog watchdog;
SerialPortFlasher serialPortFlasher;
AnalogPort analogPort;
bool logIsCleared = false;
bool mqttEnabled = false;

#ifdef USE_MQTT_Pubsub
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    //digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    //digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnectMqtt() {
  // Loop until we're reconnected
  Serial.print("MQTT disconnected - Attempting MQTT connection...");

  // Attempt to connect
  logger.println("Connecting to MQTT - User/PW (eeprom setting):" + String(mqtt_user) + "/" + String(mqtt_password));
  if (client.connect("ESP32Client", mqtt_user, mqtt_password )) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
  } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //delay(5000);
  }
}
#endif

byte AddOnPinHandler(byte command, byte pin, byte value) {
  byte result = 1;
  if (sc16is750.IsConnected()) {
    switch (command) {
    case 1:
      sc16is750.PinMode(pin, value);
      break;
    case 2:
      sc16is750.DigitalWrite(pin, value);
      break;
    case 3:
      result = sc16is750.DigitalRead(pin);
      break;
    default:
      break;
    }
  }

  return result;
}

void SetDebugMode(boolean mode) {
  DEBUG = mode;
  SensorBase::SetDebugMode(mode);
  for (int i = 0; i < 5; i++) rfms[i]->SetDebugMode(mode);
}

void Dispatch(String data, String raw="") {
  if(USE_SERIAL) {
    Serial.println(data);
  }
  
  if (USE_WIFI) {
    dataPort1.AddPayload(data);
    dataPort2.AddPayload(data);
    dataPort3.AddPayload(data);
  }

  if (raw.length() > 0) {
    raw = " [" + raw + "]";
  }

  if (data.startsWith("\n")) {
    data = data.substring(1);
  }
  logger.logData(data + raw);

  esp.Blink(1);
}

void SetDataRate(RFMxx *rfm, unsigned long dataRate) {
  if(rfm->GetDataRate() != 20000 && dataRate == 20000) {
    rfm->InitializeEC3000();
    rfm->EnableReceiver(true);
  }
  else if(rfm->GetDataRate() == 20000 && dataRate != 20000) {
    rfm->InitializeLaCrosse();
    rfm->EnableReceiver(true);
  }
  

  rfm->SetDataRate(dataRate);
}

static void HandleSerialPort(char c) {
  static unsigned long value;
  static bool rfmNumberFlag;
  static byte rfmNumber = 0;
  unsigned long dataRate = 0;
  static String commandString = "";
  static bool commandStringFlag = false;

  if(c != '"' && commandStringFlag) {
    commandString += c;
  }
  else if (c == '"') {
    if(!commandStringFlag) {
      commandStringFlag = true;
    }
    else {
      commandStringFlag = false;
      HandleCommandString(commandString);
      commandString = "";
    }
  }
  else if (c == ',') {
    commandData[commandDataPointer++] = value;
    value = 0;
  }
  else if(rfmNumberFlag) {
    rfmNumber = c - '0';
    rfmNumberFlag = false;
  }
  else if(c == '#') {
    rfmNumberFlag = true;
  }
  else if ('0' <= c && c <= '9') {
    value = 10 * value + c - '0';
  }
  else if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
    if (rfmNumber == 0) {
      rfmNumber = ('A' <= c && c <= 'Z') ? 2 : 1;
    }
  
    switch (c) {
    case 'a':
      // Activity LED    
      esp.EnableLED(value);
      break;

    case 'b':
      // Alarm 
      if (sc16is750.IsConnected()) {
        commandData[commandDataPointer] = value;
        if (++commandDataPointer == 2) {
          alarm.Set(commandData[0], commandData[1]);
        }
        else {
          alarm.Set(0, value);
        }
      }
      commandDataPointer = 0;
      break;

    case 'd':
      // DEBUG
      SetDebugMode(value);
      break;

    case 'e':
      if (value == 8266) {
        EEPROM.begin(EEPROM_SIZE);
        for (int i = 0; i < EEPROM_SIZE; i++) {
          EEPROM.write(i, 0);
        }
        EEPROM.commit();
        EEPROM.end();
        delay(500);
        ESP.restart();
        break;
      }
      else if (value == 8377) {
        ESP.restart();
      }
    
    case 'g':
      if (value == 1) {
        Settings settings;
        settings.Read(&logger);
        Dispatch(settings.ToString());
      }
      break;
      
    case 'h':
      // height
      ownSensors.SetAltitudeAboveSeaLevel(value);
      break;
    case 'x':
    case 'X':
      // Tests
      HandleCommandX(value);
      break;
    case 'w':
      // wifi    
      if (!value) {
        StopWifi();
      }
      break;
    case 'r':
    case 'R':
      // Data rate
      switch (value) {
      case 0:
        dataRate = 17241ul;
        break;
      case 1:
        dataRate = 9579ul;
        break;
      case 2:
        dataRate = 8842ul;
        break;
      case 3:
        dataRate = 20000ul;
        break;
      default:
        dataRate = value;
        break;
      }
      if (rfmNumber == 1 && rfm1.IsConnected()) {
        DATA_RATE_R1 = dataRate;
        SetDataRate(&rfm1, DATA_RATE_R1);
      }
      else if (rfmNumber == 2 && rfm2.IsConnected()) {
        DATA_RATE_R2 = dataRate;
        SetDataRate(&rfm2, DATA_RATE_R2);
      }
      else if (rfmNumber == 3 && rfm3.IsConnected()) {
        DATA_RATE_R3 = dataRate;
        SetDataRate(&rfm3, DATA_RATE_R3);
      }
      else if (rfmNumber == 4 && rfm4.IsConnected()) {
        DATA_RATE_R4 = dataRate;
        SetDataRate(&rfm4, DATA_RATE_R4);
      }
      else if (rfmNumber == 5 && rfm5.IsConnected()) {
        DATA_RATE_R5 = dataRate;
        SetDataRate(&rfm5, DATA_RATE_R5);
      }

      break;
    case 'm':
    case 'M':
      if (rfmNumber == 1 && rfm1.IsConnected()) {
        rfm1.ToggleMode = value;
      }
      else if (rfmNumber == 2 && rfm2.IsConnected()) {
        rfm2.ToggleMode = value;
      }
      else if (rfmNumber == 3 && rfm3.IsConnected()) {
        rfm3.ToggleMode = value;
      }
      else if (rfmNumber == 4 && rfm4.IsConnected()) {
        rfm4.ToggleMode = value;
      }
      else if (rfmNumber == 5 && rfm5.IsConnected()) {
        rfm5.ToggleMode = value;
      }
      break;
    case 'o':
    case 'O':
      // Set HF parameter
      commandData[commandDataPointer] = value;
      HandleCommandO(rfmNumber, value, commandData, ++commandDataPointer);
      break;
    case 'p':
      PASS_PAYLOAD = value;
      break;
    case 's':
      // Send
      commandData[commandDataPointer] = value;
      HandlePCA301Send(commandData, ++commandDataPointer);
      commandDataPointer = 0;
      break;
    case 'S':
      // Send
      commandData[commandDataPointer] = value;
      HandleCommandS(commandData, ++commandDataPointer);
      commandDataPointer = 0;
      break;
    case 'i':
      // Init PCA301
      commandData[commandDataPointer] = value;
      HandleCommandI(commandData, ++commandDataPointer);
      commandDataPointer = 0;
      break;
    case 't':
    case 'T':
      // Toggle data rate
      if (rfmNumber == 1 && rfm1.IsConnected()) {
        rfm1.ToggleInterval = value;
      }
      else if (rfmNumber == 2 && rfm2.IsConnected()) {
        rfm2.ToggleInterval = value;
      }
      if (rfmNumber == 3 && rfm3.IsConnected()) {
        rfm3.ToggleInterval = value;
      }
      if (rfmNumber == 4 && rfm4.IsConnected()) {
        rfm4.ToggleInterval = value;
      }
      if (rfmNumber == 5 && rfm5.IsConnected()) {
        rfm5.ToggleInterval = value;
      }
      break;
    case 'v':
      // Version info
      HandleCommandV();
      break;

    case 'f':
    case 'F':
      if (rfmNumber == 1 && rfm1.IsConnected()) {
        rfm1.SetFrequency(value); 
      }
      else if (rfmNumber == 2 && rfm2.IsConnected()) {
        rfm2.SetFrequency(value);
      }
      else if (rfmNumber == 3 && rfm3.IsConnected()) {
        rfm3.SetFrequency(value);
      }
      else if (rfmNumber == 4 && rfm4.IsConnected()) {
        rfm4.SetFrequency(value);
      }
      else if (rfmNumber == 5 && rfm5.IsConnected()) {
        rfm5.SetFrequency(value);
      }
      break;

    case 'z':
      ANALYZE_FRAMES = (uint16_t)value;
      break;

    default:
      HandleCommandV();
      break;
    }
    value = 0;
    rfmNumberFlag = false;
    rfmNumber = 0;
  }
  else if (' ' < c && c < 'A') {
    HandleCommandV();
  }
  
}

void LoadRadioSettings(Settings &settings) {
  for (byte i = 0; i < 5; i++) {
    String p = "Radio" + String(i + 1);
    g_radio[i].type            = settings.Get(p + "Type", i == 0 ? "RFM69" : "---");
    g_radio[i].enabled         = (g_radio[i].type != "---");
    g_radio[i].freqKHz         = (long)settings.GetInt(p + "Freq", i == 0 ? 868310 : 868300);
    g_radio[i].fixedDataRate   = settings.Get(p + "DataRate", "17.241");
    g_radio[i].toggleMask      = (uint8_t) settings.GetInt(p + "ToggleMask",  0);
    g_radio[i].toggleIntervalS = (uint32_t)settings.GetInt(p + "ToggleInterval", i == 0 ? 30 : 0);
    g_radio[i].toggleIndex  = 0;
    g_radio[i].lastToggleMs = 0;
  }
}

void HandleCommandString(String command) {
  String upperCommand = command;
  upperCommand.toUpperCase();

  if (upperCommand.startsWith("OLED ") && display.IsConnected()) {
    display.Command(command.substring(5));
  }
  else if (upperCommand.startsWith("MCP ") && digitalPorts.IsConnected()) {
    digitalPorts.Command(command.substring(4));
  }
  else if (upperCommand.startsWith("SETUP ")) {
    Settings settings;
    settings.Read(&logger);

    bool radioLock = settings.GetBool("RadioLock", false);
    String savedRadio[5][5];
    const char* radioKeys[] = {"Type", "Freq", "DataRate", "ToggleMask", "ToggleInterval"};
    if (radioLock) {
      for (byte i = 0; i < 5; i++) {
        String p = "Radio" + String(i + 1);
        for (byte k = 0; k < 5; k++) {
          savedRadio[i][k] = settings.Get(p + radioKeys[k], "");
        }
      }
    }

    settings.FromString(command.substring(6));

    if (radioLock) {
      for (byte i = 0; i < 5; i++) {
        String p = "Radio" + String(i + 1);
        for (byte k = 0; k < 5; k++) {
          if (savedRadio[i][k].length() > 0) {
            settings.Add(p + radioKeys[k], savedRadio[i][k]);
          }
        }
      }
    }

    settings.Write();
  }
  else if (upperCommand.startsWith("WATCHDOG ")) {
    watchdog.Command(command.substring(9));
  }
  else if (upperCommand.startsWith("FIRMWARE")) {
    serialPortFlasher.Begin();
  }
}

void HandleCommandI(unsigned long *commandData, byte length){
  word interval = 60;
  if (length == 3) {
    interval = commandData[2];
  }

  if (length >= 2) {
    RFMxx *rfm;
    switch (commandData[0]) {
    case 1:
      rfm = &rfm1;
      break;
    case 2:
      rfm = &rfm2;
      break;
    case 3:
      rfm = &rfm3;
      break;
    case 4:
      rfm = &rfm4;
      break;
    case 5:
      rfm = &rfm5;
      break;
    default:
      rfm = NULL;
      break;
    }
    if (rfm != NULL){
      pca301.Begin(rfm, commandData[1], interval, [](String key, String value, bool write) {
        String result = "";
        Settings settings;
        if (write) {
          settings.Read(&logger);
          settings.Add(key, value);
          settings.Write();
        }
        else {
          settings.Read(&logger);
          result = settings.Get(key, value);
        }

        return result;
      });
    }
  }
  
}

// This function is for testing 
void HandleCommandX(byte value) {
  if (value == 22) {
    String dhtTest = dht.TryInitialize(0) ? F("OK :-)") : F("not found :-(");
    logger.println(String(F("DHT22: ")) + dhtTest);
  }
}

void HandleCommandS(unsigned long *data, byte size) {
  rfm1.EnableReceiver(false);

  struct CustomSensor::Frame frame;
  frame.ID = data[0];
  frame.NbrOfDataBytes = size - 1;

  for (int i = 0; i < frame.NbrOfDataBytes; i++) {
    frame.Data[i] = data[i + 1];
  }

  CustomSensor::SendFrame(&frame, &rfm1, DATA_RATE_S1);

  rfm1.EnableReceiver(true);
}

void HandlePCA301Send(unsigned long *data, byte size) {
  if (size == 10 && pca301.IsInitialized()) {
    pca301.GetUsedRadio()->EnableReceiver(false);

    byte payload[10];
    for (int i = 0; i < 10; i++) {
      payload[i] = data[i];
    }
    pca301.SendPayload(payload, false);

    pca301.GetUsedRadio()->EnableReceiver(true);

  }
}

RFMxx *GetRfmForNumber(byte number) {
  if (number >= 1 && number <= 5) return rfms[number - 1];
  return NULL;
}

void HandleCommandO(byte rfmNbr, unsigned long value, unsigned long *data, byte size) {
  // 1,4o for RFM69
  RFMxx *rfm = GetRfmForNumber(rfmNbr);
  if (size == 2 && rfm->GetRadioType() == RFMxx::RFM69CW) {
    rfm->SetHFParameter(data[0], data[1]);
  }
}

static void AppendRfmInfo(String &result, byte num, RFMxx *rfm) {
  if (!rfm->IsConnected()) return;
  result += (num == 1) ? F(" (") : F(" + (");
  result += String(num);
  result += "=";
  result += rfm->GetRadioName();
  result += F(" f:");
  result += rfm->GetFrequency();
  if (rfm->ToggleInterval) {
    result += F(" t:");
    result += rfm->ToggleInterval;
    result += "~";
    result += rfm->ToggleMode;
  } else if (rfm->GetRadioType() == RFMxx::RadioType::RFM95W) {
    result += F(" LoRa SF=");
    result += String(rfm->GetSpreadingFactor());
    result += F(" BW=");
    result += String(rfm->GetBandwidthHz() / 1000);
    result += F(" kHz");
  } else {
    result += F(" r:");
    result += rfm->GetDataRate();
  }
  result += ")";
}

void HandleCommandV() {
  String result = "\n";

  result += "[";
  result += PROGNAME;
  result += ".";
  result += PROGVERS;

  for (int i = 0; i < 5; i++) AppendRfmInfo(result, i + 1, rfms[i]);

  if (ownSensors.HasSHT75()) {
    result += " + SHT75";
  }
  if (ownSensors.HasBMP180()) {
    result += " + BMP180";
  }
  if (ownSensors.HasBMP280()) {
    result += " + BMP280";
  }
  if (ownSensors.HasBME280()) {
    result += " + BME280";
  }
  if (ownSensors.HasBME680()) {
    result += " + BME680";
  }
  if (ownSensors.HasDHT22()) {
    result += " + DHT22";
  }
  if (ownSensors.HasLM75()) {
    result += " + LM75";
  }
  if(ownSensors.HasBH1750()) {
    result += " + BH1750";
  }
  if (sc16is750.IsConnected() || sc16is750_2.IsConnected()) {
    result += " + SC16IS750 (";
    if (sc16is750.IsConnected()) {
      result += "0x90";
    }
    if (sc16is750.IsConnected() && sc16is750_2.IsConnected()) {
      result += ", ";
    }
    if (sc16is750_2.IsConnected()) {
      result += "0x92";
    }
    result += ")";
  }
  if (display.IsConnected()) {
    result += " + OLED";
  }
  if (digitalPorts.IsConnected()) {
    result += " + MCP23008";
  }

  result += " {IP=";
  if (WiFi.status() == WL_CONNECTED) {
    result += WiFi.localIP().toString();
  }
  else if (!USE_WIFI) {
    result += "Disabled";
  }
  else {
    result += WiFi.softAPIP().toString();
  }
  result += "}";

  result += "]";
  
  Dispatch(result);
}

#define MAX_LACROSSE_SENSORS 64    
#define ACTIVE_CH_LEN (MAX_LACROSSE_SENSORS/8)
char activeChannel[ACTIVE_CH_LEN] = {0};
unsigned int sensorChannelId = 0;
unsigned int channelShift = 0;
unsigned int channelShiftByte = 0;
char activeChannelStr[2 + 2 * ACTIVE_CH_LEN + 1]; //leading '0x' - ...data... - tailing \0 byte
char *activeChannelStrPtr;
unsigned long sensorRxTime[MAX_LACROSSE_SENSORS] = {0};

//MQTT data buffer
char bufData[30];
char bufTopic[100];

bool HandleReceivedData(RFMxx *rfm) {
  unsigned long timeNow = millis();
  bool result = false;
  bool b_publishData = false;
  char activeChannelStrNew[2 + 2 * ACTIVE_CH_LEN + 1]; //leading '0x' - ...data... - tailing \0 byte
  String publishData;

  rfm->EnableReceiver(false);
  // FIX 3: all exit paths must call delete[] to prevent heap fragmentation / OOM
  const size_t fifoSize = rfm->GetFiFoSize();
  byte *payload = new byte[fifoSize];
  byte payloadSize = rfm->GetPayload(payload);
  rfm->EnableReceiver(true);

  String payloadString;
  for(int i = 0; i < payloadSize; i++) {
    payloadString += (char)payload[i];
  }

  if(rfm->GetRadioType() == RFMxx::RadioType::RFM95W && payloadString.startsWith("OK ")) {
    payloadString += " ";
    payloadString += rfm->GetRSSI();
    Dispatch(payloadString, payloadString);

    result = true;
    delete[] payload;  // FIX 3: free before early return
    return result;
  }
  else {
    if(ANALYZE_FRAMES > 0) {
      unsigned long dataRate = rfm->GetDataRate();
      if(bitRead(ANALYZE_FRAMES, 0) && LaCrosse::IsValidDataRate(dataRate)) {
        int startIdx, stopIdx;
        
        String laCrosseAnalysisString = LaCrosse::AnalyzeFrame(payload);
        logger.println(laCrosseAnalysisString);

#ifdef USE_MQTT_Pubsub
  if (mqttEnabled) {
    if(WiFi.status() != WL_CONNECTED){
      logger.println(F("WiFi disconnected - skip MQTT publish"));
    } else {
      if(laCrosseAnalysisString.indexOf("CRC:OK")>=0){
  
            logger.print("Epoch time incl DST: ");
            String timeNowLocal = getLocalTimeString();
            logger.println(timeNowLocal);
            
            //Extract ID
            startIdx = laCrosseAnalysisString.indexOf("ID:");
            stopIdx = laCrosseAnalysisString.indexOf("NewBatt:");
            publishData = laCrosseAnalysisString.substring(startIdx+3, stopIdx-1);
            if (publishData.length() == 1){
              publishData = '0' + publishData;
            }
            publishData.toUpperCase();
            String newId = publishData;
            
            String mqttTopicBase = mqttTopicBaseGW + "/" + publishData;
            String mqttTopic = mqttTopicBase;
  
            //calculate bit-position of received sensor ID
            sensorChannelId = strtol(publishData.c_str(),0,16);
            if (sensorChannelId >= MAX_LACROSSE_SENSORS){
              logger.print("Error - Too big sensor ID received: ");
              logger.println(sensorChannelId);  
              //limit sensor ID to avoid memory overwrites!
              sensorChannelId = (MAX_LACROSSE_SENSORS-1);          
            }
  
            //use ID to add "last RxTime"
            //if time passed since last publish of sensor is > TIME_SEC_BETWEEN_PUBLISH OR if sensor data has been received 1st time --> publish sensor data
            if (client.connected()&&(((timeNow - sensorRxTime[sensorChannelId])>(secBetweenPublish*1000))||(sensorRxTime[sensorChannelId]==0))){
              //use sensor data
              b_publishData = true;
              sensorRxTime[sensorChannelId] = millis();
              logger.println("Publishing sensor data...");
            }else if(!client.connected()){
              logger.println("MQTT unconnected - skip to publish!");
            }
            else{
              logger.println("Data received from same sensor with 10s - skip to publish!");
            }
            
            channelShiftByte = (sensorChannelId / 8);             // ganzkommaTeil der Division = vievieltes byte der 8 bytes
            channelShiftByte &= 0x0F;                             // sicherstellen dass es keinen Overflow gibt (sollte nicht vokommen)
            channelShift = (sensorChannelId%8);                   // nachkommaanteil der Division = vievieltes bit in einem bytes
            activeChannel[channelShiftByte] |= (1<<channelShift); //Bit setzen für neue Kanal-ID
  
            //output bit-position byte array as string 0x....
            activeChannelStrNew[0]='0';
            activeChannelStrNew[1]='x';
            activeChannelStrPtr = &activeChannelStrNew[2];
            for (int i = 0; i < ACTIVE_CH_LEN; i++) {
              activeChannelStrPtr += sprintf(activeChannelStrPtr, "%02X", activeChannel[ACTIVE_CH_LEN-i-1]);
            }
            if(strcmp(activeChannelStrNew,activeChannelStr)){
              //previousely, ID-string was only published if new ID was discovered (now copy-pasted outside strcmp)
              /*
              mqttTopic = mqttTopicBaseGW + "/lacrosse_ids";
              mqttTopic.toCharArray(bufTopic, 100);
              if (b_publishData){
                client.publish(bufTopic, activeChannelStrNew);
              } 
              */           
              mqttTopic = mqttTopicBaseGW + "/lacrosse_newid";
              mqttTopic.toCharArray(bufTopic, 100);
              newId.toCharArray(bufData, 30);
              if (b_publishData){
                client.publish(bufTopic, bufData);
              }            
            }
            //publish ID-string (better to publish this everstime...)
            mqttTopic = mqttTopicBaseGW + "/lacrosse_ids";
            mqttTopic.toCharArray(bufTopic, 100);
            if (b_publishData){
              client.publish(bufTopic, activeChannelStrNew);
            }

            strcpy(activeChannelStr, activeChannelStrNew);
            
            logger.print("ID-string: ");
            logger.println(activeChannelStr);
            
            //Extract NewBatt
            startIdx = stopIdx;
            stopIdx = laCrosseAnalysisString.indexOf("Bit12:");
            publishData = laCrosseAnalysisString.substring(startIdx+8, stopIdx-1);
            // logger.print("NewBatt: ");
            // logger.println(publishData);
  
            mqttTopic = mqttTopicBase + "/newBat";
            mqttTopic.toCharArray(bufTopic, 100);
            publishData.toCharArray(bufData, 30);
            if (b_publishData){
              client.publish(bufTopic, bufData);
            }
  
            //Extract Temp
            startIdx = laCrosseAnalysisString.indexOf("Temp:");
            stopIdx = laCrosseAnalysisString.indexOf("Hum:");
            publishData = laCrosseAnalysisString.substring(startIdx+5, stopIdx-1);
            // logger.print("Temp: ");
            // logger.println(publishData);
  
            mqttTopic = mqttTopicBase + "/temp";
            mqttTopic.toCharArray(bufTopic, 100);
            publishData.toCharArray(bufData, 30);
            if (b_publishData){
              client.publish(bufTopic, bufData);
            }
  
            //Extract Hum
            startIdx = stopIdx;
            stopIdx = laCrosseAnalysisString.indexOf("WeakBatt:");
            publishData = laCrosseAnalysisString.substring(startIdx+4, stopIdx-1);
            // logger.print("Hum: ");
            // logger.println(publishData);
  
            mqttTopic = mqttTopicBase + "/hum";
            mqttTopic.toCharArray(bufTopic, 100);
            publishData.toCharArray(bufData, 30);
            if (b_publishData){
              client.publish(bufTopic, bufData);
            }
            //Extract WeakBatt
            startIdx = stopIdx;
            publishData = laCrosseAnalysisString.substring(startIdx+9, startIdx+10);
            // logger.print("WeakBatt: ");
            // logger.println(publishData);
  
            mqttTopic = mqttTopicBase + "/weakBat";
            mqttTopic.toCharArray(bufTopic, 100);
            publishData.toCharArray(bufData, 30);
            if (b_publishData){
              client.publish(bufTopic, bufData);
            }
  
            mqttTopic = mqttTopicBase + "/timeStamp";
            mqttTopic.toCharArray(bufTopic, 100);
            timeNowLocal.toCharArray(bufData, 30);
            if (b_publishData){
              client.publish(bufTopic, bufData);        
            }
          }
        }
      }
    }
  #endif
      if(bitRead(ANALYZE_FRAMES, 1) && TX22IT::IsValidDataRate(dataRate)) {
        logger.println(TX22IT::AnalyzeFrame(payload));
      }
      if(bitRead(ANALYZE_FRAMES, 2) && WS1080::IsValidDataRate(dataRate)) {
        logger.println(WS1080::AnalyzeFrame(payload));
      }
      if(bitRead(ANALYZE_FRAMES, 3) && EMT7110::IsValidDataRate(dataRate)) {
        logger.println(EMT7110::AnalyzeFrame(payload));
      }
      if(bitRead(ANALYZE_FRAMES, 4) && LevelSenderLib::IsValidDataRate(dataRate)) {
        logger.println(LevelSenderLib::AnalyzeFrame(payload));
      }
      if(bitRead(ANALYZE_FRAMES, 5) && CustomSensor::IsValidDataRate(dataRate)) {
        logger.println(CustomSensor::AnalyzeFrame(payload));
      }
      if(bitRead(ANALYZE_FRAMES, 6) && EC3000::IsValidDataRate(dataRate)) {
        logger.println(EC3000::AnalyzeFrame(payload));
      }
      if(bitRead(ANALYZE_FRAMES, 7) && PCA301::IsValidDataRate(dataRate) && pca301.IsInitialized()) {
        logger.println(pca301.AnalyzeFrame(payload));
      }
    }

    if(PASS_PAYLOAD == 1) {
      for(int i = 0; i < payloadSize; i++) {
        Serial.print(payload[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
    else {
      if(DEBUG) {
        Serial.print("\r\nEnd receiving, HEX raw data: ");
        for(int i = 0; i < 16; i++) {
          Serial.print(payload[i], HEX);
          Serial.print(" ");
        }
        Serial.println();

        String raw = rfm->GetRadioName() + ": ";
        for(int i = 0; i < 5; i++) {
          String bt = String(payload[i], HEX);
          bt = bt.length() == 1 ? ("0" + bt) : bt;
          raw += bt;
          raw += " ";
        }
        raw.toUpperCase();
        logger.println(raw);
      }

      String data = "";
      byte frameLength = 16;

      // Try LaCrosse like TX29DTH
      if(data.length() == 0 && LaCrosse::IsValidDataRate(rfm->GetDataRate())) {
        data = LaCrosse::GetFhemDataString(payload);
        frameLength = LaCrosse::FRAME_LENGTH;
      }

      // Try EC3000
      if(data.length() == 0 && EC3000::IsValidDataRate(rfm->GetDataRate())) {
        data = EC3000::GetFhemDataString(payload);
        frameLength = EC3000::FRAME_LENGTH;
      }

      // Try TX22IT (WS 1600)
      if(data.length() == 0 && TX22IT::IsValidDataRate(rfm->GetDataRate())) {
        data = TX22IT::GetFhemDataString(payload);
        frameLength = TX22IT::GetFrameLength(payload);
      }

      // Try WS 1080
      if(data.length() == 0 && WS1080::IsValidDataRate(rfm->GetDataRate())) {
        data = WS1080::GetFhemDataString(payload);
        frameLength = WS1080::FRAME_LENGTH;
      }

      // Try LevelSender
      if(data.length() == 0 && LevelSenderLib::IsValidDataRate(rfm->GetDataRate())) {
        data = LevelSenderLib::GetFhemDataString(payload);
        frameLength = LevelSenderLib::FRAME_LENGTH;
      }

      // Try EMT7110
      if(data.length() == 0 && EMT7110::IsValidDataRate(rfm->GetDataRate())) {
        data = EMT7110::GetFhemDataString(payload);
        frameLength = EMT7110::FRAME_LENGTH;
      }

      // Try TX38IT
      if(data.length() == 0 && TX38IT::IsValidDataRate(rfm->GetDataRate())) {
        data = TX38IT::GetFhemDataString(payload);
        frameLength = TX38IT::FRAME_LENGTH;
      }

      // Try WT440XH
      if(data.length() == 0 && WT440XH::IsValidDataRate(rfm->GetDataRate())) {
        data = WT440XH::GetFhemDataString(payload);
        frameLength = WT440XH::FRAME_LENGTH;
      }

      // Try UniversalSensor
      if(data.length() == 0 && UniversalSensor::IsValidDataRate(rfm->GetDataRate())) {
        data = UniversalSensor::GetFhemDataString(payload);
        frameLength = UniversalSensor::FRAME_LENGTH;
      }

      // Try CustomSensor
      if(data.length() == 0 && CustomSensor::IsValidDataRate(rfm->GetDataRate())) {
        data = CustomSensor::GetFhemDataString(payload);
        frameLength = CustomSensor::GetFrameLength(payload);
      }

      // Try PCA301
      if(pca301.IsInitialized() && data.length() == 0 && PCA301::IsValidDataRate(rfm->GetDataRate())) {
        data = pca301.GetFhemDataString(payload);
        frameLength = 12;
      }

      if(data.length() > 0) {
        result = true;

        String raw = "";
        for(int i = 0; i < frameLength; i++) {
          String bt = String(payload[i], HEX);
          bt = bt.length() == 1 ? ("0" + bt) : bt;
          raw += bt;
          raw += i + 1 < frameLength ? " " : "";
        }
        raw.toUpperCase();

        Dispatch(data, raw);
      }
    } 
  }

  delete[] payload;  // FIX 3: correct array delete
  return result;
}

void HandleDataRateToggle(RFMxx *rfm, unsigned long *lastToggle, unsigned long *dataRate) {
  if (rfm->ToggleInterval > 0) {
    // After about 50 days millis() will overflow to zero 
    if (millis() < *lastToggle) {
      *lastToggle = 0;
    }

    if (millis() > *lastToggle + rfm->ToggleInterval * 1000 && rfm->ToggleMode > 0) {
      // Bits 1: 17.241 kbps, 2 : 9.579 kbps, 4 : 8.842 kbps, 8 : 20.000 kbps

      HashMap<unsigned long, unsigned long, 4> dataRates;
      if (rfm->ToggleMode & 1) {
        dataRates.Put(17241, 17241);
      }
      if (rfm->ToggleMode & 2) {
        dataRates.Put(9579, 9579);
        if (dataRates.Size() > 0) {
          *dataRates.GetValuePointerAt(dataRates.Size() - 2) = 9579;
        }
      }
      if (rfm->ToggleMode & 4) {
        dataRates.Put(8842, 8842);
        if (dataRates.Size() > 0) {
          *dataRates.GetValuePointerAt(dataRates.Size() - 2) = 8842;
        }
      }
      if (rfm->ToggleMode & 8) {
        dataRates.Put(20000, 20000);
        if (dataRates.Size() > 0) {
          *dataRates.GetValuePointerAt(dataRates.Size() - 2) = 20000;
        }
      }
      *dataRates.GetValuePointerAt(dataRates.Size() - 1) = dataRates.GetKeyAt(0);

      *dataRate = dataRates.Get(rfm->GetDataRate(), 0);
      if (*dataRate == 0) {
        *dataRate = 17241;
      }

      SetDataRate(rfm, *dataRate);
      *lastToggle = millis();

    }
  }
}

void HandleProgressRequest(byte action, unsigned long offset, unsigned long maxValue, String message) {
  if (display.IsConnected()) {
    switch (action) {
    case 1:
      display.PushContent();
      display.ShowProgress(maxValue, message);
      break;

    case 2:
      display.MoveProgress(offset);
      break;

    case 3:
      display.PopContent();
      break;

    default:
      break;
    }

  }

}

void updateNetworkTime() {
  configTime(MY_TZ, MY_NTP_SERVER);
  logger.println(F("NTP: configTime gesetzt"));
}

void TryConnectWIFI(String ctSSID, String ctPass, byte nbr, uint16_t timeout) {
  if (ctSSID.length() > 0 && ctSSID != "---") {
    unsigned long startMillis = millis();
    int numSsid = -1;
    uint8_t *thisBssid;

    WiFi.setAutoReconnect(true);
    // WiFi.persistent(true);

    numSsid = scanWifi(ctSSID);

    logger.println("");
    logger.print(F("SSID ") + ctSSID + F(" search result - #: ") + String(numSsid)
      + F(", ch: ") + String(channel) + F(", rssi: ") + String(rssi) + F(", bssid: "));
    LogBssid(bssid);;

    if (rssi != -999) {
      logger.println("Connecting to bssid");
      WiFi.begin(ctSSID.c_str(), ctPass.c_str(), channel, bssid, false);
    }
    else {
      logger.println("Connecting to ssid");
      WiFi.begin(ctSSID.c_str(), ctPass.c_str());
    }

    logger.print(F("Connected to ch: ") + String(WiFi.channel())
      + F(", rssi: ") + String(WiFi.RSSI()) + F(", bssid: "));
    LogBssid(WiFi.BSSID());

    logger.println("Connect " + String(timeout) + " seconds to an AP (SSID " + String(nbr) + ")");
    esp.SwitchLed(true, true);
    if (display.IsConnected()) {
      display.ShowProgress(timeout * 2, "Connect WiFi (" + String(nbr) + ")");
    }
    if (nextion.IsConnected()) {
      nextion.ShowProgress(timeout * 2, "Connect WiFi (" + String(nbr) + ")");
    }
    uint16_t retryCounter = 0;
    while (retryCounter < timeout * 2 && WiFi.status() != WL_CONNECTED) {
      retryCounter++;
      delay(500);
      ESP.wdtFeed();  // FIX 2: prevent HW-WDT reset during long WiFi connect timeout
      yield();
      logger.print(".");
      esp.SwitchLed(retryCounter % 2 == 0, true);
      if (display.IsConnected()) {
        display.MoveProgress();
      }
      if (nextion.IsConnected()) {
        nextion.MoveProgress();
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      stateManager.SetWiFiConnectTime((millis() - startMillis) / 1000.0);
    }
    

    esp.SwitchLed(false, true);
    if (display.IsConnected()) {
      display.HideProgress();
    }
    if (nextion.IsConnected()) {
      ////nextion.HideProgress();
    }
    
  }

}

static bool StartWifi(Settings &settings) {
  espconn_tcp_set_max_con(10);
  WiFiManager *wm = new WiFiManager();

  String hostName = settings.Get("HostName", "LaCrosseGateway");
  String apName   = hostName + "_" + String(ESP.getChipId(), HEX);

  wifiManager.setAPCallback([](WiFiManager* mgr) {
    logger.println(F("*** WiFiManager: Konfigurationsportal gestartet ***"));
    logger.println("SSID: " + mgr->getConfigPortalSSID());
    logger.println(F("Oeffne http://192.168.4.1 im Browser"));
    if (display.IsConnected()) {
      display.Print("WiFi Setup", DisplayArea_Line1, OLED::Alignments::Center);
      display.Print("192.168.4.1", DisplayArea_Line2, OLED::Alignments::Center);
    }
  });

  wifiManager.setSaveConfigCallback([]() {
    logger.println(F("*** WiFiManager: WLAN-Daten gespeichert ***"));
  });

  String ctSSID = settings.Get("ctSSID", "---");
  ctSSID.trim();
  bool isFirstSetup = (ctSSID.length() == 0 || ctSSID == "---");
  if (!isFirstSetup) {
    wifiManager.setConfigPortalTimeout(180);
  }

  esp.SwitchLed(true, true);

  bool connected = wm->autoConnect(apName.c_str());
  delete wm;

  if (!connected) {
    // FIX 4: Do not restart if no SSID configured yet – keep AP running for first-time setup
    String storedSSID = settings.Get("ctSSID", "---");
    storedSSID.trim();
    bool hasStoredSSID = (storedSSID.length() > 0 && storedSSID != "---");
    if (hasStoredSSID) {
      logger.println(F("*** WiFiManager: Verbindung fehlgeschlagen, starte neu ***"));
      delay(1000);
      ESP.restart();
    } else {
      logger.println(F("*** WiFiManager: Kein WLAN konfiguriert, AP laeuft weiter ***"));
    }
    return false;
  }

  // SSID/Pass in Settings sichern
  String newSSID = WiFi.SSID();
  String newPass = WiFi.psk();
  if (newSSID.length() > 0) {
    settings.Add("ctSSID", newSSID);
    settings.Add("ctPASS", newPass);
    settings.Write();
    logger.println("WiFiManager: SSID '" + newSSID + "' gespeichert");
  }

  // Hostname
  String hn = settings.Get("HostName", "LaCrosseGateway");
  WiFi.hostname(hn);
  logger.print("HostName is: ");
  logger.println(hn);
  stateManager.SetHostname(hn);

  // Statische IP
  String staticIP   = settings.Get("staticIP",   "");
  String staticMask = settings.Get("staticMask",  "");
  String staticGW   = settings.Get("staticGW",    "");
  String staticDNS  = settings.Get("staticDNS",   "");
  bool useStaticIP = (staticIP.length() >= 7 && staticMask.length() >= 7 && staticGW.length() >= 7);
  if (!useStaticIP) {
    logger.println("Using DHCP");
  } else {
    logger.println("Using static IP: " + staticIP + " / " + staticMask + " GW " + staticGW);
    IPAddress dnsIP = staticDNS.length() >= 7
      ? HTML::IPAddressFromString(staticDNS)
      : HTML::IPAddressFromString(staticGW);
    WiFi.config(
      HTML::IPAddressFromString(staticIP),
      HTML::IPAddressFromString(staticGW),
      HTML::IPAddressFromString(staticMask),
      dnsIP
    );
  }

  logger.println();
  logger.println("connected :-)");
  logger.print("IP: ");
  logger.println(WiFi.localIP().toString());

  if (display.IsConnected()) {
    display.Print("LGW V" + stateManager.GetVersion(), DisplayArea_Line1, OLED::Alignments::Center);
    display.Print(WiFi.localIP().toString(), DisplayArea_Line2, OLED::Alignments::Center);
    display.SetWifiFlag(true);
  }
  if (nextion.IsConnected()) {
    nextion.Print("LGW V" + stateManager.GetVersion() + "\r\n" + WiFi.localIP().toString());
  }

  // mDNS
  if (settings.GetBool("UseMDNS")) {
    logger.println("Starting MDNS");
    MDNS.begin("esp8266-ota", WiFi.localIP());
    MDNS.addService("arduino", "tcp", OTA_PORT);
    MDNS.addService("http", "tcp", FRONTEND_PORT);
  }

  // OTA
  logger.println("Starting OTA");
  ota.Begin(frontend.WebServer());

  // DataPorts
  uint16_t p1 = settings.GetInt("DataPort1", 81);
  uint16_t p2 = settings.GetInt("DataPort2", 0);
  uint16_t p3 = settings.GetInt("DataPort3", 0);
  if (p1 > 0) {
    logger.print("Starting data port 1 on "); logger.println(p1);
    dataPort1.Begin(p1);
    dataPort1.SetLogItemCallback([](String logItem){ logger.println(logItem); });
  }
  if (p2 > 0) {
    logger.print("Starting data port 2 on "); logger.println(p2);
    dataPort2.Begin(p2);
    dataPort2.SetLogItemCallback([](String logItem){ logger.println(logItem); });
  }
  if (p3 > 0) {
    logger.print("Starting data port 3 on "); logger.println(p3);
    dataPort3.Begin(p3);
    dataPort3.SetLogItemCallback([](String logItem){ logger.println(logItem); });
  }

  // Serial Bridges
  if (useSerialBridge && sc16is750.IsConnected()) {
    int sbPort = settings.GetInt("SerialBridgePort", 0);
    unsigned long sbBaud = settings.GetUnsignedLong("SerialBridgeBaud", 57600ul);
    if (sbPort > 0 && sbBaud > 0) {
      serialBridge.SetProgressCallback([](byte action, unsigned long cur, unsigned long max, String msg) {
        HandleProgressRequest(action, cur, max, msg);
      });
      logger.println("Starting serial bridge on port " + String(sbPort) + " with " + sbBaud + " baud");
      serialBridge.Begin(sbPort, frontend.WebServer());
      serialBridge.SetBaudrate(sbBaud);
    }
  }
  if (useSerialBridge2 && sc16is750_2.IsConnected()) {
    int sbPort = settings.GetInt("SerialBridge2Port", 0);
    unsigned long sbBaud = settings.GetUnsignedLong("SerialBridge2Baud", 57600ul);
    if (sbPort > 0 && sbBaud > 0) {
      serialBridge2.SetProgressCallback([](byte action, unsigned long cur, unsigned long max, String msg) {
        HandleProgressRequest(action, cur, max, msg);
      });
      logger.println("Starting serial bridge 2 on port " + String(sbPort) + " with " + sbBaud + " baud");
      serialBridge2.Begin(sbPort, frontend.WebServer());
      serialBridge2.SetBaudrate(sbBaud);
    }
  }

  USE_WIFI = 1;
  return true;
}

static void StopWifi() {
  WiFi.mode(WiFiMode::WIFI_OFF);
  USE_WIFI = 0;
}

String CommandHandler(String command) {
  command = HTML::UTF8ToASCII(command);
  for (uint16_t i = 0; i < command.length(); i++) {
    HandleSerialPort(command.charAt(i));
  }
  stateManager.ResetLastFullKVPUpdate();
  return "";
}

void CheckRFM(byte nbr, RFMxx *rfm, unsigned long dataRate, Settings *settings, long freqKHz = 0) {
  if (!rfm->IsConnected()) return;

  if (rfm->GetRadioType() == RFMxx::RadioType::RFM95W) {
    rfm->InitializeLoRa();
    rfm->SetFrequency(freqKHz > 0 ? freqKHz : 868200);

    // Spreading Factor Lookup
    static const struct { const char *name; RFMxx::SpreadingFactor sf; } sfTable[] = {
      {"SF6",  RFMxx::SpreadingFactor::SF6 },
      {"SF7",  RFMxx::SpreadingFactor::SF7 },
      {"SF8",  RFMxx::SpreadingFactor::SF8 },
      {"SF9",  RFMxx::SpreadingFactor::SF9 },
      {"SF10", RFMxx::SpreadingFactor::SF10},
      {"SF11", RFMxx::SpreadingFactor::SF11},
      {"SF12", RFMxx::SpreadingFactor::SF12}
    };
    String sfValue = settings->Get("SF95", "SF7");
    for (auto &e : sfTable) {
      if (sfValue == e.name) { rfm->SetSpreadingFactor(e.sf); break; }
    }

    // Bandwidth Lookup
    static const struct { const char *name; RFMxx::Bandwidth bw; } bwTable[] = {
      {"7.8",   RFMxx::Bandwidth::BW7_8  },
      {"10.4",  RFMxx::Bandwidth::BW10_4 },
      {"15.6",  RFMxx::Bandwidth::BW15_6 },
      {"20.8",  RFMxx::Bandwidth::BW20_8 },
      {"31.25", RFMxx::Bandwidth::BW31_25},
      {"41.7",  RFMxx::Bandwidth::BW41_7 },
      {"62.6",  RFMxx::Bandwidth::BW62_5 },
      {"125",   RFMxx::Bandwidth::BW125  },
      {"250",   RFMxx::Bandwidth::BW250  },
      {"500",   RFMxx::Bandwidth::BW500  }
    };
    String bwValue = settings->Get("BW95", "125");
    for (auto &e : bwTable) {
      if (bwValue == e.name) { rfm->SetBandwidth(e.bw); break; }
    }

  } else {
    rfm->InitializeLaCrosse();
    rfm->SetFrequency(freqKHz > 0 ? freqKHz : INITIAL_FREQ);
    SetDataRate(rfm, dataRate);
  }

  rfm->EnableReceiver(true);
  logger.print(F("Radio #"));
  logger.print(String(nbr));
  logger.print(F(" found: "));
  logger.println(rfm->GetRadioName());
}

bool IsMqttConfigured(Settings& settings) {
  String server = settings.Get("serverIpMqtt", "");
  String topic  = settings.Get("topic", "");
  server.trim();
  topic.trim();
  return server.length() > 0 && server != "---" && topic.length() > 0;
}


void setup(void) {
  Serial.begin(57600);
  delay(1000);
  Serial.println();

  logger.SetBufferSize(40);

  logger.Clear();
  logger.println("***CLEARLOG***");
  
  SetDebugMode(DEBUG);
  
  Settings settings;
  settings.Read(&logger);
  LoadRadioSettings(settings);
  
  pinMode(D7, INPUT);
  if (digitalRead(D7)) {
    USE_WIFI = false;
  }
  pinMode(D7, OUTPUT);
  
  if (!settings.GetBool("UseWiFi", true)) {
    USE_WIFI = false;
  }
  if (!USE_WIFI) {
    logger.Disable();
    esp.Blink(20);
  }
  

  logger.print(PROGNAME);
  logger.print(" V");
  logger.println(PROGVERS);
  logger.print("Free heap: ");
  logger.print(ESP.getFreeHeap());
  logger.print(" Flash size: ");
  logger.print(ESP.getFlashChipSizeByChipId());
  logger.print(" Core: ");
  logger.print(ESP.getCoreVersion());
  logger.print(" SDK: ");
  logger.println(ESP.getSdkVersion());
  logger.print("Reset: ");
  logger.println(ESP.getResetReason());
  logger.println(ESP.getResetInfo());
 
  ownSensors.SetLogItemCallback([](String logItem) {
    logger.println(logItem, Logger::LogType::ONLYSYS);
  });
  ownSensors.SetID(settings.GetByte("ISID", 0));

  useSerialBridge = settings.GetInt("SerialBridgePort", 0) > 0;
  useSerialBridge2 = settings.GetInt("SerialBridge2Port", 0) > 0;

  unsigned long i2cClock = 400000ul;
  
  SPI.begin();

  logger.print("Starting I2C with ");
  logger.print(String(i2cClock/1000));
  logger.println(" kHz");
  Wire.begin();
  Wire.setClock(i2cClock);
  Wire.setClockStretchLimit(1000);
  
  stateManager.Begin(PROGVERS, settings.Get("KVIdentity", String(ESP.getChipId())));
  
  dataPort1.SetConnectCallback([](bool isConnected) {
    dataPort1Connected = isConnected;
    if (display.IsConnected()) {
      display.SetFhemFlag(isConnected);
    }
  });
  dataPort2.SetConnectCallback([](bool isConnected) {
    dataPort2Connected = isConnected;
    if (display.IsConnected()) {
      display.SetFhemFlag(isConnected);
    }
  });
  dataPort3.SetConnectCallback([](bool isConnected) {
    dataPort3Connected = isConnected;
    if (display.IsConnected()) {
      display.SetFhemFlag(isConnected);
    }
  });
  serialBridge.SetConnectCallback([](bool isConnected) {
    bridge1Connected = isConnected;
    if (display.IsConnected()) {
      display.SetAddonFlag(isConnected);
    }
  });
  serialBridge2.SetConnectCallback([](bool isConnected) {
    bridge3Connected = isConnected;
    if (display.IsConnected()) {
      display.SetAddonFlag(isConnected);
    }
  });

  int displayStartMode = -3;
  String dsmSetting = settings.Get("oledStart", "on");
  if (dsmSetting == "on") {
    displayStartMode = -1;
  }
  else if (dsmSetting == "off") {
    displayStartMode = 0;
  }
  else {
    displayStartMode = settings.GetInt("oledStart", 1);
  }

  if(display.Begin(&stateManager, displayStartMode, settings.GetBool("oled13", false) ? OLED::Controllers::SH1106 : OLED::Controllers::SSD1306)) {
    logger.println("OLED found");
    delay(500);
  }
  
  delay(250);
  sc16is750.Begin(57600, false);
  if (sc16is750.IsConnected()) {
    logger.println("SC16IS750 (0x90) found");
 
    if (!useSerialBridge) {
      subProcessor.SetLogItemCallback([](String logItem, bool newLine) {
        if (newLine) {
          logger.println(logItem);
        }
        else {
          logger.print(logItem);
        }
      });

      subProcessor.SetProgressCallback([](byte action, unsigned long currentValue, unsigned long maxValue, String message) {
        HandleProgressRequest(action, currentValue, maxValue, message);
      });

      logger.println("SubProcessor vorgemerkt");
    }
    
    serialBridge.SetLogItemCallback([](String logItem, bool newLine) {
      if (newLine) {
        logger.println(logItem);
      }
      else {
        logger.print(logItem);
      }
    });
  }

  if (sc16is750.IsConnected()) {
    sc16is750.PinMode(0, OUTPUT);
    sc16is750.DigitalWrite(0, HIGH);
    sc16is750.PinMode(1, OUTPUT);
    sc16is750.DigitalWrite(1, HIGH);
    alarm.Begin();
  }

  sc16is750_2.Begin(57600, false);
  if (sc16is750_2.IsConnected()) {
    logger.println("SC16IS750 (0x92) found");
    if (!useSerialBridge2) {
      subProcessor2.SetLogItemCallback([](String logItem, bool newLine) {
        if (newLine) {
          logger.println(logItem);
        }
        else {
          logger.print(logItem);
        }
      });

      subProcessor2.SetProgressCallback([](byte action, unsigned long currentValue, unsigned long maxValue, String message) {
        HandleProgressRequest(action, currentValue, maxValue, message);
      });

      subProcessor2.Begin(frontend.WebServer());
      logger.println("SubProcessor2 Reset");
      subProcessor2.Reset();
    }
    
    serialBridge2.SetLogItemCallback([](String logItem, bool newLine) {
      if (newLine) {
        logger.println(logItem);
      }
      else {
        logger.print(logItem);
      }
    });
  }

  rfms[0] = &rfm1;
  rfms[1] = &rfm2;
  rfms[2] = &rfm3;
  rfms[3] = &rfm4;
  rfms[4] = &rfm5;

  rfm1.Begin();  
  rfm2.Begin();
  rfm3.Begin();
  if (sc16is750.IsConnected()) {
    rfm4.Begin();
    rfm5.Begin();
  }

for (byte i = 0; i < 5; i++) {
  rfms[i]->ToggleMode     = g_radio[i].enabled ? g_radio[i].toggleMask      : 0;
  rfms[i]->ToggleInterval = g_radio[i].enabled ? g_radio[i].toggleIntervalS : 0;
}

  ownSensors.TryInitialize(!rfm3.IsConnected(), !rfm2.IsConnected() && !rfm3.IsConnected(), settings.GetBool("PRD", false));

  int altitude = settings.GetInt("Altitude", 0);
  logger.println("Configured altitude: " + String(altitude));
  ownSensors.SetAltitudeAboveSeaLevel(altitude);

  ownSensors.SetCorrections(settings.Get("CorrT", "0"), settings.Get("CorrH", "0"));

  if (ownSensors.HasBMP180()) {
    logger.println("BMP180 found");
  }
  if (ownSensors.HasBMP280()) {
    logger.println("BMP280 found");
  }
  if (ownSensors.HasBME280()) {
    logger.println("BME280 found");
  }
  if (ownSensors.HasBME680()) {
    logger.println("BME680 found");
  }
  if (ownSensors.HasDHT22()) {
    logger.println("DHT22 found");
  }
  if (ownSensors.HasLM75()) {
    logger.println("LM75 found");
  }
  if (ownSensors.HasSHT75()) {
    logger.println("SHT75 found");
  }
  if(ownSensors.HasBH1750()) {
    logger.println("BH1750 found");
  }

  KVINTERVAL = settings.GetInt("KVInterval", KVINTERVAL);
  OSINTERVAL = settings.GetInt("ISIV", OSINTERVAL) * 1000;
  
  if (settings.Get("Flags", "").indexOf("NO_FRONTEND_LOG") != -1) {
    logger.Disable();
  }

  esp.EnableLED(ENABLE_ACTIVITY_LED);
  lastToggleR1 = millis();
  lastToggleR2 = millis();

  esp.Blink(5, true);
  
  accessPoint.SetLogItemCallback([](String logItem){
    logger.println("AccessPoint: " + logItem);
  });

  if (settings.Get("Flags", "").indexOf("LOG.PCA301") != -1) {
    pca301.EnableLogging(true);
  }
  pca301.SetLogItemCallback([](String logItem){
    logger.println("PCA301: "+ logItem);
  });

  if (digitalPorts.Begin(
    [](String data) {
    Dispatch(data);
  },
    [](String command) {
    CommandHandler(command);
  },
    settings)) {
    logger.println("MCP23008 found");
  }
 
  int softSerialBridgePort = settings.GetInt("SSBridgePort", 0);
  if (USE_WIFI && !rfm2.IsConnected() && !rfm3.IsConnected() && !ownSensors.HasSHT75() && softSerialBridgePort > 0) {
    unsigned long softSerialBridgeBaud = settings.GetUnsignedLong("SSBridgeBaud", 57600);
    softSerialBridge.SetProgressCallback([](byte action, unsigned long currentValue, unsigned long maxValue, String message) {
      HandleProgressRequest(action, currentValue, maxValue, message);
    });

    softSerialBridge.SetConnectCallback([](bool isConnected) {
      bridge2Connected = isConnected;
      if (display.IsConnected()) {
        display.SetAddonFlag(isConnected);
      }

      logger.println("SoftSerialBridge Connect: " + String(isConnected));
    });

    logger.println("Soft serial bridge port:" + String(softSerialBridgePort) + " baud:" + softSerialBridgeBaud);
    softSerialBridge.Begin(softSerialBridgePort, softSerialBridgeBaud, frontend.WebServer());

    if (settings.GetBool("IsNextion", false)) {
      nextion.SetProgressCallback([](byte action, unsigned long currentValue, unsigned long maxValue, String message) {
        HandleProgressRequest(action, currentValue, maxValue, message);
      });
      if (nextion.Begin(frontend.WebServer(), softSerialBridge.GetSoftSerial(), softSerialBridgeBaud, settings.GetBool("AddUnits", false), settings.GetBool("PRD", false))) {
        logger.println("Nextion initialized");
      }
    }

  }

  logger.println("Searching RFMs and Sensors");
auto parseDataRateLong = [](const String &s) -> unsigned long {
  if (s == "17.241") return 17241ul;
  if (s == "9.579")  return 9579ul;
  if (s == "8.842")  return 8842ul;
  if (s == "6.631")  return 6631ul;
  if (s == "4.800")  return 4800ul;
  return 17241ul;
};

for (byte i = 0; i < 5; i++) {
  unsigned long dr   = g_radio[i].enabled ? parseDataRateLong(g_radio[i].fixedDataRate) : 17241ul;
  long          freq = g_radio[i].enabled ? g_radio[i].freqKHz : 0;
  CheckRFM(i + 1, rfms[i], dr, &settings, freq);
}

DATA_RATE_R1 = g_radio[0].enabled ? parseDataRateLong(g_radio[0].fixedDataRate) : 17241ul;
DATA_RATE_R2 = g_radio[1].enabled ? parseDataRateLong(g_radio[1].fixedDataRate) : 9579ul;
DATA_RATE_R3 = g_radio[2].enabled ? parseDataRateLong(g_radio[2].fixedDataRate) : 8842ul;
DATA_RATE_R4 = g_radio[3].enabled ? parseDataRateLong(g_radio[3].fixedDataRate) : 17241ul;
DATA_RATE_R5 = g_radio[4].enabled ? parseDataRateLong(g_radio[4].fixedDataRate) : 17241ul;


  // Start wifi
  if (USE_WIFI) {
    int startupDelay = settings.GetInt("StartupDelay", 0);
    if (startupDelay > 0) {
      logger.println("Startup delay: " + String(startupDelay) + " seconds");
      for (int _sd = 0; _sd < startupDelay * 2; _sd++) { delay(500); ESP.wdtFeed(); yield(); }
    }
    logger.println("Starting wifi");
    bool wifiConnected = StartWifi(settings);

    if (sc16is750.IsConnected() && !useSerialBridge) {
      subProcessor.Begin(frontend.WebServer());
      logger.println("SubProcessor Reset");
      subProcessor.Reset();
    }
    if (sc16is750_2.IsConnected() && !useSerialBridge2) {
      subProcessor2.Begin(frontend.WebServer());
      logger.println("SubProcessor2 Reset");
      subProcessor2.Reset();
    }

    if (!wifiConnected) {
      return;
    }

    configTime(MY_TZ, MY_NTP_SERVER);
    logger.println(F("NTP konfiguriert, warte auf Synchronisation..."));
    time_t t = 0;
    for (int i = 0; i < 20 && t < 100000; i++) { delay(500); ESP.wdtFeed(); yield(); t = time(nullptr); }
    logger.println(F("NTP sync: ") + getLocalTimeString());
  }
  else {
    WiFi.mode(WiFiMode::WIFI_OFF);
  }
  
  if(settings.GetBool("SendAnalog")) {
    analogPort.TryInitialize(settings.GetInt("UAnalog1023", 1000));
  }


  if (display.IsConnected()) {
    display.Command("mode=s");
    ownSensors.Measure();
    lastSensorMeasurement = millis();
    display.Handle(ownSensors.GetDataFrame(), stateManager.GetFramesPerMinute(), stateManager.GetVersion(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED);
  
    String oledMode = settings.Get("oledMode", "");
    if (oledMode.length() > 0) {
      display.Command("mode=" + oledMode);
    }
  }

  if (nextion.IsConnected()) {
    delay(1000);
    nextion.SendCommand("page LGW#main");
  }

  if (!rfm2.IsConnected() && !ownSensors.HasSHT75() && !softSerialBridge.IsEnabled()) {
    pinMode(D4, OUTPUT);
    digitalWrite(D4, HIGH);
  }

  watchdog.Begin([](String message) {
    Dispatch(message);  
  });

#ifdef USE_MQTT_Pubsub
  mqttEnabled = IsMqttConfigured(settings);

  if (mqttEnabled) {
    strcpy(mqtt_server, settings.Get("serverIpMqtt", "").c_str());
    mqtt_port = atoi(settings.Get("serverPortMqtt", "1883").c_str());
    strcpy(mqtt_user, settings.Get("mqttUser", "").c_str());
    strcpy(mqtt_password, settings.Get("mqttPass", "").c_str());
    secBetweenPublish = atoi(settings.Get("pubInt", "20").c_str());
    mqttTopicBaseGW = settings.Get("topic", "esp8266_lacrossegateway");

    logger.println(F("MQTT aktiviert: ") + String(mqtt_server) + ":" + String(mqtt_port)
      + F(" Interval:") + String(secBetweenPublish) + F(" Topic:") + mqttTopicBaseGW);

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    delay(500);

    if (WiFi.status() == WL_CONNECTED) {
      reconnectMqtt();
    }
  } else {
    logger.println(F("MQTT deaktiviert: kein Server oder Topic konfiguriert"));
  }
#endif
  
  delay(1000);
  logger.println("Sending init String to FHEM");
  HandleCommandV();

  logger.println("Setup completely done");
  logger.SetBufferSize(10);
}

byte HandleDataReception() {
  byte receivedPackets = 0;
  for (int i = 0; i < 5; i++) {
    if (rfms[i]->IsConnected()) {
      rfms[i]->Receive();
      if (rfms[i]->PayloadIsReady() && HandleReceivedData(rfms[i]))
        receivedPackets++;
    }
  }
  return receivedPackets;
}

void HandleDataRate() {
  static unsigned long *lastToggles[5] = { &lastToggleR1, &lastToggleR2, &lastToggleR3, &lastToggleR4, &lastToggleR5 };
  static unsigned long *dataRates[5]   = { &DATA_RATE_R1, &DATA_RATE_R2, &DATA_RATE_R3, &DATA_RATE_R4, &DATA_RATE_R5 };
  for (int i = 0; i < 5; i++) {
    if (rfms[i]->IsConnected())
      HandleDataRateToggle(rfms[i], lastToggles[i], dataRates[i]);
  }
}

// **********************************************************************
void loop(void) {
  static unsigned long timetry = 0;
  String mqttTopic;
  
  if (serialPortFlasher.IsUploading()) {
    if (Serial.available()) {
      serialPortFlasher.Add(Serial.read());
    }
    serialPortFlasher.Handle();
  }
  else {
    // Captive Portal DNS + Auto-Close pflegen
  accessPoint.Handle();

  stateManager.SetLoopStart();
#ifdef USE_MQTT_Pubsub
  if (mqttEnabled) {
    if ((WiFi.status() == WL_CONNECTED) && !client.connected() && (((millis() - timetry) > 2000) || !timetry)) {
      reconnectMqtt();
      timetry = millis();
    } else if ((WiFi.status() != WL_CONNECTED) && (((millis() - timetry) > 5000) || !timetry)) {
      timetry = millis();
      logger.println(F("Wifi disconnected!"));
    }
    client.loop();
  }
#endif
    // Handle the commands from the serial port
    // ----------------------------------------
    if (Serial.available()) {
      HandleSerialPort(Serial.read());
    }

    // Periodically send own sensor data
    // ---------------------------------
    ownSensors.Handle();
    if (millis() < lastSensorMeasurement) {
      lastSensorMeasurement = 0;
    }
    if(millis() < lastSensorTransmission) {
      lastSensorTransmission = 0;
    }

    if (millis() > lastSensorMeasurement + 10000) {
      ownSensors.Measure();
      lastSensorMeasurement = millis();
    }

    if(OSINTERVAL > 0) {
      if(millis() > lastSensorTransmission + OSINTERVAL) {
        String data = ownSensors.GetFhemDataString();

        if(WiFi.status() == WL_CONNECTED){
            updateNetworkTime();
        }
        
        // extract data from ownSensors and send via MQTT
        WSBase::Frame actOwnData;
        actOwnData = ownSensors.GetDataFrame();
        logger.print("Current Temperature: ");
        logger.println(actOwnData.Temperature);
        logger.print("Current Humidity: ");
        logger.println(actOwnData.Humidity);

        String mqttTopicBase = "esp8266_lacrossegateway";

        if(mqttEnabled && client.connected() && (WiFi.status() == WL_CONNECTED)){
          logger.println("Publishing sensor data...");
          //publish rssi
          ltoa(WiFi.RSSI(), bufData, 10);
          mqttTopic = mqttTopicBaseGW + "/rssi";
          mqttTopic.toCharArray(bufTopic, 100);
          client.publish(bufTopic, bufData);

          sprintf (bufData, "%f", actOwnData.Temperature);
          mqttTopic = mqttTopicBaseGW + "/temp";
          mqttTopic.toCharArray(bufTopic, 100);
          client.publish(bufTopic, bufData);

          itoa(actOwnData.Humidity, bufData, 10)  ;
          mqttTopic = mqttTopicBaseGW + "/hum";
          mqttTopic.toCharArray(bufTopic, 100);
          client.publish(bufTopic, bufData);

          String timeNowLocal = getLocalTimeString();
          mqttTopic = mqttTopicBaseGW + "/timeStamp";
          mqttTopic.toCharArray(bufTopic, 100);
          timeNowLocal.toCharArray(bufData, 30);
          client.publish(bufTopic, bufData);        
            
        }else{
          logger.println("MQTT unconnected and or Wifi unconnected - skip to publish!");
        }



        
        if(data.length() > 0) {
          Dispatch(data);
        }
        lastSensorTransmission = millis();
      }
    }
 
    // Handle the data reception
    // -------------------------
    byte receivedPackets = HandleDataReception();

    // Handle PCA301
    if (pca301.IsInitialized()) {
      pca301.Handle();
    } 

    // Periodically send some info about us
    // ------------------------------------
    stateManager.Handle(receivedPackets);
    if (KVINTERVAL > 1) { 
      String kvData = stateManager.GetKVP(KVINTERVAL);
      if (kvData.length() > 0) {
        Dispatch(kvData);
      }
    }

    // Handle the data rate
    // --------------------
    HandleDataRate();

    if (USE_WIFI) {
      // Handle the web fronted and the access point
      // -------------------------------------------
      frontend.Handle();
      accessPoint.Handle();

      // Handle the ports for FHEM
      // -------------------------
      dataPort1.Handle(CommandHandler);
      dataPort2.Handle(CommandHandler);
      dataPort3.Handle(CommandHandler);
    }

    // Handle OTA
    // ----------
    if (WiFi.status() == WL_CONNECTED) {
      ota.Handle();
    }

    // Handle Alarm
    if (sc16is750.IsConnected()) {
      alarm.Handle();
    }

    // Handle DigitalPorts
    if (digitalPorts.IsConnected()) {
      digitalPorts.Handle();
    }

    if (sc16is750.IsConnected()) {
      // Handle SerialBridge
      // -------------------
      if (useSerialBridge) {
        serialBridge.Handle();
      }

      // Handle SubProcessor
      // -------------------
      if (!useSerialBridge) {
        String subData = subProcessor.Handle();
        if (subData.length() > 0) {
          Dispatch("OK VALUES " + subData.substring(3));
        }
      }
    }

    if (sc16is750_2.IsConnected()) {
      // Handle SerialBridge
      // -------------------
      if (useSerialBridge2) {
        serialBridge2.Handle();
      }

      // Handle SubProcessor
      // -------------------
      if (!useSerialBridge2) {
        String subData = subProcessor2.Handle();
        if (subData.length() > 0) {
          Dispatch("OK VALUES " + subData.substring(3));
        }
      }
    }

    // Handle soft serial bridge
    // -------------------------
    if (softSerialBridge.IsEnabled()) {
      softSerialBridge.Handle();
    }
    
    WSBase::Frame frame = ownSensors.GetDataFrame();
    
    // Handle Nextion
    // --------------
    if (nextion.IsConnected()) {
      nextion.Handle(frame, &stateManager, WiFi.RSSI(), dataPort1Connected || dataPort2Connected || dataPort3Connected, bridge1Connected, bridge2Connected);
    }

    // Handle OLED
    // -----------
    if (display.IsConnected()) {
      display.Handle(frame, stateManager.GetFramesPerMinute(), stateManager.GetVersion(), WiFi.RSSI(), WiFi.status() == WL_CONNECTED);
    }

    // Watchdog
    // --------
    watchdog.Handle();

    // Analogport
    // ----------
    String analogData;
    if (analogPort.IsEnabled()){
      analogData = analogPort.GetFhemDataString();
    }
    if (analogPort.IsEnabled() && analogData.length() > 0) {
      Dispatch(analogData);
    }

    stateManager.SetLoopEnd();

    if (!logIsCleared && millis() > 60000) {
      logIsCleared = true;
      logger.Clear();
    }

  }
}
