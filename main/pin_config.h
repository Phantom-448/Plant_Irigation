#pragma once

// GPIO definitions for the ESP32-C3 smart balcony irrigation system
#define GPIO_RELAY           10

// SPI pins for the SD-card module
#define GPIO_SD_MISO         5
#define GPIO_SD_MOSI         6
#define GPIO_SD_SCLK         4
#define GPIO_SD_CS           7

// I2C pins for external sensor hardware (optional/test setup)
#define GPIO_I2C_SDA         8
#define GPIO_I2C_SCL         9

// SD card filesystem mount points and directories
#define SD_MOUNT_POINT       "/sdcard"
#define SD_LOG_DIR           SD_MOUNT_POINT "/logs"
#define SD_PROFILE_DIR       SD_MOUNT_POINT "/profiles"
#define SD_LOG_FILE          SD_LOG_DIR "/sensor_log.csv"
