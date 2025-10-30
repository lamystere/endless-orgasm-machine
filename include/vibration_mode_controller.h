#ifndef __VibrationModeController_h
#define __VibrationModeController_h

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vibration_pattern_step {
    uint8_t motor_speed;
    uint8_t hold_ticks;
    bool ramp_to;
} vibration_pattern_step_t;

typedef struct vibration_pattern {
    size_t step_count;
    vibration_pattern_step_t steps[];
} vibration_pattern_t;

typedef float (*vibration_mode_callback_t)(void);
typedef void (*vibration_mode_tick_func_t)(float motor_speed, uint16_t arousal);

typedef struct vibration_mode_controller {
    vibration_mode_callback_t start;
    vibration_mode_callback_t increment;
    vibration_mode_callback_t stop;
    vibration_mode_tick_func_t tick;
} vibration_mode_controller_t;

// Helper Functions

// Returns the increment per tick of a given span:
//   increment_per_second = (target - start) / time_s
//   increment_per_tick = increment_per_second / ticks_per_second
//
#define calculate_increment(start, target, time_s)                                                 \
    ((time_s > 0 && Config.update_frequency_hz > 0)                                                \
         ? ((float)(target - start) / ((float)time_s)) / Config.update_frequency_hz                \
         : 0)

// Vibration Modes
extern const vibration_mode_controller_t RampStopController;
extern const vibration_mode_controller_t EnhancementController;
extern const vibration_mode_controller_t DepletionController;
extern const vibration_mode_controller_t PatternController;

// Vibration Patterns
extern const vibration_pattern_t StepPattern;
extern const vibration_pattern_t WavePattern;

#ifdef __cplusplus
}
#endif

#endif