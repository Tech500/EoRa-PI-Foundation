// === PRODUCTION VERSION - CLEAN CODE FOR GITHUB ===

/*
LoRa Remote Switch Controller - Production Version
Hardware: ESP32-S3 + SX1262 LoRa + Dual 74HC04N Wake Circuit
Author: William Lucid: Collabative project with Claude, ChatGPT, Gemini, and Copilot  
Version: 118
Date: August 11, 2025

Features:
- Battery-powered LoRa remote switch control
- Deep sleep with LoRa packet wake-up
- High-power discharge load control (2+ minute cycles)
- Dual 74HC04N inverter wake circuit for reliable sleep/wake
- Power-optimized operation

Hardware Requirements:
- EoRa PI ESP32-S3 development board
- SX1262 LoRa radio (915MHz)
- Dual 74HC04N inverters for wake circuit
- KY002S bi-stable switch module
- High-power load (relay, contactor, etc.)

Circuit:
LoRa DIO1 → GPIO33 → 74HC04N #1 → 74HC04N #2 → GPIO16 → ESP32 Wake
*/

#define RADIOLIB_DEBUG  // Comment out for production

#include <RadioLib.h>
#define EoRa_PI_V1

#include "boards.h"
#include <LittleFS.h>
#include <Wire.h>
#include <Ticker.h>
#include <rom/rtc.h>
#include <driver/rtc_io.h>
#include "eora_s3_power_mgmt.h"

// === CONFIGURATION ===
#define USING_SX1262_868M
#define DISCHARGE_TIME_SECONDS 120  // Production: 2+ minutes, Testing: 60

#if defined(USING_SX1268_433M)
uint8_t txPower = 14;
float radioFreq = 433.0;
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#elif defined(USING_SX1262_868M)
uint8_t txPower = 14;
float radioFreq = 915.0;
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif

// === PIN DEFINITIONS ===
#define TRIGGER 15      // KY002S bi-stable switch control
#define KY002S_PIN 46   // KY002S switch state readback
#define ALERT 19        // INA226 battery monitor alert
#define WAKE_PIN 16     // Wake input from dual 74HC04N circuit

// === GLOBAL VARIABLES ===
volatile bool receivedFlag = false;
volatile bool interruptEnabled = true;
volatile bool taskMgr = false;
bool firstLoop = true;
int taskCode = 0;
String timestamp = "";

// === TIMERS ===
Ticker dischargeTimer;

// === DEBUG CONTROL ===
#define DEBUG_SERIAL 1  // Set to 0 for production, 1 for debug

#if DEBUG_SERIAL
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, ...)
#endif

// === INTERRUPT HANDLERS ===
void IRAM_ATTR setFlag(void) {
  if (!interruptEnabled) return;
  receivedFlag = true;
}

void IRAM_ATTR dischargeComplete(void) {
  taskMgr = true;
}

// === CORE FUNCTIONS ===

void triggerKY002S() {
  digitalWrite(TRIGGER, LOW);
  delay(100);
  digitalWrite(TRIGGER, HIGH);
}

void parseString(String str) {
  int delimiterIndex = str.indexOf(',');
  if (delimiterIndex != -1) {
    taskCode = str.substring(0, delimiterIndex).toInt();
    timestamp = str.substring(delimiterIndex + 1);
    
    DEBUG_PRINTF("Task: %d, Time: %s\n", taskCode, timestamp.c_str());
  }
}

void taskDispatcher(int task) {
  switch (task) {
    case 1:
      // Start high-power discharge cycle
      DEBUG_PRINTLN("Starting discharge cycle");
      
      triggerKY002S();  // Turn on load
      DEBUG_PRINTLN("Load activated");
      
      // Start discharge timer
      dischargeTimer.detach();
      dischargeTimer.once(DISCHARGE_TIME_SECONDS, dischargeComplete);
      DEBUG_PRINTF("Discharge timer: %d seconds\n", DISCHARGE_TIME_SECONDS);
      break;
      
    case 2:
      // End discharge cycle and sleep
      DEBUG_PRINTLN("Discharge complete - going to sleep");
      
      triggerKY002S();  // Turn off load
      DEBUG_PRINTLN("Load deactivated");
      
      goToSleep();
      break;
      
    default:
      DEBUG_PRINTF("Unknown task: %d\n", task);
      break;
  }
  
  taskCode = 0;  // Reset after processing
}

void setupLoRa() {
  initBoard();
  delay(1000);
  
  DEBUG_PRINTLN("Initializing LoRa...");
  
  int state = radio.begin(radioFreq, 125.0, 9, 7, 
                         RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 
                         txPower, 16, 0.0, true);
  
  if (state == RADIOLIB_ERR_NONE) {
    DEBUG_PRINTF("LoRa OK: %.1f MHz, %d dBm\n", radioFreq, txPower);
  } else {
    DEBUG_PRINTF("LoRa init failed: %d\n", state);
    while (true) delay(1000);  // Halt on LoRa failure
  }
  
  receivedFlag = false;
  interruptEnabled = false;
  
  radio.setDio1Action(setFlag);
  
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    DEBUG_PRINTLN("LoRa receiver ready");
    interruptEnabled = true;
  } else {
    DEBUG_PRINTF("Start receive failed: %d\n", state);
  }
}

