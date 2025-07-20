/*
   Based on Ebyte "utilites.h" pin assignments.  Coded by Claude with 
   prompting by William Lucid and Working Ebyte example for EoRa PI 
   (Ebyte EoRa-S3-900TB dev board)  "SX1262_Transmit_Interrupt".
      
   "EoRa_PI_WOR_Receiver_with_Functions.ino"   07/20/2025 @ 10:53 EDT
   Remote Receiver --Battery Powered

   Code building path:  "H:\EoRa PI Project\EoRa_Ra_PI_Remote_Switch\EoRa_Ra_PI_Remote_Switch.ino"
*/

#include <RadioLib.h>
#define EoRa_PI_V1
#include "boards.h"

#include <LittleFS.h>
#include <INA226_WE.h>
#include <Wire.h>
#include <Ticker.h>

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

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool interruptEnabled = true;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void)
{
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

String timestamp = "";

// --- Task Definitions ---
enum TaskOption {
  TASK_NONE = 0,
  TASK_MEASURE_POWER = 1,   // Triggered by LoRa
  TASK_TRIGGER_SENSOR = 2   // Triggered by timer
};

#define INA_SDA 15
#define INA_SCL 16

TwoWire I2C_1 = TwoWire(1);


#define INA226_ADDRESS 0x40

INA226_WE ina226(&I2C_1, INA226_ADDRESS);

#define TRIGGER    45     //KY002S MOSFET Bi-Stable Switch
#define KY002S_PIN 46  //KY002S MOSFET Bi-Stable Switch --Read output
#define ALERT      19        //INA226 Battery Monitor

// Global variables for missing declarations
volatile bool alertFlag = false;
int activationCount = 0;

// Forward declarations
void readINA226(const char* dtStamp);
void triggerKY002S();

// Alert interrupt handler
void alert() {
    alertFlag = true;
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
            //Get Data and Log INA226
            readINA226(timestamp); // Pass a timestamp string
            //Start Two Minute Timer
            break;
        case TASK_TRIGGER_SENSOR:
            triggerKY002S();
            break;
        default:
#ifdef LOG_ON
            Serial.println("Unknown task");
#endif
            break;
    }
}

void parseString(String str) {
    Serial.print("Received String: ");
    Serial.println(str);
    Serial.flush();

    int delimiterIndex = str.indexOf(',');
    if (delimiterIndex != -1) {
        int taskCode = str.substring(0, delimiterIndex).toInt();
        String timestamp = str.substring(delimiterIndex + 1);
        
        // Pass both taskCode and timestamp
        dispatchTask(taskCode, timestamp);

        Serial.println("Parsed Task: " + String(taskCode));
        Serial.println("Timestamp: " + timestamp);
        Serial.flush();
    }
}

void checkForI2cErrors() {
    byte errorCode = ina226.getI2cErrorCode();
    if (errorCode) {
        Serial.print("I2C error: ");
        Serial.println(errorCode);
        switch (errorCode) {
            case 1:
                Serial.println("Data too long to fit in transmit buffer");
                break;
            case 2:
                Serial.println("Received NACK on transmit of address");
                break;
            case 3:
                Serial.println("Received NACK on transmit of data");
                break;
            case 4:
                Serial.println("Other error");
                break;
            case 5:
                Serial.println("Timeout");
                break;
            default:
                Serial.println("Can't identify the error");
        }
        if (errorCode) {
            while (1) {}
        }
    }
}

void readINA226(const char* dtStamp) {
    float shuntVoltage_mV = 0.0;
    float loadVoltage_V = 0.0;
    float busVoltage_V = 0.0;
    float current_mA = 0.0;
    float power_mW = 0.0;
    
    ina226.startSingleMeasurement();
    ina226.readAndClearFlags();
    shuntVoltage_mV = ina226.getShuntVoltage_mV();
    busVoltage_V = ina226.getBusVoltage_V();
    current_mA = ina226.getCurrent_mA();
    power_mW = ina226.getBusPower();
    loadVoltage_V = busVoltage_V + (shuntVoltage_mV / 1000);
    checkForI2cErrors();

    Serial.println(dtStamp);
    Serial.print("Shunt Voltage [mV]: ");
    Serial.println(shuntVoltage_mV);
    Serial.print("Bus Voltage [V]: ");
    Serial.println(busVoltage_V);
    Serial.print("Load Voltage [V]: ");
    Serial.println(loadVoltage_V);
    Serial.print("Current[mA]: ");
    Serial.println(current_mA);
    Serial.print("Bus Power [mW]: ");
    Serial.println(power_mW);

    if (!ina226.overflow) {
        Serial.println("Values OK - no overflow");
    } else {
        Serial.println("Overflow! Choose higher current range");
    }
    Serial.println();

    // Open a "log.txt" for appended writing
    File log = LittleFS.open("/log.txt", "a");

    if (!log) {
        Serial.println("file 'log.txt' open failed");
        return; // Exit if file cannot be opened
    }

    log.print(dtStamp);
    log.print(" , ");
    log.print(activationCount);
    log.print(" , ");
    log.print(shuntVoltage_mV, 3);
    log.print(" , ");
    log.print(busVoltage_V, 3);
    log.print(" , ");
    log.print(loadVoltage_V, 3);
    log.print(" , ");
    log.print(current_mA, 3);
    log.print(" , ");
    log.print(power_mW, 3);
    log.print(" , ");
    if (alertFlag) {
        log.print("Under Voltage alert");
        alertFlag = false;
    }
    log.println("");
    log.close();
    
    activationCount++; // Increment activation count
}

