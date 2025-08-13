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

// IMPORTANT: Match receiver parameters exactly!
uint8_t txPower = 22;
float radioFreq = 915.0;
// Increased preamble length for duty cycle receivers - CRITICAL for wake-up!
#define LORA_PREAMBLE_LENGTH 512   // Increased from 16 to 64 for reliable duty cycle reception
// Make sure spreading factor matches receiver
#define LORA_SPREADING_FACTOR 7   // Changed from 9 to 7 to match receiver
#define LORA_BANDWIDTH 125.0      // Match receiver
#define LORA_CODINGRATE 7         // Match receiver

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
  MONTH = ti->tm_mon + 1;int option = 0;
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
  int state;
  unsigned long transmitTime = 0;
  Serial.println("=== STARTING TRANSMISSION ===");
  Serial.printf("Payload: %s\n", payload.c_str());
  Serial.printf("Payload length: %d bytes\n", payload.length());
  Serial.printf("Preamble length: %d symbols\n", LORA_PREAMBLE_LENGTH);
  Serial.printf("Spreading Factor: %d\n", LORA_SPREADING_FACTOR);

  // Send payload transmission
  unsigned long startTime = millis();
  state = radio.transmit(payload);
  transmitTime = millis() - startTime;
  delay(100);


  delay(500);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅ Packet transmitted successfully!");
    Serial.printf("Transmission time: %lu ms\n", transmitTime);
    Serial.printf("Data rate: %.2f bps\n", (payload.length() * 8.0) / (transmitTime / 1000.0));
    Serial.println("=== TRANSMISSION COMPLETE ===");
  } else {
    Serial.printf("❌ Transmission failed! Error code: %d\n", state);
    
    // Decode common error codes
    switch(state) {
      case RADIOLIB_ERR_PACKET_TOO_LONG:
        Serial.println("Error: Packet too long");
        break;
      case RADIOLIB_ERR_TX_TIMEOUT:
        Serial.println("Error: Transmission timeout");
        break;
      case RADIOLIB_ERR_CHIP_NOT_FOUND:
        Serial.println("Error: Radio chip not responding");
        break;
      default:
        Serial.println("Check RadioLib documentation for error code details");
    }
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
    Serial.println("connected...");
  }

  initBoard();  // LoRa + SD + OLED

  Serial.println("=== CONFIGURING TRANSMITTER FOR DUTY CYCLE RECEIVER ===");

  // CRITICAL: Parameters must match receiver exactly
  int state = radio.begin(
    radioFreq,                          // 915.0 MHz - Match receiver
    LORA_BANDWIDTH,                     // 125.0 kHz - Match receiver  
    LORA_SPREADING_FACTOR,              // 7 - Match receiver (was 9)
    LORA_CODINGRATE,                    // 7 (4/7) - Match receiver
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,  // Private sync word - Match receiver
    txPower,                            // 22 dBm
    LORA_PREAMBLE_LENGTH,               // 64 symbols - INCREASED for duty cycle
    0.0,                                // No TCXO (EoRa Pi uses XTAL)
    false                               // DC-DC mode (more efficient than LDO)
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("✅ LoRa transmitter initialized successfully!"));
    Serial.printf("Frequency: %.1f MHz\n", radioFreq);
    Serial.printf("Bandwidth: %.1f kHz\n", LORA_BANDWIDTH);
    Serial.printf("Spreading Factor: %d\n", LORA_SPREADING_FACTOR);
    Serial.printf("Coding Rate: 4/%d\n", LORA_CODINGRATE);
    Serial.printf("TX Power: %d dBm\n", txPower);
    Serial.printf("Preamble: %d symbols\n", LORA_PREAMBLE_LENGTH);
    Serial.println("=== READY FOR DUTY CYCLE COMMUNICATION ===");
  } else {
    Serial.printf("❌ LoRa init failed, code %d\n", state);
    while (true) delay(1000);  // Halt on init failure
  }

  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request) {
    getDateTime();
    timestamp = dtStamp;  // Correct: update timestamp
    String option = "1";
    String payload = "1," + timestamp;
    Serial.println("\n=== WEB REQUEST RECEIVED ===");
    Serial.printf("Sending WOR payload: %s\n", payload.c_str());
    sendLoRaWOR(payload);  // Send WOR-style with timestamp
    delay(500);
    sendLoRaWOR(payload);
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
