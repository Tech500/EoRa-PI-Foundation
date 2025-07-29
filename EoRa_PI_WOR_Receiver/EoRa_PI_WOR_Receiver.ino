/*
  Updated 07/29/2025 # 01:22 EDT  

   Based on Ebyte "utilites.h" pin assignments.  Coded by Claude with 
   prompting by William Lucid 
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
#include "eora_s3_power_mgmt.h"

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

#define TXD1   12
#define RXD1   48

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
  Serial1.println("--- DISPATCHING TASK ---");
  Serial1.print("Task Code: ");
  Serial1.println(taskCode);
#endif

  switch (taskCode) {
    case TASK_MEASURE_POWER:
      //Toggle Switch
      triggerKY002S();
      Serial1.println("Turned on Switch");
      twoMinuteTicker.detach();
      twoMinuteTicker.once(120.0, isrCountTimer);  //Countdown timer interval 120 Seconds
      break;
    case TASK_TRIGGER_SENSOR:
      triggerKY002S();
      Serial1.println("Turned off Switch");
      goToSleep();
      break;
    default:
#ifdef LOG_ON
      Serial1.println("Unknown task");
#endif
      break;
  }
}

void parseString(String str) {
  int delimiterIndex = str.indexOf(',');
  if (delimiterIndex != -1) {
    taskCode = str.substring(0, delimiterIndex).toInt();
    timestamp = str.substring(delimiterIndex + 1);  // ✅ Use global timestamp

    dispatchTask(taskCode, timestamp);  // ✅ Single call to dispatch
    Serial1.println("Parsed Task: " + String(taskCode));
    Serial1.println("Timestamp: " + timestamp);
    Serial1.flush();
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
  radio.standby();

  // Optional: log or pulse logic pin
  Serial1.println("Going to Deep Sleep\n");

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
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);
  while (!Serial1) {};
 
  // Test config - don't touch ANY GPIOs
  eora_power_config_t config = {
    .disable_wifi = true,
    .disable_bluetooth = true,
    .disable_uart = false,
    .disable_adc = true,
    .disable_i2c = false,
    .disable_unused_spi = true,
    .configure_safe_gpios = false  // DISABLE GPIO config temporarily
  };

  // Keep your pins defined but don't configure them
  uint64_t my_pins = (1ULL << 12) |  // INA226 Alert
                     (1ULL << 13) |  // KY002S Trigger
                     (1ULL << 15) |  // GPIO33->15 route (CRITICAL!)
                     (1ULL << 38) |  // Wake indicator
                     (1ULL << 47) |  // INA226 SDA
                     (1ULL << 48);   // INA226 SCL

  eora_power_management(&config, my_pins);

  // Only call WiFi/BT disable
  eora_disable_wifi();
  eora_disable_bluetooth();
  eora_disable_adc();
  eora_disable_unused_spi();

  // Then manually configure GPIO38 after power management
  gpio_config_t led_config = {
    .pin_bit_mask = (1ULL << 38),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };

  gpio_config(&led_config);
  gpio_set_level(GPIO_NUM_38, 0);  // Turn off LED

  bool fsok = LittleFS.begin(true);
  Serial1.printf_P(PSTR("FS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

  pinMode(TRIGGER, OUTPUT);    // ESP32S3, GPIO45
  pinMode(KY002S_PIN, INPUT);  //ESP32S3, GPIO46
  pinMode(ALERT, INPUT);       // ESP32S3, GPIO41

  // Initialize board first - this sets up SPI and other peripherals
  initBoard();

  // Critical: Wait for board initialization to complete
  delay(2000);

  Serial1.println("=== LoRa Receiver Starting ===");

  // Debug: Print pin configuration
  Serial1.println("📌 Pin Configuration:");
  Serial1.printf("  CS (NSS): GPIO %d\n", RADIO_CS_PIN);
  Serial1.printf("  DIO1: GPIO %d\n", RADIO_DIO1_PIN);
  Serial1.printf("  RESET: GPIO %d\n", RADIO_RST_PIN);
  Serial1.printf("  BUSY: GPIO %d\n", RADIO_BUSY_PIN);
  Serial1.printf("  SCLK: GPIO %d\n", RADIO_SCLK_PIN);
  Serial1.printf("  MISO: GPIO %d\n", RADIO_MISO_PIN);
  Serial1.printf("  MOSI: GPIO %d\n", RADIO_MOSI_PIN);
  Serial1.println();

  // Initialize SX126x with the EXACT same settings as OEM example
  Serial1.print(F("[SX126x] Initializing ... "));

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
    Serial1.println(F("success!"));
    Serial1.printf("Radio initialized at %.1f MHz\n", radioFreq);
  } else {
    Serial1.print(F("failed, code "));
    Serial1.println(state);
    Serial1.println("Common error codes:");
    Serial1.println("  -707: SPI communication failed");
    Serial1.println("  -2: Chip not found/responding");
    Serial1.println("Check wiring and power supply");
    while (true) {
      delay(1000);
    }
  }

  // Set the function that will be called when new packet is received
  radio.setDio1Action(setFlag);

  esp_sleep_wakeup_cause_t wakeSource = esp_sleep_get_wakeup_cause();

  Serial1.print("Wake reason: ");
  switch (wakeSource) {
    case ESP_SLEEP_WAKEUP_EXT1: Serial1.println("External RTC signal (e.g. DIO1)"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial1.println("Timer wake"); break;
    case ESP_SLEEP_WAKEUP_UNDEFINED: Serial1.println("Power-on or reset"); break;
    default: Serial1.println("Other wakeup reason"); break;
  }

  // Start listening for LoRa packets
  Serial1.print(F("[SX126x] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial1.println(F("success!"));
    Serial1.println("=== Receiver Ready ===\n");
  } else {
    Serial1.print(F("failed, code "));
    Serial1.println(state);
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
      Serial1.println(F("[SX126x] Received packet!"));
      parseString(str);  // ✅ parseString calls dispatchTask()
      // DO NOT call dispatchTask again here!
      Serial1.print(F("[SX126x] Data:\t\t"));
      Serial1.println(str);
      Serial1.print(F("[SX126x] RSSI:\t\t"));
      Serial1.print(radio.getRSSI());
      Serial1.println(F(" dBm"));
      Serial1.print(F("[SX126x] SNR:\t\t"));
      Serial1.print(radio.getSNR());
      Serial1.println(F(" dB"));
      Serial1.print(F("[SX126x] Frequency error:\t"));
      Serial1.print(radio.getFrequencyError());
      Serial1.println(F(" Hz"));
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      Serial1.println(F("CRC error!"));
    } else {
      Serial1.print(F("Receive failed, code "));
      Serial1.println(state);
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
