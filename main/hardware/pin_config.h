#pragma once

// --- AKTUELLE HARDWARE VERKABELUNG ---

// SPI pins für das SD-Karten-Modul (gemäß Schematic & Jumper-Konfiguration)
#define GPIO_SD_MOSI         4
#define GPIO_SD_MISO         7
#define GPIO_SD_SCLK         10
#define GPIO_SD_CS           0    

// I2C pins für externen Feuchtigkeitssensor (oder andere I2C Geräte)
#define GPIO_I2C_SDA         5
#define GPIO_I2C_SCL         6

// GPIO für das Relais (Wasserpumpe) - Wir nehmen Pin 8 als Ersatz, da 10 belegt ist
#define GPIO_RELAY           8   

// DHT22 Pin (falls angeschlossen)
#define GPIO_DHT22           2

// Optionaler kapazitiver Luftfeuchtesensor (VCC, GND, DATA)
// Uncomment and set the pin if you use a second one-wire humidity sensor
// #define GPIO_CHUMID         3

// GPIO für den manuellen Taster (Pulldown)
#define GPIO_BUTTON          12

// SD card filesystem mount points and directories
#define SD_MOUNT_POINT       "/sdcard"
#define SD_LOG_DIR           SD_MOUNT_POINT "/logs"
#define SD_PROFILE_DIR       SD_MOUNT_POINT "/profiles"
#define SD_LOG_FILE          SD_LOG_DIR "/sensor_log.csv"

// Set to 1 to use a simulated temperature sensor (no hardware). Set to 0 to use DHT22 if available.
#define TEMP_SIMULATED       0