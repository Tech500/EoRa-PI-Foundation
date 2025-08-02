/*
  
 VERSION 42

 Arduino Ide 2.3.6 was used to compile, BOARD MANAGER 2.0.17!

   Based on Ebyte "utilites.h" pin assignments.  Coded by Claude with 
   Design and prompting by William Lucid 
      
   "Claude_Receiver_testing_Version_42.ino"   Filename on computer

   
*/

//#define RADIOLIB_DEBUG

#include <RadioLib.h>
#define EoRa_PI_V1

#include "boards.h"
#include <LittleFS.h>
#include <Wire.h>
#include <Ticker.h>
#include <rom/rtc.h>
#include <driver/rtc_io.h>
#include "eora_s3_power_mgmt.h"

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

#define TXD 48
#define RXD 16

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool interruptEnabled = true;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) {
  // check if the interrupt is enabled
  if (!interruptEnabled) {
    return;
  }
  // we got a packet, set the flag
  receivedFlag = true;
}

// --- Timer Setup ---
Ticker twoMinuteTicker;

int taskCode = 0;
String timestamp = "";

// --- Task Dispatcher ---
enum TaskOption {
  TASK_NONE = 0,
  TASK_MEASURE_POWER = 1,  // Triggered by LoRa
  TASK_TRIGGER_SENSOR = 2  // Triggered by timer
};

#define TRIGGER 8    //KY002S MOSFET Bi-Stable Switch
#define KY002S_PIN 46  //KY002S MOSFET Bi-Stable Switch --Read output
#define ALERT 19       //INA226 Battery Monitor

#define LATCH_CLEAR_PIN 8  // GPIO8 (not used with dual inverters)
#define WAKE_PIN 16        // GPIO16 for wake-up from dual inverters (Pin 14)

// Global variables for missing declarations
volatile bool alertFlag = false;
int activationCount = 0;

// Forward declarations
void triggerKY002S();

// Alert interrupt handler
void alert() {
  alertFlag = true;
}

void IRAM_ATTR isrCountTimer() {
  Serial.println("Timer expired - turning off switch and going to sleep");
  Serial.flush();
  goToSleep();
}

void dispatchTask(int taskCode, String timestamp) { // Fixed function signature
  if (taskCode == 1) {
    //Toggle Switch
    triggerKY002S();
    Serial.println("Turned on Switch");
    Serial.flush();
    
    twoMinuteTicker.detach();
    twoMinuteTicker.once(60.0, isrCountTimer);  // 60 second timer instead of 120
    Serial.println("Timer started - will sleep in 60 seconds");
    Serial.flush();
  }
}

void parseString(String str) {
  int delimiterIndex = str.indexOf(',');
  if (delimiterIndex != -1) {
    taskCode = str.substring(0, delimiterIndex).toInt();
    timestamp = str.substring(delimiterIndex + 1);

    dispatchTask(taskCode, timestamp);
    Serial.println("Parsed Task: " + String(taskCode));
    Serial.println("Timestamp: " + timestamp);
    Serial.flush();
  } else {
    // Handle case where no delimiter found - treat entire string as task code
    taskCode = str.toInt();
    if (taskCode > 0) {
      dispatchTask(taskCode, "");
      Serial.println("Parsed Task (no timestamp): " + String(taskCode));
      Serial.flush();
    }
  }
}

void triggerKY002S() {
  digitalWrite(TRIGGER, LOW);
  delay(100);
  digitalWrite(TRIGGER, HIGH);
}

