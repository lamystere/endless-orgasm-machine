#include "config.h"
//#include "esp_log.h"
#include "orgasm_control.h"
#include "vibration_mode_controller.h"

static const char* TAG = "depletion_controller";

static struct {
    float pleasure;
    uint16_t arousal;
    float base_speed;
} state;

static float start(void) {
    state.base_speed = Config.initial_pleasure;
    return state.base_speed;
}

static float increment(void) {
    if (Config.sensitivity_threshold == 0) return 0.0;

    if (state.base_speed < Config.max_pleasure) {
        state.base_speed += calculate_increment(
            Config.initial_pleasure, Config.max_pleasure, Config.motor_ramp_time_s
        );
    }

    float alter_perc = ((float)state.arousal / Config.sensitivity_threshold);
    float final_speed = state.base_speed * (1 - alter_perc);

    if (final_speed < (float)Config.initial_pleasure) {
        return Config.initial_pleasure;
    } else if (final_speed > (float)Config.max_pleasure) {
        return Config.max_pleasure;
    } else {
        return final_speed;
    }
}

static void tick(float pleasure, uint16_t arousal) {
    state.pleasure = pleasure;
    state.arousal = arousal;
}

static float stop(void) {
    return 0;
}

const vibration_mode_controller_t DepletionController = {
    .start = start,
    .increment = increment,
    .tick = tick,
    .stop = stop,
};