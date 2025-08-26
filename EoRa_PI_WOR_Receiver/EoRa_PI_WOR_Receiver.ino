/*
  EoRa Pi Foundation --Receiver code    08/26/2025  @ 03:54 EDT
  EoRa Pi (EoRa-S3-900TB from EbyeIoT.com) Project is aweb request based; remote, wireless load control.
  William Lucid Designed, Debugged, and Prompted collbative team members Claude, ChatGPT, Copilot, and Gemini
  everyone of the members contributed to a successfull, completed project!
  
  Complete, reliable, and repeatable cycles; LoRa, remote Load Controller project
  Removed INA226 Power Monitor 
*/

#include <RadioLib.h>
#define EoRa_PI_V1


#include "boards.h"
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>
#include <rom/rtc.h>
#include <driver/rtc_io.h>
#include <Ticker.h>
#include "eora_s3_power_mgmt.h"

int symbols = 512;

// Check wake-up reason
esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

// EoRa Pi dev board pin configuration
#define RADIO_SCLK_PIN 5
#define RADIO_MISO_PIN 3
#define RADIO_MOSI_PIN 6
#define RADIO_CS_PIN 7
#define RADIO_DIO1_PIN 33  // SX1262 DIO1 pin
#define RADIO_BUSY_PIN 34
#define RADIO_RST_PIN 8
#define BOARD_LED 37

// Use existing WAKE_PIN definition
#define WAKE_PIN GPIO_NUM_16  // Inverted DIO1 signal for RTC wake-up

#define BOARD_LED 37
/** LED on level */
#define LED_ON HIGH
/** LED off level */
#define LED_OFF LOW

// === CONFIGURATION ===
#define USING_SX1262_868M

#if defined(USING_SX1268_433M)
uint8_t txPower = 14;
float radioFreq = 433.0;
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#elif defined(USING_SX1262_868M)
uint8_t txPower = 14;
float radioFreq = 915.0;
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif


#define COUNTDOWN_TIME_SECONDS 30  // Production: 2+ minutes, Testing: 30 seconds

SPIClass spi(SPI);
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);

// Define LoRa parameters (MUST MATCH TRANSMITTER EXACTLY!)
#define RF_FREQUENCY 915.0                                // MHz
#define TX_OUTPUT_POWER 14                                // dBm
#define LORA_BANDWIDTH 125.0                              // kHz
#define LORA_SPREADING_FACTOR 7                           // [SF7..SF12]
#define LORA_CODINGRATE 7                                 // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8] -> RadioLib uses 7 for 4/7
#define LORA_PREAMBLE_LENGTH symbols                         // INCREASED TO MATCH TRANSMITTER!
#define LORA_SYNC_WORD RADIOLIB_SX126X_SYNC_WORD_PRIVATE  // LoRa sync word

#define DUTY_CYCLE_PARAMS 100, 4900  // Test 5 --(Best --2% Duty cycle  100, 4900)

// === PIN DEFINITIONS ===
#define SDA  12
#define SCL  47

#define TRIGGER 15     // KY002S bi-stable switch control
#define KY002S_PIN 38  // KY002S switch state readback

#define ALERT  39       // INA226 battery monitor alert

// Add this with your other RTC variables
RTC_DATA_ATTR String packetBuffer = "";
RTC_DATA_ATTR bool packetWaiting = false;

int task = 0;
String str;
String timestamp;

#define BUFFER_SIZE 512  // Define the payload size here

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) {
  // check if the interrupt is enabled
  if (!enableInterrupt) {
    return;
  }

  // we got a packet, set the flag
  receivedFlag = true;
}

volatile bool taskMgr = false;

void dischargeComplete() {
  taskMgr = true;
}

// Debug: Track wake-up source
String wakeupReason = "Unknown";

/** Print reset reason */
void print_reset_reason(RESET_REASON reason);

void triggerKY002S() {
  digitalWrite(TRIGGER, LOW);
  delay(100);
  digitalWrite(TRIGGER, HIGH);
}

