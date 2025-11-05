#include "config.h"
#include "esp_log.h"
#include "vibration_mode_controller.h"

static const char* TAG = "ramp_stop_controller";

static struct {
    float motor_speed;
    uint16_t arousal;
} state;

static float start(void) {
    state.motor_speed = Config.motor_start_speed;
    return Config.motor_start_speed;
}

static float increment(void) {
    float motor_increment = calculate_increment(
        Config.motor_start_speed, Config.motor_max_speed, Config.motor_ramp_time_s
    );
    //ESP_LOGI(TAG, "Ramp Increment: %f, %f", motor_increment, state.motor_speed + motor_increment);

    if (state.motor_speed < (Config.motor_max_speed - motor_increment)) {
        state.motor_speed += motor_increment;
        return state.motor_speed;
    } else {
        return Config.motor_max_speed;
    }
}

static void tick(float motor_speed, uint16_t arousal) {
    state.motor_speed = motor_speed;
    state.arousal = arousal;
}

static float stop(void) {
    state.motor_speed = 0.0f;
    return 0.0f;
}

const vibration_mode_controller_t RampStopController = {
    .start = start,
    .increment = increment,
    .tick = tick,
    .stop = stop,
};