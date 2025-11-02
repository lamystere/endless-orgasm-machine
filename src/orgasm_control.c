#include "orgasm_control.h"
//#include "accessory_driver.h"
//#include "bluetooth_driver.h"
#include "config.h"
#include "eom-hal-esp32.h"
#include "esp_log.h"
#include "esp_timer.h"
//#include "system/event_manager.h"
#include "system/websocket_handler.h"
////#include "ui/toast.h"
//#include "ui/ui.h"
//#include "util/i18n.h"
#include "util/running_average.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char* TAG = "orgasm_control";

static const char* orgasm_output_mode_str[] = {
    "MANUAL",
    "AUTOMATIC",
    "ORGASM",
};

static struct {
    unsigned long last_update_ms;
    running_average_t* average;
    uint16_t last_value;
    uint16_t pressure_value;
    uint16_t peak_start;
    uint16_t arousal;
    uint8_t update_flag;
    unsigned long in_cooldown;
} arousal_state;

static struct {
    orgasm_output_mode_t output_mode;
    vibration_mode_t vibration_mode;
    unsigned long motor_stop_time;
    unsigned long motor_start_time;
    unsigned long edge_time_out; // 10000?
    unsigned long random_additional_delay;
    int twitch_count;
    uint8_t control_motor;
    uint8_t prev_control_motor;
    uint8_t motor_speed;
} output_state;

static struct {
    // File Writer
    unsigned long recording_start_ms;
    FILE* logfile;
} logger_state;

static struct {
    //  Post Orgasm Clench variables
    long clench_pressure_threshold; //  4096?
    int clench_duration;

    // Autoedging Time and Post-Orgasm varables
    unsigned long auto_edging_start_millis;
    unsigned long post_orgasm_start_millis;
    unsigned long post_orgasm_duration_millis;
    unsigned long clench_start_millis;
    long clench_duration_millis;
    oc_bool_t menu_is_locked;
    oc_bool_t detected_orgasm;
    int post_orgasm_duration_seconds;
} post_orgasm_state;

volatile static struct {
    int orgasm_count;
    //event_handler_node_t* _h_orgasm;
} orgasm_state = { 0 };

// static void _evt_orgasm_start(
//     const char* evt, EVENT_HANDLER_ARG_TYPE eap, int eai, EVENT_HANDLER_ARG_TYPE hap
// ) {
//     if ( orgasm_control_is_permit_orgasm_reached() ) {
//         post_orgasm_state.detected_orgasm = ocTRUE;
//     }
//     //orgasm_state.orgasm_count += 1;
// }