void taskDispatcher(int task, String timestamp) {
  switch (task) {
    case 1:
      task = 0;
      radio.sleep();
      digitalWrite(BUILTIN_LED, HIGH);
      Serial.println("✅ Task 1:  Timer Started");
      Serial.println(timestamp);
      Serial.println("LoRa radio.sleep() called");
      triggerKY002S();  // Turn on load
      Serial.println("Load Activated");
      // Start timer
      wakeByTimer();
      break;

    case 2:
      // End discharge cycle and sleep
      Serial.println("✅ Task 2:  Timer Expired");
      radio.startReceiveDutyCycleAuto();
      triggerKY002S();  // Turn off load
      Serial.println("Load Deactivated");

      digitalWrite(BUILTIN_LED, LOW);

      goToSleep();
      break;

    case 3:
      Serial.println("✅ Wake on Radio");
      break;

    default:
      Serial.println("Unknown case");
      break;
  }
}

void parseString(String str) {
  int delimiterIndex = str.indexOf(',');
  if (delimiterIndex != -1) {
    task = str.substring(0, delimiterIndex).toInt();
    timestamp = str.substring(delimiterIndex + 1);

    Serial.printf("Task: %d, Time: %s\n", task, timestamp.c_str());

  }
}

void setupLoRa(){
  initBoard();
    // When the power is turned on, a delay is required.
    delay(1500);

    // initialize SX126x with default settings
    Serial.print(F("[SX126x] Initializing ... "));
    //int state = radio.begin(radioFreq);  //example
    int state = radio.begin(
    radioFreq,  // 915.0 MHz
    125.0,      // Bandwidth
    7,          // Spreading factor
    7,          // Coding rate
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    14,   // 14 dBm for good balance
    512,   // Preamble length
    0.0,  // No TCXO
    true  // LDO mode ON
  );
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
            ;
    }

    // set the function that will be called
    // when new packet is received
    radio.setDio1Action(setFlag);

    // start listening for LoRa packets
    Serial.print(F("[SX126x] Starting to listen ... "));
    //state = radio.startReceive();  //example
    // Set up the radio for duty cycle receiving
    state = radio.startReceiveDutyCycleAuto();
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true)
            ;
    }

    // if needed, 'listen' mode can be disabled by calling
    // any of the following methods:
    //
    // radio.standby()
    // radio.sleep()
    // radio.transmit();
    // radio.receive();
    // radio.readData();
    // radio.scanChannel();
}

void wakeByTimer(){

  // Set deep sleep timer for 30s test / 120s production
  esp_sleep_enable_timer_wakeup(30 * 1000000ULL); // 30 seconds in microseconds
  // or
  //esp_sleep_enable_timer_wakeup(120 * 1000000ULL); // 120 seconds

  // Go to deep sleep - ESP32 wakes itself up after timer
  esp_deep_sleep_start();

  //Turn off load

  //gotoSleep --external wake up by LoRa

}

void goToSleep(void) {

  

  Serial.println("=== PREPARING FOR DEEP SLEEP ===");
  Serial.printf("DIO1 pin state before sleep: %d\n", digitalRead(RADIO_DIO1_PIN));
  Serial.printf("Wake pin (GPIO16) state before sleep: %d\n", digitalRead(WAKE_PIN));
  
  // Set up the radio for duty cycle receiving
  radio.startReceiveDutyCycleAuto();

  Serial.println("Configuring RTC GPIO and deep sleep wake-up...");
  // Configure GPIO16 for RTC wake-up - using internal pull-down
  rtc_gpio_pulldown_en(WAKE_PIN);  // Internal pull-down on GPIO16

  // Setup deep sleep with wakeup by GPIO16 - RISING edge (buffered DIO1 signal)
  esp_sleep_enable_ext0_wakeup(WAKE_PIN, RISING);

  // Turn off LED before sleep
  digitalWrite(BOARD_LED, LED_OFF);

  Serial.println("✅ Going to deep sleep now...");
  Serial.println("Wake-up sources: DIO1 pin reroute");
  Serial.flush();  // Make sure all serial data is sent before sleep

  SPI.end();

  // Finally set ESP32 into sleep
  esp_deep_sleep_start();  
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);

  Serial.println(timestamp);

  bool fsok = LittleFS.begin(true);
  Serial.printf_P(PSTR("\nFS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  // === BASIC PIN SETUP ONLY ===
  pinMode(TRIGGER, OUTPUT);
  pinMode(KY002S_PIN, INPUT);

  triggerKY002S();
  
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
  
  Serial.print("CPU Frequency: ");
  Serial.print(getCpuFrequencyMhz());
  Serial.println(" MHz");
  
  // Power management optimizations
  eora_disable_wifi();
  eora_disable_bluetooth();
  eora_disable_adc();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER){
    setupLoRa();
    task = 2;
    taskDispatcher(task,timestamp);
  }
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Woke from LoRa packet");
    setupLoRa();
    task = 1;
    taskDispatcher(task, timestamp);
    return;
  }

  // 🔥 NEW: Check if this is a power-on reset (compile/upload/reset)
  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("Power-on reset - initializing and going to sleep");

    setupLoRa();
    return;

    Serial.println("Going to sleep - wake on first packet");
    goToSleep();
  }

  // This should rarely execute now
  Serial.println("Unknown wake reason - staying awake");
  // ... original cold boot code as fallback
}

