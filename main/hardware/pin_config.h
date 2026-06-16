#pragma once

// --- AKTUELLE HARDWARE VERKABELUNG ---

// SPI pins für das SD-Karten-Modul (gemäß Schematic & Jumper-Konfiguration)
#define GPIO_SD_MOSI         27
#define GPIO_SD_MISO         25
#define GPIO_SD_SCLK         26
#define GPIO_SD_CS           14    

// GPIO für das Relais
#define GPIO_RELAY           4   

// DHT11 Pin 
#define GPIO_AIR_SENSOR      33

#define GPIO_SOIL_MOISTURE   36    // ADC1_CHANNEL_0, für Bodenfeuchtigkeitssensor

// GPIO für den manuellen Taster (Pulldown)
#define GPIO_BUTTON          13

// SD card filesystem mount points and directories
#define SD_MOUNT_POINT       "/sdcard"
#define SD_LOG_DIR           SD_MOUNT_POINT "/logs"
#define SD_PROFILE_DIR       SD_MOUNT_POINT "/profiles"
#define SD_LOG_FILE          SD_LOG_DIR "/sensor_log.csv"


#define MUTEX_TIMEOUT_MS     100  // Timeout for state_mutex operations (milliseconds)