#define update_check(variable, value)                                                              \
    {                                                                                              \
        if (variable != (value)) {                                                                 \
            ESP_LOGD(TAG, "updated: %s = %s", #variable, #value);                                  \
            variable = value;                                                                      \
            arousal_state.update_flag = ocTRUE;                                                    \
        }                                                                                          \
    }
int orgasm_control_get_orgasm_count(void) {
    return orgasm_state.orgasm_count;
}
int orgasm_control_get_arousal_sensitivity(void) {
    return Config.sensor_sensitivity;
}
/**
 * @brief Simplified method to set speed, which also handles broadcasting the event.
 * @param speed
 */
void orgasm_control_set_motor_speed(uint8_t speed) {
    static uint8_t last_speed = 0;
    if (speed > Config.motor_max_speed) speed = Config.motor_max_speed;
    if (speed == last_speed) return;
    last_speed = speed;

    #if MOTOR1_ENABLED
        eom_hal_set_motor_speed(speed);  //implement change on motor control pin.
    #endif

    output_state.motor_speed = speed;
    //event_manager_dispatch(EVT_SPEED_CHANGE, NULL, speed);
    //bluetooth_driver_broadcast_speed(speed);
}

void orgasm_control_init(void) {
    eom_hal_init_pressure_sensor();
    eom_hal_set_sensor_sensitivity(Config.sensor_sensitivity);
    output_state.output_mode = OC_AUTOMATIC;
    output_state.vibration_mode = Config.vibration_mode;
    output_state.edge_time_out = 10000;
    post_orgasm_state.clench_pressure_threshold = 4096;    
    arousal_state.in_cooldown = 0; 

    running_average_init(&arousal_state.average, Config.pressure_smoothing);
    orgasm_state.orgasm_count = 0;  
    // if (orgasm_state._h_orgasm == NULL) {
    //     orgasm_state._h_orgasm =
    //         event_manager_register_handler(EVT_ORGASM_START, &_evt_orgasm_start, NULL);
    // }
}

// Rename to get_vibration_mode_controller();
static const vibration_mode_controller_t* orgasm_control_getVibrationMode() {
    switch (Config.vibration_mode) {
    case Enhancement: return &EnhancementController;

    default:
    case Depletion: return &DepletionController;

    case Pattern: return &PatternController;

    case RampStop: return &RampStopController;
    }
}

/**
 * Main orgasm detection / edging algorithm happens here.
 * This happens with a default update frequency of 50Hz.
 */
static void orgasm_control_updateArousal() {
    // Decay stale arousal value:
    update_check(arousal_state.arousal, arousal_state.arousal * 0.99);

    // Acquire new pressure and take average:
    arousal_state.pressure_value = eom_hal_get_pressure_reading();
    running_average_add_value(arousal_state.average, arousal_state.pressure_value);
    long p_avg = running_average_get_average(arousal_state.average);
    long p_check = Config.use_average_values ? p_avg : arousal_state.pressure_value;

    // Increment arousal
    if (p_check < arousal_state.last_value) {                      // falling edge of peak
        if (arousal_state.last_value > arousal_state.peak_start) { // first tick past peak?
            if (arousal_state.last_value - arousal_state.peak_start >= Config.sensitivity_threshold / 10) { // big peak

                update_check(
                    arousal_state.arousal,
                    arousal_state.arousal + (arousal_state.last_value - arousal_state.peak_start)
                );

                arousal_state.peak_start = p_check;
            }
        }

        if (p_check < arousal_state.peak_start) {
            // run this value down to a new minimum after a peak detected.
            arousal_state.peak_start = p_check;
        }
    }

    arousal_state.last_value = p_check;

    long clench_duration;
    clench_duration = orgasm_control_clench_detect(p_check);
    if (Config.clench_detector_in_edging) {
        if (clench_duration > Config.clench_time_threshold_ms &&
                clench_duration < Config.max_clench_duration_ms) {
            arousal_state.arousal += 5;
            arousal_state.update_flag = ocTRUE;
        }
    }

    // Update accessories:
    if (arousal_state.update_flag) {
        //event_manager_dispatch(EVT_AROUSAL_CHANGE, NULL, arousal_state.arousal);
        //bluetooth_driver_broadcast_arousal(arousal_state.arousal);
        //websocket_driver_broadcast_arousal(arousal_state.arousal);

        // Update LED for Arousal Color
        // if (output_state.output_mode == OC_AUTOMATIC) {
        //     float arousal_perc = orgasm_control_get_arousal_percent() * 255.0f;
        //     if (arousal_perc > 255.0f) arousal_perc = 255.0f;
        //     eom_hal_set_encoder_rgb(arousal_perc, 255 - arousal_perc, 0);
        // }
    }
}

static void orgasm_control_updateMotorSpeed() {
    if (!output_state.control_motor) return;

    const vibration_mode_controller_t* controller = orgasm_control_getVibrationMode();
    controller->tick(output_state.motor_speed, arousal_state.arousal);

    // Calculate timeout delay
    //oc_bool_t arousal_state.in_cooldown = ocFALSE;
    long on_time = (esp_timer_get_time() / 1000UL) - output_state.motor_start_time;

    //check if its been long enough since motor stopped
    if ((esp_timer_get_time() / 1000UL) - output_state.motor_stop_time >
        (Config.edge_delay * 1000) + output_state.random_additional_delay) {
        arousal_state.in_cooldown = 0;
    } else {
        arousal_state.in_cooldown = (Config.edge_delay * 1000) + output_state.random_additional_delay - 
                                   ((esp_timer_get_time() / 1000UL) - output_state.motor_stop_time);
    }

    if (arousal_state.in_cooldown > 0) {
        // It hasn't been long enough since the pleasure stopped
        orgasm_control_twitch_detect();  //if still aroused restart the cooldown timeout counter until arousal totally subsides

    } else if (arousal_state.arousal > Config.sensitivity_threshold &&
               output_state.motor_speed > 0 && on_time > (Config.minimum_on_time * 1000)) {
        // We're not already in timeout, but arousal is high enough to stop the motor
        // Set the motor speed to 0, set stop time, and determine the new additional random time.
        output_state.motor_speed = controller->stop();
        output_state.motor_stop_time = (esp_timer_get_time() / 1000UL);
        output_state.motor_start_time = 0;
        arousal_state.update_flag = ocTRUE;
        arousal_state.in_cooldown = (Config.edge_delay * 1000) + output_state.random_additional_delay;
        eom_hal_set_led(1); // Turn on LED when orgasm detected
        orgasm_state.orgasm_count += 1;
        ESP_LOGI(TAG, "Orgasm Denied! Total: %d", orgasm_state.orgasm_count);

        //event_manager_dispatch(EVT_ORGASM_DENIAL, NULL, 0);

        // If Max Additional Delay is not disabled, caculate a new delay every time the motor is stopped
        if (Config.max_additional_delay  != 0) {
            output_state.random_additional_delay = random() % (Config.max_additional_delay * 1000);
        }

    } else if (output_state.motor_speed == 0 && output_state.motor_start_time == 0) {
        //the timeout is over and motor is stopped
        // its time to start again
        output_state.motor_speed = controller->start();
        output_state.motor_start_time = (esp_timer_get_time() / 1000UL);
        output_state.random_additional_delay = 0;
        arousal_state.update_flag = ocTRUE;
        eom_hal_set_led(0); // Turn off LED when motor starts
        ESP_LOGI(TAG, "Starting cycle: %d", orgasm_state.orgasm_count + 1);
    } else {
        // Normal pleasure mode...Increment or Change
        update_check(output_state.motor_speed, controller->increment());
    }

    // Control motor if we are not manually doing so.
    if (output_state.control_motor) {
        uint8_t speed = orgasm_control_get_motor_speed();
        orgasm_control_set_motor_speed(speed);
    }
}

static void orgasm_control_updateEdgingTime() { // Edging+Orgasm timer
    // Make sure menu_is_locked is turned off in Manual mode
    if (output_state.output_mode == OC_MANUAL) {
        post_orgasm_state.menu_is_locked = ocFALSE;
        post_orgasm_state.post_orgasm_duration_seconds = Config.post_orgasm_duration_seconds;
        return;
    }

    // keep edging start time to current time as long as system is not in Edge-Orgasm mode 2
    if (output_state.output_mode != OC_ORGASM) {
        post_orgasm_state.auto_edging_start_millis = (esp_timer_get_time() / 1000UL);
        post_orgasm_state.post_orgasm_start_millis = 0;
    }

    // Lock Menu if turned on. and in Edging_orgasm mode
    if (Config.edge_menu_lock && !post_orgasm_state.menu_is_locked) {
        // Lock only after 2 minutes
        if ((esp_timer_get_time() / 1000UL) >
            post_orgasm_state.auto_edging_start_millis + (2 * 60 * 1000)) {
            post_orgasm_state.menu_is_locked = ocTRUE;
            arousal_state.update_flag = ocTRUE;
        }
    }

    // Pre-Orgasm loop -- Orgasm is permited
    if (orgasm_control_is_permit_orgasm_reached() && !orgasm_control_is_post_orgasm_reached()) {
        if (output_state.control_motor) {
            orgasm_control_pause_control(); // make sure orgasm is now possible
        }

        // now detect the orgasm to start post orgasm torture timer
        if (post_orgasm_state.detected_orgasm) {
            post_orgasm_state.post_orgasm_start_millis =
                (esp_timer_get_time() / 1000UL); // Start Post orgasm torture timer
            // Lock menu if turned on
            if (Config.post_orgasm_menu_lock && !post_orgasm_state.menu_is_locked) {
                post_orgasm_state.menu_is_locked = ocTRUE;
            }

           // eom_hal_set_encoder_rgb(255, 0, 0);
        } else {
          //  eom_hal_set_encoder_rgb(0, 255, 0);
        }

        // raise motor speed to max speed. protect not to go higher than max
        if (output_state.motor_speed <= (Config.motor_max_speed - 5)) {
            output_state.motor_speed += 5;  //why 5?
        } else {
            update_check(output_state.motor_speed, Config.motor_max_speed);
        }
    }

    // Post Orgasm loop
    if (orgasm_control_is_post_orgasm_reached()) {
        post_orgasm_state.post_orgasm_duration_millis =
            (post_orgasm_state.post_orgasm_duration_seconds * 1000);

        // Detect if within post orgasm session
        if ((esp_timer_get_time() / 1000UL) < (post_orgasm_state.post_orgasm_start_millis +
                                               post_orgasm_state.post_orgasm_duration_millis)) {
            output_state.motor_speed = Config.motor_max_speed;
        } else {                                // Post_orgasm timer reached
            if (output_state.motor_speed > 0) { // Ramp down motor speed to 0
                output_state.motor_speed = output_state.motor_speed - 1;  // why 1?
            } else {
                post_orgasm_state.menu_is_locked = ocFALSE;
                post_orgasm_state.detected_orgasm = ocFALSE;
                output_state.motor_speed = 0;
                orgasm_control_set_motor_speed(output_state.motor_speed);
                orgasm_control_set_output_mode(OC_MANUAL);
            }
        }
    }
    // Control output while motor control is paused
    if (output_state.control_motor == OC_MANUAL) {
        uint8_t speed = orgasm_control_get_motor_speed();
        orgasm_control_set_motor_speed(speed);
    }
}

/**
 *  Detect muscle clenching.  
 *  Used to ajust arousal if turned on
 *  Used to detect the start of a Orgasm
 *  @param p_check
 *  @return clench duration in ms
 */
long orgasm_control_clench_detect(long p_check){
    static bool orgasm_detect = false;
    // raise clench threshold to pressure - 1/2 sensitivity
    long current_time = (esp_timer_get_time() / 1000UL);
    if (p_check >=
        (post_orgasm_state.clench_pressure_threshold + Config.clench_pressure_sensitivity)) {
        post_orgasm_state.clench_pressure_threshold =
            (p_check - (Config.clench_pressure_sensitivity / 2));
    }

    // Start counting clench time if pressure over threshold
    if (p_check >= post_orgasm_state.clench_pressure_threshold) {
        post_orgasm_state.clench_duration_millis =
            current_time - post_orgasm_state.clench_start_millis;

        // Orgasm detected
        if (post_orgasm_state.clench_duration_millis >= Config.clench_time_to_orgasm_ms &&
            !orgasm_detect) {
            orgasm_detect = true;
            if ( orgasm_control_is_permit_orgasm_reached() ) {
                post_orgasm_state.detected_orgasm = ocTRUE;
            }
            //event_manager_dispatch(EVT_ORGASM_START, NULL, 0);    
        }
        return post_orgasm_state.clench_duration_millis;

    } else {
        orgasm_detect = false;
        post_orgasm_state.clench_start_millis = current_time;
        post_orgasm_state.clench_duration_millis -= 150; // ms
        if (post_orgasm_state.clench_duration_millis <= 0) {
            post_orgasm_state.clench_duration_millis = 0;
            // clench pressure threshold value decays over time to a min of pressure + 1/2
            // sensitivity
            if ((p_check + (Config.clench_pressure_sensitivity / 2)) <
                post_orgasm_state.clench_pressure_threshold) {
                post_orgasm_state.clench_pressure_threshold *= 0.99;
            }
        }
        return 0;
    } // END of clench detector
}

void orgasm_control_twitch_detect() {
    if (arousal_state.arousal > Config.sensitivity_threshold) {
        output_state.motor_stop_time = (esp_timer_get_time() / 1000UL);
        arousal_state.in_cooldown = (Config.edge_delay * 1000) + output_state.random_additional_delay;
    }
}

orgasm_output_mode_t orgasm_control_get_output_mode(void) {
    return output_state.output_mode;
}

oc_bool_t orgasm_control_in_cooldown(void) {
    return arousal_state.in_cooldown;
}

const char* orgasm_control_get_output_mode_str(void) {
    if (output_state.output_mode < _OC_MODE_MAX) {
        return orgasm_output_mode_str[output_state.output_mode];
    } else {
        return "";
    }
}

orgasm_output_mode_t orgasm_control_str_to_output_mode(const char* str) {
    for (int i = 0; i < _OC_MODE_MAX; i++) {
        if (!strcasecmp(str, orgasm_output_mode_str[i])) {
            return (orgasm_output_mode_t)i;
        }
    }

    return -1;
}

oc_bool_t orgasm_control_is_recording() {
    return (oc_bool_t) !!logger_state.logfile;
}

void orgasm_control_tick() {
    if (Config.update_frequency_hz == 0) return;

    unsigned long millis = esp_timer_get_time() / 1000UL;
    unsigned long update_frequency_ms = 1000UL / Config.update_frequency_hz;

    if (millis - arousal_state.last_update_ms > update_frequency_ms) {
        orgasm_control_updateArousal();
        orgasm_control_updateEdgingTime();
        orgasm_control_updateMotorSpeed();
        arousal_state.last_update_ms = millis;

        if (Config.classic_serial || logger_state.logfile) {

            // Data for logfile or classic log.
            char data_csv[255];
            snprintf(
                data_csv,
                255,
                "%d,%d,%d,%d,%ld,%ld,%d",
                orgasm_control_get_average_pressure(),
                orgasm_control_get_arousal(),
                orgasm_control_get_motor_speed(),
                Config.sensitivity_threshold,
                post_orgasm_state.clench_pressure_threshold,
                post_orgasm_state.clench_duration_millis,
                orgasm_state.orgasm_count
            );

            // Write out to logfile, which includes millis:
            if (logger_state.logfile != NULL) {
                fprintf(
                    logger_state.logfile,
                    "%ld,%s\n",
                    arousal_state.last_update_ms - logger_state.recording_start_ms,
                    data_csv
                );
            }

            // Write to console for classic log mode:
            if (Config.classic_serial) {
                printf("%s\n", data_csv);
            }
        }
    }
}

oc_bool_t orgasm_control_updated() {
    return arousal_state.update_flag;
}

void orgasm_control_clear_update_flag(void) {
    arousal_state.update_flag = ocFALSE;
}

/**
 * Returns a normalized motor speed from 0..255
 * @return normalized motor speed byte
 */
uint8_t orgasm_control_get_motor_speed() {
    if (output_state.motor_speed > Config.motor_max_speed)
        return Config.motor_max_speed;
    else
        return (uint8_t)floor(output_state.motor_speed);
}

int orgasm_control_get_motor_speed_percent() {
    return (int)(orgasm_control_get_motor_speed() * 100.0f / Config.motor_max_speed);
}

uint16_t orgasm_control_get_arousal() {
    return arousal_state.arousal;
}

float orgasm_control_get_arousal_percent() {
    if (Config.sensitivity_threshold == 0) return 1.0;
    return (float)arousal_state.arousal / Config.sensitivity_threshold;
}

void orgasm_control_increment_arousal_threshold(int threshold) {
    orgasm_control_set_arousal_threshold(Config.sensitivity_threshold + threshold);
}

void orgasm_control_set_arousal_threshold(int threshold) {
    // Sensitivity threshold of 0 prevents horrible issues from happening.
    // It also prevents confusing the customers, which is a big win I'd say.
    Config.sensitivity_threshold = threshold >= 10 ? threshold : 10;
    config_enqueue_save(300);
}

int orgasm_control_get_arousal_threshold(void) {
    return Config.sensitivity_threshold;
}

uint16_t orgasm_control_get_last_pressure() {
    return arousal_state.pressure_value;
}

uint16_t orgasm_control_get_average_pressure() {
    return running_average_get_average(arousal_state.average);
}

void orgasm_control_control_motor(orgasm_output_mode_t control) {
    orgasm_control_set_output_mode(control);
}

void orgasm_control_trigger_arousal() {
    arousal_state.arousal = Config.sensitivity_threshold + 1;
    //arousal_state.in_cooldown = ocTRUE;
    output_state.motor_speed = 0;
    output_state.motor_stop_time = (esp_timer_get_time() / 1000UL);
    output_state.motor_start_time = 0;
    arousal_state.update_flag = ocTRUE;
}

void orgasm_control_set_output_mode(orgasm_output_mode_t mode) {
    orgasm_output_mode_t old = output_state.output_mode;
    output_state.output_mode = mode;
    output_state.control_motor = mode != OC_MANUAL;
    output_state.motor_stop_time = 0;
    //arousal_state.in_cooldown = ocFALSE; // reset cooldown on mode change

    if (old == OC_MANUAL) {
        const vibration_mode_controller_t* controller = orgasm_control_getVibrationMode();
        //output_state.motor_speed = controller->start(); //lets let the motor run at its former speed
    } else if (mode == OC_MANUAL) {
        const vibration_mode_controller_t* controller = orgasm_control_getVibrationMode();
        //output_state.motor_speed = controller->stop();  //lets let the motor run at its former speed
    }
    //event_manager_dispatch(EVT_MODE_SET, NULL, mode);
}

void orgasm_control_pause_control() {
    output_state.prev_control_motor = output_state.control_motor;
    output_state.control_motor = OC_MANUAL;
}

void orgasm_control_resume_control() {
    output_state.control_motor = output_state.prev_control_motor;
}

void orgasm_control_permit_orgasm(int seconds) {
    post_orgasm_state.detected_orgasm = ocFALSE;
    orgasm_control_set_output_mode(OC_ORGASM);
    post_orgasm_state.auto_edging_start_millis =
        (esp_timer_get_time() / 1000UL) - (Config.auto_edging_duration_minutes * 60 * 1000);
    post_orgasm_state.post_orgasm_duration_seconds = seconds;
}

oc_bool_t orgasm_control_is_permit_orgasm_reached() {
    // Detect if edging time has passed
    if ((esp_timer_get_time() / 1000UL) > (post_orgasm_state.auto_edging_start_millis +
                                           (Config.auto_edging_duration_minutes * 60 * 1000))) {
        return ocTRUE;
    } else {
        return ocFALSE;
    }
}

oc_bool_t orgasm_control_is_post_orgasm_reached() {
    // Detect if after orgasm
    if (post_orgasm_state.post_orgasm_start_millis > 0) {
        return ocTRUE;
    } else {
        return ocFALSE;
    }
}

oc_bool_t orgasm_control_is_menu_locked() {
    return post_orgasm_state.menu_is_locked;
};

void orgasm_control_lock_menu(oc_bool_t value) {
    post_orgasm_state.menu_is_locked = value;
}

void orgasm_control_reset_denied(void) {
    orgasm_state.orgasm_count = 0;
}
