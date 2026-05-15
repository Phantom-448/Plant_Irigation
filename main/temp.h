#pragma once // Verhindert, dass der Header doppelt geladen wird
#include <stdbool.h>

// Initialisiert den internen Sensor und den Filter
void temp_sensor_init(void);

// Liefert den aktuell gefilterten Temperaturwert zurück
float temp_sensor_read_filtered(void);