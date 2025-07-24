/*

  Updated 07/23/2025 # 20:00 EDT  

   Based on Ebyte "utilites.h" pin assignments.  Coded by Claude with 
   prompting by William Lucid and Working Ebyte example for EoRa PI 
   (Ebyte EoRa-S3-900TB dev board)  "SX1262_Transmit_Interrupt".
*/

#include <RadioLib.h>
#define EoRa_PI_V1
#include "boards.h"

#include <LittleFS.h>
//#include <INA226_WE.h>
//#include <Wire.h>
#include <Ticker.h>
#include <rom/rtc.h>
#include <driver/rtc_io.h>

#define USING_SX1262_868M

#if defined(USING_SX1268_433M)
uint8_t txPower = 22;
float radioFreq = 433.0;
SX1268 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

#elif defined(USING_SX1262_868M)
uint8_t txPower = 22;
float radioFreq = 915.0;
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);

#endif

#define DI01 GPIO_NUM_15

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

//---------------- Added definitions ------------

// --- Timer Setup ---
Ticker twoMinuteTicker;

int taskCode = 0;

String timestamp = "";

// --- Task Definitions ---
enum TaskOption {
  TASK_NONE = 0,
  TASK_MEASURE_POWER = 1,  // Triggered by LoRa
  TASK_TRIGGER_SENSOR = 2  // Triggered by timer
};

#define TRIGGER 16     //KY002S MOSFET Bi-Stable Switch
#define KY002S_PIN 46  //KY002S MOSFET Bi-Stable Switch --Read output
#define ALERT 19       //INA226 Battery Monitor

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
  taskCode = 2;  
}

void dispatchTask(int taskCode, String(timestamp)) {
#ifdef LOG_ON
  Serial.println("--- DISPATCHING TASK ---");
  Serial.print("Task Code: ");
  Serial.println(taskCode);
#endif

  switch (taskCode) {
    case TASK_MEASURE_POWER:
      //Toggle Switch
      triggerKY002S();
      Serial.println("Turned on Switch");
      twoMinuteTicker.detach();
      twoMinuteTicker.once(120.0, isrCountTimer);
      break;
    case TASK_TRIGGER_SENSOR:
      triggerKY002S();
      Serial.println("Turned off Switch");
      goToSleep();
      break; 
    default:
#ifdef LOG_ON
      Serial.println("Unknown task");
#endif
      break;
  }
}

void parseString(String str) {
  Serial.print("\nReceived String: ");
  Serial.println(str);
  Serial.flush();

  int delimiterIndex = str.indexOf(',');
  if (delimiterIndex != -1) {
    taskCode = str.substring(0, delimiterIndex).toInt();
    timestamp = str.substring(delimiterIndex + 1);  // ✅ Use global timestamp

    dispatchTask(taskCode, timestamp);  // ✅ Single call to dispatch
    Serial.println("Parsed Task: " + String(taskCode));
    Serial.println("Timestamp: " + timestamp);
    Serial.flush();
  }
}

void triggerKY002S() {
  digitalWrite(TRIGGER, LOW);
  delay(100);
  digitalWrite(TRIGGER, HIGH);
}

