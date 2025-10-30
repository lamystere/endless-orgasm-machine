#ifndef __eom_hal_hpp
#define __eom_hal_hpp

#include <stddef.h>
#include <stdint.h>



//=== Housekeeping

#ifdef __cplusplus
extern "C" {
#endif

//=== Pressure
uint16_t eom_hal_get_pressure_reading(void);
uint8_t eom_hal_get_sensor_sensitivity(void);
void eom_hal_set_sensor_sensitivity(uint8_t sensitivity);
void eom_hal_init_pressure_sensor(void);
void eom_hal_setup_pressure_ambient(void);

//=== Vibration
void eom_hal_set_motor_speed(uint8_t speed);
void eom_hal_init_motor(void);

void eom_hal_set_led(uint8_t on);

#endif
