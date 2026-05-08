#ifndef ACTOR_H
#define ACTOR_H
#include <stdbool.h>

void relay_init(void);
void actor_set_relay(bool state);
void actor_start_timed_watering(int minutes);

#endif // ACTOR_H