void goToSleep() {
  //esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0); // Wake on LOW

  // Pulse to turn OFF switch
  triggerKY002S();

  // Put LoRa radio to sleep
  //radio.standby();

  // Optional: log or pulse logic pin
  Serial.println("Turned off Switch; Going to Deep Sleep");

  // Configure the pin as RTC GPIO
  rtc_gpio_init(GPIO_NUM_15);
  rtc_gpio_set_direction(GPIO_NUM_15, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_15);

  // Configure EXT0 wake-up (wake on LOW level - button press)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1);

  // Enter deep sleep
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {};

  esp_sleep_wakeup_cause_t wakeSource = esp_sleep_get_wakeup_cause();

  Serial.print("Wake reason: ");
  switch (wakeSource) {
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("External RTC signal (e.g. DIO1)"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Timer wake"); break;
    case ESP_SLEEP_WAKEUP_UNDEFINED: Serial.println("Power-on or reset"); break;
    default: Serial.println("Other wakeup reason"); break;
  }

  bool fsok = LittleFS.begin(true);
  Serial.printf_P(PSTR("FS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  pinMode(TRIGGER, OUTPUT);    // ESP32S3, GPIO45
  pinMode(KY002S_PIN, INPUT);  //ESP32S3, GPIO46
  pinMode(ALERT, INPUT);       // ESP32S3, GPIO41

  // Initialize board first - this sets up SPI and other peripherals
  initBoard();

  // Critical: Wait for board initialization to complete
  delay(2000);

  Serial.println("=== LoRa Receiver Starting ===");

  // Debug: Print pin configuration
  Serial.println("📌 Pin Configuration:");
  Serial.printf("  CS (NSS): GPIO %d\n", RADIO_CS_PIN);
  Serial.printf("  DIO1: GPIO %d\n", RADIO_DIO1_PIN);
  Serial.printf("  RESET: GPIO %d\n", RADIO_RST_PIN);
  Serial.printf("  BUSY: GPIO %d\n", RADIO_BUSY_PIN);
  Serial.printf("  SCLK: GPIO %d\n", RADIO_SCLK_PIN);
  Serial.printf("  MISO: GPIO %d\n", RADIO_MISO_PIN);
  Serial.printf("  MOSI: GPIO %d\n", RADIO_MOSI_PIN);
  Serial.println();

  // Initialize SX126x with the EXACT same settings as OEM example
  Serial.print(F("[SX126x] Initializing ... "));

  int state = radio.begin(
    radioFreq,  // 915.0 MHz as set earlier
    125.0,      // Bandwidth (default LoRa)
    9,          // Spreading factor
    7,          // Coding rate
    RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    txPower,  // 22 dBm
    16,       // Preamble length
    0.0,      // No TCXO
    true      // 🔐 LDO mode ON
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
    Serial.printf("Radio initialized at %.1f MHz\n", radioFreq);
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    Serial.println("Common error codes:");
    Serial.println("  -707: SPI communication failed");
    Serial.println("  -2: Chip not found/responding");
    Serial.println("Check wiring and power supply");
    while (true) {
      delay(1000);
    }
  }

  // Set the function that will be called when new packet is received
  radio.setDio1Action(setFlag);

  // Start listening for LoRa packets
  Serial.print(F("[SX126x] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
    Serial.println("=== Receiver Ready ===\n");
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) {
      delay(1000);
    }
  }

#ifdef HAS_DISPLAY
  if (u8g2) {
    u8g2->clearBuffer();
    u8g2->drawStr(0, 12, "LoRa Receiver");
    u8g2->drawStr(0, 24, "Ready to receive...");
    char freqStr[32];
    snprintf(freqStr, sizeof(freqStr), "%.1f MHz", radioFreq);
    u8g2->drawStr(0, 36, freqStr);
    u8g2->sendBuffer();
  }
#endif
}

void loop() {
  // Handle LoRa receive
  if (receivedFlag) {
    interruptEnabled = false;
    receivedFlag = false;

    String str;
    int state = radio.readData(str);

    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("[SX126x] Received packet!"));
      parseString(str);  // ✅ parseString calls dispatchTask()
      // DO NOT call dispatchTask again here!
      Serial.print(F("[SX126x] Data:\t\t"));
      Serial.println(str);
      Serial.print(F("[SX126x] RSSI:\t\t"));
      Serial.print(radio.getRSSI());
      Serial.println(F(" dBm"));
      Serial.print(F("[SX126x] SNR:\t\t"));
      Serial.print(radio.getSNR());
      Serial.println(F(" dB"));
      Serial.print(F("[SX126x] Frequency error:\t"));
      Serial.print(radio.getFrequencyError());
      Serial.println(F(" Hz"));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      Serial.println(F("CRC error!"));
    } else {
      Serial.print(F("Receive failed, code "));
      Serial.println(state);
    }

    radio.startReceive();
    interruptEnabled = true;
  }

  // ✅ Handle timer-triggered task (e.g. go to sleep)
  if (taskCode == TASK_TRIGGER_SENSOR) {
    dispatchTask(taskCode, timestamp);
    taskCode = TASK_NONE;
  }

  delay(1);  // prevent excessive CPU usage
}
