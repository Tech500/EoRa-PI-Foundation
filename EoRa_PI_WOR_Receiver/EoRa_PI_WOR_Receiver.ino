/*
   v6 Fixed LoRa Receiver - Based on working Ebyte "utilites.h" pin assignments.
   Coded by Claude with prompting by William Lucid and Ebyte provided example for
   EoRa PI (Ebyte EoRa-S3-900TB dev board) Example:  "SX1262_Transmit_Interrupt".
      
   100% Working!!!  07/18/2025 @ 12:32 EDT
*/

#include <RadioLib.h>
#define EoRa_PI_V1
#include "boards.h"

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

void setup()
{
    Serial.begin(115200);
    
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
        Serial.println("=== Receiver Ready ===");
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