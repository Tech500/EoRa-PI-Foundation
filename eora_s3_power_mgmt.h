#ifndef EORA_S3_POWER_MGMT_H
#define EORA_S3_POWER_MGMT_H

#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_pm.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/i2c.h"
#include "soc/periph_defs.h"
#include "esp_private/periph_ctrl.h"
#include "hal/gpio_hal.h"
#include "soc/gpio_num.h"
#include "esp_log.h"

static const char* TAG = "EORA_POWER";

// EoRa-S3-900TB specific reserved pins - DO NOT TOUCH THESE!
#define EORA_LORA_SCLK_PIN      5   // Internal LoRa SPI Clock
#define EORA_LORA_MISO_PIN      3   // Internal LoRa SPI MISO (BOOT PIN!)
#define EORA_LORA_MOSI_PIN      6   // Internal LoRa SPI MOSI  
#define EORA_LORA_CS_PIN        7   // Internal LoRa SPI CS
#define EORA_LORA_DIO1_PIN      33  // Internal LoRa DIO1
#define EORA_LORA_BUSY_PIN      34  // Internal LoRa BUSY
#define EORA_LORA_RST_PIN       8   // Internal LoRa Reset

// Configuration for EoRa-S3-900TB specific power management
typedef struct {
    bool disable_wifi;
    bool disable_bluetooth;
    bool disable_uart;          // Will keep UART0 for debugging
    bool disable_adc;
    bool disable_i2c;
    bool disable_unused_spi;    // Will NOT disable SPI2 (LoRa needs it)
    bool configure_safe_gpios;  // Only touch safe, unused GPIOs
} eora_power_config_t;

// Safe default for EoRa board
#define EORA_POWER_DEFAULT_CONFIG() { \
    .disable_wifi = true, \
    .disable_bluetooth = true, \
    .disable_uart = false, \
    .disable_adc = true, \
    .disable_i2c = true, \
    .disable_unused_spi = true, \
    .configure_safe_gpios = true \
}

// Conservative config - minimal power savings, maximum safety
#define EORA_POWER_SAFE_CONFIG() { \
    .disable_wifi = true, \
    .disable_bluetooth = true, \
    .disable_uart = false, \
    .disable_adc = false, \
    .disable_i2c = false, \
    .disable_unused_spi = false, \
    .configure_safe_gpios = false \
}

/**
 * @brief Check if GPIO is safe to reconfigure on EoRa-S3-900TB
 * @param gpio_num GPIO number to check
 * @return true if safe to modify, false if reserved
 */
bool eora_is_gpio_safe(int gpio_num) {
    // Check basic validity first
    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX || !GPIO_IS_VALID_GPIO(gpio_num)) {
        return false;
    }
    
    // CRITICAL: Never touch these EoRa-S3-900TB reserved pins
    // LoRa SPI pins (internal connections)
    if (gpio_num == EORA_LORA_SCLK_PIN || gpio_num == EORA_LORA_MISO_PIN || 
        gpio_num == EORA_LORA_MOSI_PIN || gpio_num == EORA_LORA_CS_PIN ||
        gpio_num == EORA_LORA_DIO1_PIN || gpio_num == EORA_LORA_BUSY_PIN ||
        gpio_num == EORA_LORA_RST_PIN) {
        return false;
    }
    
    // ESP32-S3 critical system pins
    // Boot/strapping pins: 0, 45, 46 (GPIO3 already excluded above)
    // Flash pins: 26-32 
    // USB pins: 19, 20
    if (gpio_num == 0 || gpio_num == 45 || gpio_num == 46 ||
        gpio_num == 19 || gpio_num == 20 ||  // USB
        (gpio_num >= 26 && gpio_num <= 32)) { // Flash
        return false;
    }
    
    return true;
}

/**
 * @brief Safely disable WiFi for EoRa board
 */
esp_err_t eora_disable_wifi(void) {
    ESP_LOGI(TAG, "EoRa: Disabling WiFi");
    
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "WiFi stop failed: %s", esp_err_to_name(ret));
    }
    
    ret = esp_wifi_deinit();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "WiFi deinit failed: %s", esp_err_to_name(ret));
    }
    
    periph_module_disable(PERIPH_WIFI_MODULE);
    return ESP_OK;
}

/**
 * @brief Safely disable Bluetooth for EoRa board
 */
esp_err_t eora_disable_bluetooth(void) {
    ESP_LOGI(TAG, "EoRa: Disabling Bluetooth");
    
    esp_err_t ret = esp_bt_controller_disable();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "BT disable failed: %s", esp_err_to_name(ret));
    }
    
    ret = esp_bt_controller_deinit();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "BT deinit failed: %s", esp_err_to_name(ret));
    }
    
    periph_module_disable(PERIPH_BT_MODULE);
    return ESP_OK;
}

/**
 * @brief Disable UART (keep UART0 for debugging)
 */
esp_err_t eora_disable_uart(void) {
    ESP_LOGI(TAG, "EoRa: Disabling UART1, UART2 (keeping UART0)");
    
    uart_driver_delete(UART_NUM_1);
    uart_driver_delete(UART_NUM_2);
    
    periph_module_disable(PERIPH_UART1_MODULE);
    periph_module_disable(PERIPH_UART2_MODULE);
    
    return ESP_OK;
}

/**
 * @brief Disable ADC (safe for EoRa)
 */
esp_err_t eora_disable_adc(void) {
    ESP_LOGI(TAG, "EoRa: Disabling ADC");
    periph_module_disable(PERIPH_SARADC_MODULE);
    return ESP_OK;
}

