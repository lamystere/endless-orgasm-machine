#include <stddef.h>
#include <stdint.h>
//#include "config.h"
#include "eom-hal-esp32.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
//#include "driver/adc_oneshot.h"
#include "driver/dac_oneshot.h"
#include "driver/gpio.h"
#include "config.h"
#include "esp_log.h"

static const char* TAG = "HAL";

uint8_t pressure_ambient = DEFAULT_AMBIENT_RESSURE;
uint8_t pressure_sensitivity = DEFAULT_PRESSURE_SENSITIVITY;  

adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t adc1_init_cfg = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};
#define MOTOR1_PIN DAC_CHANNEL_0
#define MOTOR2_PIN DAC_CHANNEL_1
#if SOC_DAC_SUPPORTED
    dac_oneshot_handle_t dac1_handle;
    dac_oneshot_handle_t dac2_handle;
    dac_oneshot_config_t dac1_cfg = {
        .chan_id = DAC_CHAN_0,  //motor 1
    };
    dac_oneshot_config_t dac2_cfg = {
        .chan_id = DAC_CHAN_1,  //motor 2
    };
#endif

//=== Pressure
uint16_t eom_hal_get_pressure_reading(void) {
    int raw = 0;

    //ADC_CHANNEL_0 = pin 1 on Waveshare ESP32-S3

    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw);  

    //ESP_LOGI(TAG, "Pressure raw: %d", raw);
    raw = raw - pressure_ambient;
    if (raw < 0) {
        raw = 0;
        eom_hal_setup_pressure_ambient();
    }

    //raw *= pressure_sensitivity;  //option 1: software multiplier - not necessary if using this value to set different attenuation levels
    if (raw > EOM_HAL_PRESSURE_MAX) raw = EOM_HAL_PRESSURE_MAX;
    return (uint16_t)raw;
}

uint8_t eom_hal_get_sensor_sensitivity(void) {
    return pressure_sensitivity; 
}

void eom_hal_init_pressure_sensor(void) {
    // ESP_ERROR_CHECK(adc1_config_width(ADC_BITWIDTH_12));

    adc_atten_t esp_sensitivity = Config.sensor_sensitivity >= 3 ? ADC_ATTEN_DB_0 
                                : (Config.sensor_sensitivity == 2 ? ADC_ATTEN_DB_2_5 
                                : (Config.sensor_sensitivity == 1 ? ADC_ATTEN_DB_6 
                                : ADC_ATTEN_DB_12));
    adc_oneshot_chan_cfg_t adcCfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = esp_sensitivity,
    };
    adc_oneshot_new_unit(&adc1_init_cfg, &adc1_handle);
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &adcCfg));

    eom_hal_setup_pressure_ambient();

    //intializing LED here also for now..need a general hardware init
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
}

void eom_hal_setup_pressure_ambient(void) {
    uint16_t readings[10];
    uint32_t sum = 0;
        
    for (int i = 0; i < 10; i++) {
        readings[i] = eom_hal_get_pressure_reading();
        sum += readings[i];
        
        //vTaskDelay(10);  // Small delay between readings?
    }
    pressure_ambient = sum / 10;
}

void eom_hal_set_sensor_sensitivity(uint8_t sensitivity) {
    pressure_sensitivity = sensitivity;

    if (adc1_handle == NULL) return; //not initialized yet

    adc_atten_t esp_sensitivity = sensitivity >= 3 ? ADC_ATTEN_DB_0 
                                : (sensitivity == 2 ? ADC_ATTEN_DB_2_5 
                                : (sensitivity == 1 ? ADC_ATTEN_DB_6 
                                : ADC_ATTEN_DB_12));
    adc_oneshot_chan_cfg_t adcCfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = esp_sensitivity,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &adcCfg));

    //use this as a software multiplier or use it to set different ADC attenuation levels?
    // ADC_ATTEN_DB_0   = 0,  ///<No input attenuation, ADC can measure up to approx.
    // ADC_ATTEN_DB_2_5 = 1,  ///<The input voltage of ADC will be attenuated extending the range of measurement by about 2.5 dB
    // ADC_ATTEN_DB_6   = 2,  ///<The input voltage of ADC will be attenuated extending the range of measurement by about 6 dB
    // ADC_ATTEN_DB_12  = 3,  ///<The input voltage of ADC will be attenuated extending the range of measurement by about 12 dB


}

//=== Vibration
void eom_hal_set_motor1_speed(uint8_t speed) {
#if SOC_DAC_SUPPORTED
    dac_oneshot_output_voltage(dac1_handle, speed);
#else
    // No DAC support on C6, implement pwm motor control here...eventually
#endif
}

void eom_hal_set_motor2_speed(uint8_t speed) {
#if SOC_DAC_SUPPORTED
    dac_oneshot_output_voltage(dac2_handle, speed);
#else
    // No DAC support on C6, implement pwm motor control here...eventually
#endif
}

void eom_hal_init_motor(void) {
#if SOC_DAC_SUPPORTED
    dac_oneshot_new_channel(&dac1_cfg, &dac1_handle);
    dac_oneshot_new_channel(&dac2_cfg, &dac2_handle);
#else
    // gpio_config(MOTOR1_PIN, GPIO_MODE_OUTPUT);
    // gpio_config(MOTOR2_PIN, GPIO_MODE_OUTPUT);
#endif
}

void eom_hal_set_led(uint8_t on) {
    if (on) {
        gpio_set_level(LED_PIN, 1);
    } else {
        gpio_set_level(LED_PIN, 0);
    }
}