void goToSleep() {
  triggerKY002S();  // Turn off switch
  Serial.println("Turned off switch");
  Serial.println("=== GOING TO SLEEP WITH DUAL INVERTER WAKE ===");
  Serial.flush();

  // Debug: Check current states
  Serial.printf("GPIO33 (DIO1): %d\n", digitalRead(33));
  Serial.printf("GPIO16 (Wake): %d\n", digitalRead(16));
  Serial.flush();

  // Configure GPIO16 for RTC wake-up (wake on LOW since pin is stuck HIGH)
  rtc_gpio_init(GPIO_NUM_16);
  rtc_gpio_set_direction(GPIO_NUM_16, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_16);  // Pull up, wake on LOW

  // Enable EXT0 wake-up on GPIO16 LOW (since it's stuck HIGH)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_16, 0);

  // Use ultra-low power radio sleep
  radio.sleep();

  Serial.println("Sleeping... Dual inverters will wake on next LoRa packet");
  Serial.flush();
  
  // Add delay to ensure serial output completes
  delay(100);
  
  esp_deep_sleep_start();
}

void clearJKLatch() {
  // Pulse CLR pin LOW to reset flip-flop (Q=0)
  digitalWrite(LATCH_CLEAR_PIN, LOW);
  delayMicroseconds(10);  // Brief pulse
  digitalWrite(LATCH_CLEAR_PIN, HIGH);

  Serial.println("J-K flip-flop cleared");
}

// Hardware test function - with timeout
void testHardwareRoute() {
  Serial.println("\n=== TESTING GPIO33 → GPIO12 HARDWARE ROUTE ===");
  
  // Test 1: Check initial states
  Serial.printf("Initial states - GPIO33: %d, GPIO12: %d\n", 
                digitalRead(33), digitalRead(12));
  
  // Test 2: Force LoRa into different states and monitor GPIO33
  Serial.println("Testing LoRa state changes...");
  
  radio.standby();
  delay(100);
  Serial.printf("Standby - GPIO33: %d, GPIO12: %d\n", 
                digitalRead(33), digitalRead(12));
  
  radio.startReceive();
  delay(100);
  Serial.printf("Receive - GPIO33: %d, GPIO12: %d\n", 
                digitalRead(33), digitalRead(12));
  
  // Test 3: Quick toggle test (reduced from 5 to 2 iterations)
  Serial.println("Attempting to trigger DIO1...");
  
  for (int i = 0; i < 2; i++) {  // Reduced iterations
    radio.standby();
    delay(50);
    radio.startReceive();
    delay(50);
    Serial.printf("Toggle %d - GPIO33: %d, GPIO12: %d\n", 
                  i+1, digitalRead(33), digitalRead(12));
  }
  
  Serial.println("=== HARDWARE ROUTE TEST COMPLETE ===\n");
  Serial.flush();  // Ensure output completes
}

