#include "eora_s3_power_mgmt.h"   //Must be in same folder as eora-S3-Power---Test.ino

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("No-I2C Power Test");
    
 // Test config - don't touch ANY GPIOs
eora_power_config_t config = {
    .disable_wifi = true,
    .disable_bluetooth = true,
    .disable_uart = true,        
    .disable_adc = true,         
    .disable_i2c = false,        
    .disable_unused_spi = true,  
    .configure_safe_gpios = false // DISABLE GPIO config temporarily
};

// Keep your pins defined but don't configure them
uint64_t my_pins = (1ULL << 12) |  // INA226 Alert
                   (1ULL << 13) |  // KY002S Trigger
                   (1ULL << 15) |  // GPIO33->15 route (CRITICAL!)
                   (1ULL << 47) |  // INA226 SDA
                   (1ULL << 48);   // INA226 SCL

eora_power_management(&config, my_pins);
    //my_pins = 0;  // No pins

    
    // Only call WiFi/BT disable
    eora_disable_wifi();
    eora_disable_bluetooth();
    
    Serial.println("Minimal WiFi/BT disable completed!");
}

void loop() {
    Serial.println("Running without I2C conflicts...");
    delay(5000);
}