void triggerKY002S() {
    digitalWrite(TRIGGER, LOW);
    delay(100);
    digitalWrite(TRIGGER, HIGH);
}

void setup()
{
    Serial.begin(115200);
    while(!Serial){};

    I2C_1.begin(INA_SDA, INA_SCL, 100000);
    // INA226 init here using I2C_1

    bool fsok = LittleFS.begin(true);
    Serial.printf_P(PSTR("FS init: %s\n"), fsok ? PSTR("ok") : PSTR("fail!"));

    pinMode(TRIGGER, OUTPUT);        // ESP32S3, GPIO45
    pinMode(KY002S_PIN, INPUT);      //ESP32S3, GPIO46
    pinMode(ALERT, INPUT);           // ESP32S3, GPIO41

    int value = digitalRead(KY002S_PIN);  //KY002S, Vo pin
    if (value < 1) {
        digitalWrite(TRIGGER, HIGH);  //KY002S, TRG pin
    }

    twoMinuteTicker.once(120, []() {
        triggerKY002S();  // Task 2
    });

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
        16,        // Preamble length
        0.0,      // No TCXO
        true      // 🔐 LDO mode ON
    );
    
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
        Serial.printf("Radio initialized at %.1f MHz\n", radioFreq);
    }
    else
    {
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
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("success!"));
        Serial.println("=== Receiver Ready ===\n");
    }
    else
    {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true) {
            delay(1000);
        }
    }

#ifdef HAS_DISPLAY
    if (u8g2)
    {
        u8g2->clearBuffer();
        u8g2->drawStr(0, 12, "LoRa Receiver");
        u8g2->drawStr(0, 24, "Ready to receive...");
        char freqStr[32];
        snprintf(freqStr, sizeof(freqStr), "%.1f MHz", radioFreq);
        u8g2->drawStr(0, 36, freqStr);
        u8g2->sendBuffer();
    }
#endif

   
    if (!ina226.init()) {
        Serial.println("\nFailed to init INA226. Check your wiring.");
        //while(1){}
    }

    // INA226 configuration
    ina226.enableAlertLatch();
    ina226.setAlertType(BUS_UNDER, 2.9);
    attachInterrupt(digitalPinToInterrupt(ALERT), alert, FALLING);
}

void loop()
{
    // check if the flag is set
    if (receivedFlag)
    {
        // disable the interrupt service routine while processing the data
        interruptEnabled = false;

        // reset flag
        receivedFlag = false;

        // you can read received data as an Arduino String
        String str;
        int state = radio.readData(str);

        if (state == RADIOLIB_ERR_NONE)
        {
            // packet was successfully received
            Serial.println(F("[SX126x] Received packet!"));

            parseString(str); // Fixed function name and pass the string
            
            // print data of the packet
            Serial.print(F("[SX126x] Data:\t\t"));
            Serial.println(str);

            // print RSSI (Received Signal Strength Indicator)
            Serial.print(F("[SX126x] RSSI:\t\t"));
            Serial.print(radio.getRSSI());
            Serial.println(F(" dBm"));

            // print SNR (Signal-to-Noise Ratio)
            Serial.print(F("[SX126x] SNR:\t\t"));
            Serial.print(radio.getSNR());
            Serial.println(F(" dB"));

            // print frequency error
            Serial.print(F("[SX126x] Frequency error:\t"));
            Serial.print(radio.getFrequencyError());
            Serial.println(F(" Hz"));

#ifdef HAS_DISPLAY
            if (u8g2)
            {
                u8g2->clearBuffer();
                char buf[256];
                u8g2->drawStr(0, 12, "Received OK!");
                
                // Truncate long messages for display
                String displayStr = str;
                if (displayStr.length() > 16) {
                    displayStr = displayStr.substring(0, 16) + "...";
                }
                snprintf(buf, sizeof(buf), "RX:%s", displayStr.c_str());
                u8g2->drawStr(0, 24, buf);
                
                snprintf(buf, sizeof(buf), "RSSI:%.1f", radio.getRSSI());
                u8g2->drawStr(0, 36, buf);
                
                snprintf(buf, sizeof(buf), "SNR:%.1f", radio.getSNR());
                u8g2->drawStr(0, 48, buf);
                
                u8g2->sendBuffer();
            }
#endif
        }
        else if (state == RADIOLIB_ERR_CRC_MISMATCH)
        {
            // packet was received, but is malformed
            Serial.println(F("CRC error!"));
            
#ifdef HAS_DISPLAY
            if (u8g2)
            {
                u8g2->clearBuffer();
                u8g2->drawStr(0, 12, "CRC Error!");
                u8g2->drawStr(0, 24, "Packet corrupted");
                u8g2->sendBuffer();
            }
#endif
        }
        else
        {
            // some other error occurred
            Serial.print(F("failed, code "));
            Serial.println(state);
        }

        // put module back to listen mode
        radio.startReceive();

        // we're ready to receive more packets,
        // enable interrupt service routine
        interruptEnabled = true;
    }
    
    // Optional: Add a small delay to prevent excessive CPU usage
    delay(1);
}
