#include "api/index.h"
//#include "eom-hal.h"
#include "esp_log.h"
#include "orgasm_control.h"
#include "system/websocket_handler.h"

static const char* TAG = "api/edging";

static command_err_t
cmd_edging_set_mode(cJSON* command, cJSON* response, websocket_client_t* client) {
    const char* mode_str = command->valuestring;
    orgasm_output_mode_t mode = orgasm_control_str_to_output_mode(mode_str);

    ESP_LOGI(TAG, "orgasm_control_str_to_output_mode(%s) = %d", mode_str, mode);

    // Legacy compatibility fallback:
    if (mode == _OC_MODE_ERROR) {
        if (!strcasecmp("automatic", mode_str)) {
            mode = OC_AUTOMATIC;
        } else if (!strcasecmp("manual", mode_str)) {
            mode = OC_MANUAL;
        }
    }

    if (mode != _OC_MODE_ERROR) {
        orgasm_control_set_output_mode(mode);
        cJSON_AddStringToObject(response, "mode", orgasm_control_get_output_mode_str());
        return CMD_OK;
    } else {
        return CMD_ARG_ERR;
    }
}

static const websocket_command_t cmd_edging_set_mode_s = {
    .command = "setMode",
    .func = &cmd_edging_set_mode,
};

static command_err_t
cmd_edging_set_motor(cJSON* command, cJSON* response, websocket_client_t* client) {
    int speed = command->valueint;
    if (orgasm_control_get_output_mode() != OC_MANUAL) {
        orgasm_control_set_output_mode(OC_MANUAL);
        cJSON_AddStringToObject(response, "mode", orgasm_control_get_output_mode_str());
    }

    orgasm_control_set_motor_speed(speed);
    cJSON_AddNumberToObject(response, "speed", speed);
    return CMD_OK;
}

static const websocket_command_t cmd_edging_set_motor_s = {
    .command = "setMotor",
    .func = &cmd_edging_set_motor,
};

static command_err_t
cmd_edging_reset_denied(cJSON* command, cJSON* response, websocket_client_t* client) {
    orgasm_control_reset_denied();
    cJSON_AddNumberToObject(response, "denied", 0);
    return CMD_OK;
}

static const websocket_command_t cmd_edging_reset_denied_s = {
    .command = "resetDenied",
    .func = &cmd_edging_reset_denied,
};

static command_err_t cmd_edging_trigger_arousal(cJSON* command, cJSON* response, websocket_client_t* client) {
    orgasm_control_trigger_arousal();
    cJSON_AddNumberToObject(response, "arousal", orgasm_control_get_arousal());
    return CMD_OK;
}

static const websocket_command_t cmd_edging_trigger_arousal_s = {
    .command = "triggerArousal",
    .func = &cmd_edging_trigger_arousal,
};

void api_register_edging(void) {
    websocket_register_command(&cmd_edging_set_mode_s);
    websocket_register_command(&cmd_edging_set_motor_s);
    websocket_register_command(&cmd_edging_reset_denied_s);
    websocket_register_command(&cmd_edging_trigger_arousal_s);
}