void loop() {
  /// check if the flag is set
    if (receivedFlag)
    {
        // disable the interrupt service routine while
        // processing the data
        enableInterrupt = false;

        // reset flag
        receivedFlag = false;

        // you can read received data as an Arduino String
        String str;
        int state = radio.readData(str);

        // you can also read received data as byte array
        /*
          byte byteArr[8];
          int state = radio.readData(byteArr, 8);
        */

        if (state == RADIOLIB_ERR_NONE)
        {
            // packet was successfully received
            Serial.println(F("[SX126x] Received packet!"));

            // print data of the packet
            Serial.print(F("[SX126x] Data:\t\t"));
            Serial.println(str);
            parseString(str);
            taskDispatcher(task, timestamp);

            // print RSSI (Received Signal Strength Indicator)
            Serial.print(F("[SX126x] RSSI:\t\t"));
            Serial.print(radio.getRSSI());
            Serial.println(F(" dBm"));

            // print SNR (Signal-to-Noise Ratio)
            Serial.print(F("[SX126x] SNR:\t\t"));
            Serial.print(radio.getSNR());
            Serial.println(F(" dB"));
        }
        else if (state == RADIOLIB_ERR_CRC_MISMATCH)
        {
            // packet was received, but is malformed
            Serial.println(F("CRC error!"));
        }
        else
        {
            // some other error occurred
            Serial.print(F("failed, code "));
            Serial.println(state);
        }

        // put module back to listen mode
        //radio.startReceive();  //example
        // Set up the radio for duty cycle receiving
        //radio.startReceiveDutyCycleAuto();
        radio.startReceiveDutyCycleAuto();

        // we're ready to receive more packets,
        // enable interrupt service routine
        enableInterrupt = true;
    }
}

void print_reset_reason(RESET_REASON reason) {
  switch (reason) {
    case 1:
      Serial.println("POWERON_RESET");
      break;
    case 3:
      Serial.println("SW_RESET");
      break;
    case 4:
      Serial.println("OWDT_RESET");
      break;
    case 5:
      Serial.println("DEEPSLEEP_RESET");
      break;
    case 6:
      Serial.println("SDIO_RESET");
      break;
    case 7:
      Serial.println("TG0WDT_SYS_RESET");
      break;
    case 8:
      Serial.println("TG1WDT_SYS_RESET");
      break;
    case 9:
      Serial.println("RTCWDT_SYS_RESET");
      break;
    case 10:
      Serial.println("INTRUSION_RESET");
      break;
    case 11:
      Serial.println("TGWDT_CPU_RESET");
      break;
    case 12:
      Serial.println("SW_CPU_RESET");
      break;
    case 13:
      Serial.println("RTCWDT_CPU_RESET");
      break;
    case 14:
      Serial.println("EXT_CPU_RESET");
      break;
    case 15:
      Serial.println("RTCWDT_BROWN_OUT_RESET");
      break;
    case 16:
      Serial.println("RTCWDT_RTC_RESET");
      break;
    default:
      Serial.println("NO_MEAN");
  }
}