/**
 * @brief Disable I2C (safe for EoRa)
 */
esp_err_t eora_disable_i2c(void) {
    ESP_LOGI(TAG, "EoRa: Disabling I2C");
    
    i2c_driver_delete(I2C_NUM_0);
    i2c_driver_delete(I2C_NUM_1);
    
    periph_module_disable(PERIPH_I2C0_MODULE);
    periph_module_disable(PERIPH_I2C1_MODULE);
    
    return ESP_OK;
}

/**
 * @brief Disable unused SPI (CRITICAL: Do NOT disable SPI2 - LoRa needs it!)
 */
esp_err_t eora_disable_unused_spi(void) {
    ESP_LOGI(TAG, "EoRa: Disabling SPI3 only (keeping SPI1/SPI2 for flash/LoRa)");
    
    // ONLY disable SPI3 - SPI1 is for flash, SPI2 is for internal LoRa
    periph_module_disable(PERIPH_SPI3_MODULE);
    
    return ESP_OK;
}

/**
 * @brief Configure safe, unused GPIOs for low power
 * @param user_pins Bitmask of additional pins user is using
 */
esp_err_t eora_configure_safe_gpios(uint64_t user_pins) {
    ESP_LOGI(TAG, "EoRa: Configuring safe GPIOs for low power");
    
    int configured_count = 0;
    
    for (int i = 0; i < GPIO_NUM_MAX; i++) {
        // Skip if not safe to modify
        if (!eora_is_gpio_safe(i)) {
            continue;
        }
        
        // Skip if user is using this pin
        if (user_pins & (1ULL << i)) {
            continue;
        }
        
        // Configure as input with pullup to prevent floating
        gpio_num_t gpio_pin = (gpio_num_t)i;
        gpio_set_direction(gpio_pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(gpio_pin, GPIO_PULLUP_ONLY);
        gpio_pullup_en(gpio_pin);
        gpio_pulldown_dis(gpio_pin);
        
        configured_count++;
    }
    
    ESP_LOGI(TAG, "EoRa: Configured %d safe GPIOs for low power", configured_count);
    return ESP_OK;
}

/**
 * @brief Complete EoRa-safe power management
 * @param config Power configuration
 * @param user_gpio_pins Bitmask of GPIO pins you're using in your application
 */
esp_err_t eora_power_management(const eora_power_config_t *config, uint64_t user_gpio_pins) {
    ESP_LOGI(TAG, "EoRa-S3-900TB: Starting safe power management");
    
    if (config->disable_wifi) {
        eora_disable_wifi();
    }
    
    if (config->disable_bluetooth) {
        eora_disable_bluetooth();
    }
    
    if (config->disable_uart) {
        eora_disable_uart();
    }
    
    if (config->disable_adc) {
        eora_disable_adc();
    }
    
    if (config->disable_i2c) {
        eora_disable_i2c();
    }
    
    if (config->disable_unused_spi) {
        eora_disable_unused_spi();
    }
    
    if (config->configure_safe_gpios) {
        eora_configure_safe_gpios(user_gpio_pins);
    }
    
    ESP_LOGI(TAG, "EoRa-S3-900TB: Power management completed safely");
    return ESP_OK;
}

/**
 * @brief Quick setup for deep sleep (EoRa-safe)
 * @param user_pins Your application GPIO pins to preserve
 */
void eora_prepare_deep_sleep(uint64_t user_pins) {
    ESP_LOGI(TAG, "EoRa-S3-900TB: Preparing for deep sleep");
    
    eora_power_config_t config = EORA_POWER_DEFAULT_CONFIG();
    config.disable_uart = false; // Keep UART0 for wake debugging
    
    eora_power_management(&config, user_pins);
    
    // Set conservative CPU frequency
    esp_pm_config_esp32s3_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 10,
        .light_sleep_enable = false
    };
    esp_pm_configure(&pm_config);
    
    ESP_LOGI(TAG, "EoRa-S3-900TB: Ready for deep sleep - LoRa preserved");
}

/**
 * @brief Emergency shutdown (preserves LoRa functionality)
 */
void eora_emergency_shutdown(void) {
    ESP_LOGW(TAG, "EoRa-S3-900TB: Emergency power shutdown (LoRa-safe)");
    
    // Only disable the big power consumers, leave LoRa alone
    esp_wifi_stop();
    esp_bt_controller_disable();
    
    periph_module_disable(PERIPH_WIFI_MODULE);
    periph_module_disable(PERIPH_BT_MODULE);
    
    ESP_LOGW(TAG, "EoRa-S3-900TB: Emergency shutdown completed");
}

/**
 * @brief Simple test function to verify safe operation
 */
void eora_power_test(void) {
    ESP_LOGI(TAG, "EoRa-S3-900TB Power Management Test");
    ESP_LOGI(TAG, "Reserved LoRa pins: 3,5,6,7,8,33,34");
    ESP_LOGI(TAG, "System boot pins: 0,19,20,26-32,45,46");
    ESP_LOGI(TAG, "Safe GPIO configuration ready");
    
    // Test with no user pins
    eora_power_config_t test_config = EORA_POWER_SAFE_CONFIG();
    test_config.configure_safe_gpios = true;
    
    eora_power_management(&test_config, 0);
    ESP_LOGI(TAG, "EoRa-S3-900TB: Power test completed successfully");
}

#endif // EORA_S3_POWER_MGMT_H