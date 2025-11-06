#include "config.h"
#include "esp_log.h"
#include "vibration_mode_controller.h"

static const char* TAG = "ramp_stop_controller";

static struct {
    float pleasure;
    uint16_t arousal;
} state;

static float start(void) {
    state.pleasure = Config.initial_pleasure;
    return Config.initial_pleasure;
}

static float increment(void) {
    float motor_increment = calculate_increment(
        Config.initial_pleasure, Config.max_pleasure, Config.motor_ramp_time_s
    );
    //ESP_LOGI(TAG, "Ramp Increment: %f, %f", motor_increment, state.pleasure + motor_increment);

    if (state.pleasure < (Config.max_pleasure - motor_increment)) {
        state.pleasure += motor_increment;
        return state.pleasure;
    } else {
        return Config.max_pleasure;
    }
}

static void tick(float pleasure, uint16_t arousal) {
    state.pleasure = pleasure;
    state.arousal = arousal;
}

static float stop(void) {
    state.pleasure = 0.0f;
    return 0.0f;
}

const vibration_mode_controller_t RampStopController = {
    .start = start,
    .increment = increment,
    .tick = tick,
    .stop = stop,
};