#include <stddef.h>
#include <stdint.h>
//#include "config.h"
#include "eom-hal-esp32.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/dac_oneshot.h"
#include "driver/gpio.h"
#include "config.h"
#include "esp_log.h"

//#include "driver/adc_oneshot.h"
static const char* TAG = "HAL";

uint8_t pressure_ambient = DEFAULT_AMBIENT_RESSURE;
uint8_t pressure_sensitivity = DEFAULT_PRESSURE_SENSITIVITY;  

adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t adc1_cfg = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};
dac_oneshot_handle_t dac1_handle;
dac_oneshot_handle_t dac2_handle;
dac_oneshot_config_t dac1_cfg = {
    .chan_id = DAC_CHAN_0,  //motor 1
};
dac_oneshot_config_t dac2_cfg = {
    .chan_id = DAC_CHAN_1,  //motor 2
};


//=== Pressure
uint16_t eom_hal_get_pressure_reading(void) {
    int raw = 0;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw);
    // adc1_get_raw(ADC_CHANNEL_5);

    //ESP_LOGI(TAG, "Pressure raw: %d", raw);
    raw = raw - pressure_ambient;
    if (raw < 0) {
        raw = 0;
        //eom_hal_setup_pressure_ambient();
    }
    if (raw > EOM_HAL_PRESSURE_MAX) raw = EOM_HAL_PRESSURE_MAX;
    return (uint16_t)raw;
}

uint8_t eom_hal_get_sensor_sensitivity(void) {
    return pressure_sensitivity; 
}

void eom_hal_init_pressure_sensor(void) {
    // ESP_ERROR_CHECK(adc1_config_width(ADC_BITWIDTH_12));
    // ESP_ERROR_CHECK(adc1_config_channel_atten(ADC_CHANNEL_5,ADC_ATTEN_DB_11));
    adc_oneshot_new_unit(&adc1_cfg, &adc1_handle);

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
}

void eom_hal_setup_pressure_ambient(void) {
    uint16_t readings[10];
    uint32_t sum = 0;
        
    for (int i = 0; i < 10; i++) {
        readings[i] = eom_hal_get_pressure_reading();
        sum += readings[i];
        // Small delay between readings
        //vTaskDelay(10);
    }
    pressure_ambient = sum / 10;
}

void eom_hal_set_sensor_sensitivity(uint8_t sensitivity) {
    pressure_sensitivity = sensitivity;
}

//=== Vibration
void eom_hal_set_motor_speed(uint8_t speed) {
    //uint8_t scaledSpeed = (uint8_t)((speed / 127.0f) * 255);  
    dac_oneshot_output_voltage(dac1_handle, speed);
    dac_oneshot_output_voltage(dac2_handle, speed);
}

void eom_hal_init_motor(void) {
    dac_oneshot_new_channel(&dac1_cfg, &dac1_handle);
    dac_oneshot_new_channel(&dac2_cfg, &dac2_handle);
}

void eom_hal_set_led(uint8_t on) {
    if (on) {
        gpio_set_level(LED_PIN, 1);
    } else {
        gpio_set_level(LED_PIN, 0);
    }
}