void setupLoRa() {
  // Initialize board first
  initBoard();
  delay(2000);

  Serial.println("=== LoRa Setup with Dual Inverter Wake ===");

  int state = radio.begin(
    radioFreq,  // 915.0 MHz
    125.0,      // Bandwidth
    9,          // Spreading factor
    7,          // Coding rate
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    14,   // 14 dBm for good balance
    16,   // Preamble length
    0.0,  // No TCXO
    true  // LDO mode ON
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa initialized successfully!");
    Serial.printf("Radio frequency: %.1f MHz\n", radioFreq);
    Serial.printf("TX Power: %d dBm\n", txPower);
  } else {
    Serial.printf("LoRa init failed: %d\n", state);
    while (true) delay(1000);
  }

  radio.setDio1Action(setFlag);

  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("=== Receiver Ready with Dual Inverters ===");
  } else {
    Serial.printf("Start receive failed: %d\n", state);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {};

  // Configure J-K flip-flop clear pin
  pinMode(LATCH_CLEAR_PIN, OUTPUT);
  digitalWrite(LATCH_CLEAR_PIN, HIGH);  // CLR is active LOW, so keep HIGH

  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.printf("Wakeup cause: %d\n", wakeup_reason);

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("*** WOKE FROM TIMER - CHECKING FOR PACKETS ***");
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("*** WOKE FROM LORA PACKET VIA INVERTER CHAIN! ***");
  } else {
    Serial.println("Cold boot or other wake source");
  }

  // Test the J-K flip-flop clear function immediately
  Serial.printf("GPIO16 at startup: %d\n", digitalRead(16));
  Serial.println("Testing dual inverter buffer...");
  delay(100);
  Serial.printf("GPIO16 after delay: %d\n", digitalRead(16));

  // Simplified power management - disable individual functions
  eora_disable_wifi();
  eora_disable_bluetooth();
  eora_disable_adc();

  // Configure wake indicator LED manually  
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);  // Turn off LED

  // Initialize LittleFS
  bool fsok = LittleFS.begin(true);
  Serial.printf("FS init: %s\n", fsok ? "ok" : "fail!");
  Serial.flush();

  // Basic pin setup
  pinMode(TRIGGER, OUTPUT);
  pinMode(KY002S_PIN, INPUT);
  pinMode(ALERT, INPUT);
  pinMode(38, OUTPUT);  // Wake indicator
  digitalWrite(38, LOW);

  // Check KY002S initial state
  int value = digitalRead(KY002S_PIN);
  if (value < 1) {
    digitalWrite(TRIGGER, HIGH);
  }

  // Initialize LoRa
  setupLoRa();

  Serial.println("=== System ready with dual inverter wake-up ===");
  Serial.flush();

  // Only test hardware on cold boot, not on wake
  if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("🔄 Testing dual inverter chain...");
    // testHardwareRoute();  // Only on cold boot if needed
  }

  // If this is a cold boot, listen briefly then sleep regardless of packets
  if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0 && wakeup_reason != ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Cold boot - will listen for 30 seconds then force sleep");
    Serial.flush();
    
    unsigned long startTime = millis();
    bool packetReceived = false;
    
    // Listen for packets for 30 seconds (extended for testing)
    while (millis() - startTime < 30000) {
      if (receivedFlag) {
        packetReceived = true;
        Serial.println("=== Packet received during startup! ===");
        
        interruptEnabled = false;
        receivedFlag = false;

        String str;
        int state = radio.readData(str);

        if (state == RADIOLIB_ERR_NONE) {
          Serial.print("Data: ");
          Serial.println(str);
          Serial.printf("RSSI: %.1f dBm\n", radio.getRSSI());
          Serial.flush();

          parseString(str);  // Process the packet
          
          // If task was triggered, break and continue to main loop
          if (taskCode != 0) {
            Serial.println("Task triggered - staying awake");
            Serial.flush();
            radio.startReceive();
            interruptEnabled = true;
            return;  // Exit setup, continue to loop
          }
        }

        radio.startReceive();
        interruptEnabled = true;
      }
      delay(100);  // Small delay in listening loop
    }
    
    // After 30 seconds, force sleep regardless
    Serial.println("Startup period ended - going to sleep");
    Serial.printf("Packets received: %s\n", packetReceived ? "YES" : "NO");
    Serial.flush();
    goToSleep();
  }
}

void loop() {
  // Check for received packets
  if (receivedFlag) {
    interruptEnabled = false;
    receivedFlag = false;

    String str;
    int state = radio.readData(str);

    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("=== Packet received! ===");
      Serial.print("Data: ");
      Serial.println(str);
      
      // Print RSSI and SNR
      Serial.printf("RSSI: %.1f dBm\n", radio.getRSSI());
      Serial.printf("SNR: %.1f dB\n", radio.getSNR());
      Serial.printf("Frequency error: %.1f Hz\n", radio.getFrequencyError());
      Serial.flush();

      parseString(str);  // This triggers your switch and timer
      
   // } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
   //   Serial.println("CRC error - packet corrupted");
    } else {
      Serial.printf("Receive failed, code: %d\n", state);
    }

    // Resume listening
    radio.startReceive();
    interruptEnabled = true;
  }

  // Small delay to prevent excessive CPU usage
  delay(10);
}
