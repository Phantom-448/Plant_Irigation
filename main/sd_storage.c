#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "sd_storage.h"
#include "pin_config.h"

static const char *TAG = "SD_CARD";


// Globale Variable für die Karte
sdmmc_card_t *card;

void init_sd_card(void) {
    esp_err_t ret;

    // 1. Konfiguration des VFS (Virtual File System)
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true, // Formatiert die Karte, wenn sie komplett leer ist
        .max_files = 5,                 // Max. gleichzeitig geöffnete Dateien
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Initialisiere SD-Karte über SPI...");

    // 2. SPI-Bus konfigurieren
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_SD_MOSI,
        .miso_io_num = GPIO_SD_MISO,
        .sclk_io_num = GPIO_SD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    // SPI Bus starten (Standard DMA Kanal)
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fehler beim Initialisieren des SPI Busses.");
        return;
    }

    // 3. Den Chip-Select Pin an den Host binden
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_SD_CS;
    slot_config.host_id = host.slot;

    // 4. Mounten! (Das verbindet den SPI-Treiber mit dem Dateisystem)
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mounten der SD-Karte fehlgeschlagen (Code %s)", esp_err_to_name(ret));
        return;
    }
    
    // Wenn wir hier ankommen, war es erfolgreich
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD-Karte erfolgreich gemountet unter %s", SD_MOUNT_POINT);

    // Basisordner für Logs und Profile anlegen, falls nicht vorhanden
    mkdir(SD_LOG_DIR, 0755);
    mkdir(SD_PROFILE_DIR, 0755);
}

// Ein Profil speichern (z.B. "sommer.json")
void save_profile(const char* profil_name, const char* json_string) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", SD_PROFILE_DIR, profil_name);

    ESP_LOGI(TAG, "Öffne Datei zum Schreiben: %s", filepath);
    FILE* f = fopen(filepath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Fehler: Konnte %s nicht öffnen", filepath);
        return;
    }
    
    fprintf(f, "%s", json_string);
    fclose(f);
    ESP_LOGI(TAG, "Profil '%s' erfolgreich gespeichert.", profil_name);
}

void sd_write_log(const char* log_line) {
    FILE* f = fopen(SD_LOG_FILE, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Fehler: Konnte Log-Datei nicht öffnen!");
        return;
    }
    fprintf(f, "%s\n", log_line);
    fclose(f);
}

// Ein Profil laden und auf der Konsole ausgeben
void load_profile(const char* profil_name) {
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", SD_PROFILE_DIR, profil_name);

    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "Profil '%s' existiert noch nicht.", filepath);
        return;
    }

    // Datei einlesen
    char line[128];
    ESP_LOGI(TAG, "--- Lese Profil: %s ---", profil_name);
    while (fgets(line, sizeof(line), f) != NULL) {
        printf("%s", line); // Gibt das JSON im Terminal aus
    }
    printf("\n");
    fclose(f);
}