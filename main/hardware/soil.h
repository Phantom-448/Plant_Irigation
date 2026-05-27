#ifndef MAIN_HARDWARE_SOIL_H
#define MAIN_HARDWARE_SOIL_H

#include <stdint.h>
#include "hal/adc_types.h" // <-- Neuer Header für die ADC-Typen

/* ADC channel to use: Für GPIO0 (Pin IO0 auf dem C3) nutzt man ADC_CHANNEL_0 */
/* Da auf dem C3 ADC1 für Sensoren genutzt wird, ist ADC_CHANNEL_0 korrekt */
#define SOIL_ADC_CHANNEL ADC_CHANNEL_0

#define SOIL_SENSOR_INVERT 1 // Kapazitive Sensoren sind oft invertiert (Trocken = hoher Wert)

int read_soil_moisture(void);

#endif // MAIN_HARDWARE_SOIL_H