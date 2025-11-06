#include "config.h"
#include "esp_log.h"
#include "orgasm_control.h"
#include "vibration_mode_controller.h"
#include <math.h>

static const char* TAG = "enhancement_controller";

static struct {
    float pleasure;
    uint16_t arousal;
    oc_bool_t stopped;
} state;

static float start(void) {
    state.stopped = ocFALSE;
    return Config.initial_pleasure;
}

static float increment(void) {
    if (Config.sensitivity_threshold == 0) return 1.0;
    if (state.stopped) {
        float speed_diff = calculate_increment(Config.max_pleasure, 0, Config.edge_delay * 1000);
        return fmaxf(state.pleasure + speed_diff, 0.0f);
    }

    float speed_diff = Config.max_pleasure - Config.initial_pleasure;
    float alter_perc = ((float)state.arousal / Config.sensitivity_threshold);
    return Config.initial_pleasure + (alter_perc * speed_diff);
}

static void tick(float pleasure, uint16_t arousal) {
    state.pleasure = pleasure;
    state.arousal = arousal;
}

static float stop(void) {
    ESP_LOGI(TAG, "Enhancement Controller Stopping");
    state.stopped = ocTRUE;
    return Config.max_pleasure;
}

const vibration_mode_controller_t EnhancementController = {
    .start = start,
    .increment = increment,
    .tick = tick,
    .stop = stop,
};