#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the timer subsystem. Call once at startup.
void timer_init(void);

// Start a cycling timer.
// cycle_interval_minutes: minutes between cycle starts.
// watering_duration_minutes: duration of each watering cycle.
// Returns true if the timer was successfully started.
bool timer_start_cycle(int cycle_interval_minutes, int watering_duration_minutes);

// Stop the cycling timer.
void timer_stop(void);

// Returns true when the cycling timer is active.
bool timer_is_running(void);

// Returns the number of seconds until the next watering cycle starts.
int timer_get_seconds_to_next_cycle(void);

// Returns the currently configured cycle interval in minutes.
int timer_get_cycle_interval_minutes(void);

// Returns the currently configured watering duration in minutes.
int timer_get_watering_duration_minutes(void);

// Register a callback that is called when a cycle starts.
// The callback should start the actual watering action.
void timer_register_callback(void (*callback)(void));

#ifdef __cplusplus
}
#endif

#endif // TIMER_H
