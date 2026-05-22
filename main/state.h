#pragma once // Verhindert, dass der Header doppelt geladen wird
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// In state.h - oberhalb von SystemState_t einfügen

typedef struct {
    char profile_name[32];
    int check_interval_minutes;       // Wie oft soll geprüft werden? (z.B. alle 60 Min)
    int base_watering_minutes;        // Standard-Dauer
    int min_soil_moisture_percent;    // Schwellenwert: Nur wässern, wenn trockener als X %
    float hot_temp_threshold;         // Ab welcher Temp gilt es als "heiß"? (z.B. 30.0 °C)
    int hot_temp_extra_minutes;       // Wie viele Minuten extra, wenn es heiß ist?
} WateringProfile_t;

// Deine bestehende Struktur erweitern:
typedef struct {
    bool valve_1_state;
    int watering_duration;
    float current_temp;
    float air_humidity;
    int soil_moisture_1;
    int soil_moisture_2;
    
    WateringProfile_t active_profile; // <--- NEU: Das aktuell geladene Profil
} SystemState_t;

// 'extern' sagt dem Compiler: Diese Variable existiert, ist aber in der .c Datei definiert
extern SystemState_t sys_state;
extern SemaphoreHandle_t state_mutex;

// Funktion zum Initialisieren des Mutex
void state_init(void);