#pragma once

// --- AKTUELLE HARDWARE VERKABELUNG ---

// SPI pins für das SD-Karten-Modul (gemäß Schematic & Jumper-Konfiguration)
#define GPIO_SD_MOSI         27
#define GPIO_SD_MISO         25
#define GPIO_SD_SCLK         26
#define GPIO_SD_CS           14    

// GPIO für das Relais (Wasserpumpe)
#define GPIO_RELAY           4   

// DHT22 Pin (falls angeschlossen)
#define GPIO_DHT22           12

#define GPIO_SOIL_MOISTURE   36    // ADC1_CHANNEL_0, für Bodenfeuchtigkeitssensor

// GPIO für den manuellen Taster (Pulldown)
#define GPIO_BUTTON          13

// SD card filesystem mount points and directories
#define SD_MOUNT_POINT       "/sdcard"
#define SD_LOG_DIR           SD_MOUNT_POINT "/logs"
#define SD_PROFILE_DIR       SD_MOUNT_POINT "/profiles"
#define SD_LOG_FILE          SD_LOG_DIR "/sensor_log.csv"

// Set to 1 to use a simulated temperature sensor (no hardware). Set to 0 to use DHT22 if available.
#define TEMP_SIMULATED       0

// --- COMMON CONSTANTS ---
#define MUTEX_TIMEOUT_MS     100  // Timeout for state_mutex operations (milliseconds)