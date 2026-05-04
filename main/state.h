#pragma once // Verhindert, dass der Header doppelt geladen wird
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Die Struktur für unseren Systemzustand
typedef struct {
    bool valve_1_state;
    bool valve_2_state;
    int watering_duration;
} SystemState_t;

// 'extern' sagt dem Compiler: Diese Variable existiert, ist aber in der .c Datei definiert
extern SystemState_t sys_state;
extern SemaphoreHandle_t state_mutex;

// Funktion zum Initialisieren des Mutex
void state_init(void);