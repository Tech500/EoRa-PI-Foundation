//Claude's EoRa PI_Transmitter.ino  08/02/2025 @ 01:18  
//New Ebyte, EoRa PI (EoRa-S3-900TB) LoRa development board 915.0 Mhz  
//WifiManager library + setup for WiFiManage tzpau (sp?)
//William Lucid in colboration with Copilot, ChatGPT, Gemini 07/16/2025 @ 09:39 EDT

//Ardino IDE:  ESP32S3 Dev Module 
//Board Manager:  2.0.18

//EoRa_PI_Transmitter
//"D:\Transmitter --Fixed_v6_Receiver\EoRa_PI_WOR_Transmitter\EoRa_PI_WOR_Transmitter.ino"

//Enable RadioLib debug output before anything else
//#define RADIOLIB_DEBUG

#include <RadioLib.h>
#define EoRa_PI_V1
#include "boards.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <time.h>
#include <Ticker.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

//#import "index7.h"  //HTML7 for web request

#define RADIOLIB_SX126X_SYNC_WORD_PUBLIC 0x34

///Are we currently connected?
boolean connected = false;

WiFiUDP udp;
// local port to listen for UDP packets
//const int udpPort = 1337;
char incomingPacket[255];
char replyPacket[] = "Hi there! Got the message :-)";
//Settings for NTP
const int udpPort = 1337;
//NTP Time Servers
const char * udpAddress1 = "pool.ntp.org";
const char * udpAddress2 = "time.nist.gov";

WiFiManager wm;

AsyncWebServer serverAsync(80);

String linkAddress = "192.168.12.154:80";  //Server ipAddress for web page HTML7 (index7.h)

#define USING_SX1262_868M

uint8_t txPower = 22;
float radioFreq = 915.0;
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);



#define TZ "EST+5EDT,M3.2.0/2,M11.1.0/2"

int DOW, MONTH, DATE, YEAR, HOUR, MINUTE, SECOND;

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

char strftime_buf[64];

String dtStamp(strftime_buf);

AsyncWebServer server(80);
Ticker countdownTicker;
bool countdownActive = false;

String getDateTime() {
  struct tm *ti;

  time_t tnow = time(nullptr);

  ti = localtime(&tnow);
  DOW = ti->tm_wday;
  YEAR = ti->tm_year + 1900;
  MONTH = ti->tm_mon + 1;
  DATE = ti->tm_mday;
  HOUR = ti->tm_hour;
  MINUTE = ti->tm_min;
  SECOND = ti->tm_sec;
  strftime(strftime_buf, sizeof(strftime_buf), "%a %m %d %Y  %H:%M:%S", localtime(&tnow));  //Removed %Z
  dtStamp = strftime_buf;
  dtStamp.replace(" ", "-");
  return (dtStamp);
}

String timestamp = " ";

void sendLoRaWOR(String payload) {
  Serial.println("=== TRANSMITTING LORA PACKET ===");
  Serial.println("Payload: " + payload);
  Serial.printf("Power: %d dBm\n", txPower);
  Serial.printf("Frequency: %.1f MHz\n", radioFreq);
  Serial.flush();
  
  // Add delay before transmission to ensure receiver is asleep
  Serial.println("Waiting 3 seconds before transmission...");
  delay(3000);
  
  // Send single packet instead of multiple rapid ones
  Serial.println("Transmitting now...");
  unsigned long startTime = millis();
  int state = radio.transmit(payload);
  unsigned long transmitTime = millis() - startTime;
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅ Packet transmitted successfully!");
    Serial.printf("Transmission time: %lu ms\n", transmitTime);
    Serial.printf("Packet length: %d bytes\n", payload.length());
    Serial.println("=== TRANSMISSION COMPLETE ===");
  } else {
    Serial.printf("❌ Transmission failed! Error code: %d\n", state);
    // Error handling...
  }
  
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while(!Serial){};

  WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  //wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("AutoConnectAP", "password");  // password protected ap

  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  initBoard();  // LoRa + SD + OLED

int state = radio.begin(
  radioFreq,  // 915.0 MHz as set earlier
  125.0,      // Bandwidth (default LoRa)
  9,          // Spreading factor
  7,          // Coding rate
  RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
  txPower,  // 22 dBm
  16,        // Preamble length
  0.0,      // No TCXO
  true      // 🔐 LDO mode ON
);

if (state == RADIOLIB_ERR_NONE) {
  Serial.println(F("LoRa init successful!"));
} else {
  Serial.printf("LoRa init failed, code %d\n", state);
  while (true);  // Halt on init failure
}

  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request) {
    getDateTime();
    String timestamp = dtStamp;  // Correct: update timestamp
    String option = "1";
    String payload = option + "," + timestamp;
    //String payload = option;
    sendLoRaWOR(payload);  // Send WOR-style with timestamp
    request->send(200, "text/plain", "Sent WOR payload: " + payload);
  });

  server.begin();
 
  configTime();
  
}

void loop() {
  //udp only send data when connected
  if (connected)
  {

    //Send a packet
    udp.beginPacket(udpAddress1, udpPort);
    udp.printf("Seconds since boot: %u", millis() / 1000);
    udp.endPacket();
  }
}

String processor7(const String &var) {  //Part of web request

  //index7.h

  if (var == F("LINK"))
    return linkAddress;

  return String();
}

void configTime()
{

  configTime(0, 0, udpAddress1, udpAddress2);
  setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 3);   // this sets TZ to Indianapolis, Indiana
  tzset();

  //udp only send data when connected
  if (connected)
  {

    //Send a packet
    udp.beginPacket(udpAddress1, udpPort);
    udp.printf("Seconds since boot: %u", millis() / 1000);
    udp.endPacket();
  }

  Serial.print("wait for first valid timestamp");

  while (time(nullptr) < 100000ul)
  {
    Serial.print(".");
    delay(5000);
  }

  Serial.println("\nSystem Time set\n");

  getDateTime();

  Serial.println(dtStamp);

}