void goToSleep() {
  // Stop all packet processing
  interruptEnabled = false;
  receivedFlag = false;
  
  // Sleep radio for power saving
  radio.sleep();
  delay(200);
  
  DEBUG_PRINTLN("Entering deep sleep...");
  DEBUG_FLUSH();
  
  // Configure wake circuit
  pinMode(33, INPUT_PULLUP);    // LoRa DIO1 input
  pinMode(WAKE_PIN, INPUT_PULLUP);  // Dual inverter output
  delay(100);
  
  // Debug GPIO states
  DEBUG_PRINTF("Sleep states: GPIO33=%d, GPIO16=%d\n", 
               digitalRead(33), digitalRead(WAKE_PIN));
  
  // Ensure proper sleep state (GPIO16 should be LOW for wake-on-HIGH)
  if (digitalRead(WAKE_PIN) == 1) {
    DEBUG_PRINTLN("Correcting sleep state...");
    pinMode(33, OUTPUT);
    digitalWrite(33, LOW);
    delay(100);
    pinMode(33, INPUT_PULLUP);
  }
  
  // Configure RTC wake-up
  rtc_gpio_init((gpio_num_t)WAKE_PIN);
  rtc_gpio_set_direction((gpio_num_t)WAKE_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en((gpio_num_t)WAKE_PIN);
  
  // Wake when LoRa packet makes GPIO16 HIGH
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, 1);
  
  DEBUG_PRINTLN("Sleeping...");
  DEBUG_FLUSH();
  delay(100);
  
  esp_deep_sleep_start();
}

// === SETUP ===
void setup() {
  #if DEBUG_SERIAL
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {};  // 3 second timeout for serial
  #endif
  
  DEBUG_PRINTLN("=== LoRa Remote Switch Controller ===");
  DEBUG_PRINTLN("Version 118 - Production Release");
  
  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    DEBUG_PRINTLN("Woke from LoRa packet");
    setupLoRa();
    return;  // Skip cold boot initialization
  }
  
  DEBUG_PRINTLN("Cold boot initialization");
  
  // Power management
  eora_disable_wifi();
  eora_disable_bluetooth();
  eora_disable_adc();
  
  // File system
  LittleFS.begin(true);
  
  // Pin configuration
  pinMode(TRIGGER, OUTPUT);
  pinMode(KY002S_PIN, INPUT);
  pinMode(ALERT, INPUT);
  pinMode(33, INPUT_PULLUP);      // LoRa DIO1
  pinMode(WAKE_PIN, INPUT_PULLUP); // Dual inverter output
  
  // Initialize switch state
  if (digitalRead(KY002S_PIN) < 1) {
    digitalWrite(TRIGGER, HIGH);
  }
  
  // Initialize LoRa
  setupLoRa();
  
  DEBUG_PRINTLN("System ready - listening for commands");
  
  // Cold boot listening period
  unsigned long startTime = millis();
  while (millis() - startTime < 30000) {  // 30 second listen window
    if (receivedFlag) {
      // Process startup packet
      receivedFlag = false;
      interruptEnabled = false;
      
      String str;
      int state = radio.readData(str);
      
      if (state == RADIOLIB_ERR_NONE && str.length() > 0) {
        DEBUG_PRINTF("Startup packet: %s\n", str.c_str());
        parseString(str);
        
        if (taskCode == 1 || taskCode == 2) {
          DEBUG_PRINTF("Processing task %d\n", taskCode);
          taskDispatcher(taskCode);
          return;  // Exit if task triggered
        }
      }
      
      radio.startReceive();
      interruptEnabled = true;
    }
    delay(100);
  }
  
  // No packets during startup - go to sleep
  DEBUG_PRINTLN("No startup packets - sleeping");
  goToSleep();
}

// === MAIN LOOP ===
void loop() {
  // Handle discharge timer expiration
  if (taskMgr) {
    taskMgr = false;
    taskCode = 2;
    taskDispatcher(taskCode);
    return;  // Will go to sleep, never returns
  }
  
  // Process received packets
  if (receivedFlag && interruptEnabled) {
    receivedFlag = false;
    interruptEnabled = false;
    
    DEBUG_PRINTLN("Processing packet...");
    
    String str;
    int state = radio.readData(str);
    
    if (state == RADIOLIB_ERR_NONE && str.length() > 0) {
      DEBUG_PRINTF("RX: %s (RSSI: %.1f)\n", str.c_str(), radio.getRSSI());
      parseString(str);
      taskDispatcher(taskCode);
    } else {
      DEBUG_PRINTLN("Invalid packet");
    }
    
    // Re-enable if not going to sleep
    if (taskCode != 2) {
      radio.startReceive();
      interruptEnabled = true;
    }
  }
  
  delay(100);  // Prevent tight loop
}
*/
