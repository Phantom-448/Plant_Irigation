#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sd_storage.h"
#include "pin_config.h"
#include <stdio.h>

static const char *TAG = "SD_CARD";
static sdmmc_card_t *card = NULL;
static bool sd_bus_initialized = false;
static sdmmc_host_t host = {0};

static bool ensure_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }

    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "Fehler beim Erstellen des Ordners %s", path);
        return false;
    }

    ESP_LOGI(TAG, "Ordner auf SD-Karte erstellt: %s", path);
    return true;
}

static bool init_spi_bus(void) {
    if (sd_bus_initialized) {
        return true;
    }

    host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 20000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_SD_MOSI,
        .miso_io_num = GPIO_SD_MISO,
        .sclk_io_num = GPIO_SD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fehler bei der Initialisierung des SPI-Busses: %s", esp_err_to_name(ret));
        return false;
    }

    sd_bus_initialized = true;
    return true;
}

static bool mount_sd_card_internal(void) {
    if (!init_spi_bus()) {
        return false;
    }

    if (card != NULL) {
        return true;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_SD_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Versuche SD-Karte zu mounten...");
    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Mounten der SD-Karte fehlgeschlagen (Konnte Dateisystem nicht lesen).");
        } else {
            ESP_LOGE(TAG, "Mounten der SD-Karte fehlgeschlagen (Code %s)", esp_err_to_name(ret));
        }
        card = NULL;
        return false;
    }

    ESP_LOGI(TAG, "SD-Karte erfolgreich gemountet.");
    ensure_directory(SD_LOG_DIR);
    ensure_directory(SD_PROFILE_DIR);
    return true;
}

static void unmount_sd_card_internal(void) {
    if (card == NULL) {
        return;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fehler beim Unmounten der SD-Karte: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SD-Karte unmounted.");
    }
    card = NULL;
}

void init_sd_card(void) {
    gpio_set_pull_mode(GPIO_SD_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SD_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SD_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SD_CS, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "Initialisiere SD-Karte über SPI...");
    mount_sd_card_internal();
}

bool sd_card_ready(void) {
    return card != NULL;
}

bool sd_card_self_test(void) {
    if (!sd_card_ready()) {
        ESP_LOGW(TAG, "SD-Karte nicht initialisiert. Selbsttest übersprungen.");
        return false;
    }

    const char *filepath = SD_MOUNT_POINT "/.sdcard_self_test";
    const char *marker = "sdcard_self_test";
    char buffer[64] = {0};

    ESP_LOGI(TAG, "Starte SD-Karten-Selbsttest: %s", filepath);

    FILE *f = fopen(filepath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Konnte Testdatei nicht schreiben: %s", filepath);
        return false;
    }

    if (fprintf(f, "%s", marker) < 0) {
        ESP_LOGE(TAG, "Schreiben in SD-Testdatei fehlgeschlagen.");
        fclose(f);
        remove(filepath);
        return false;
    }
    fclose(f);

    f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Konnte Testdatei nicht lesen: %s", filepath);
        remove(filepath);
        return false;
    }

    if (fgets(buffer, sizeof(buffer), f) == NULL) {
        ESP_LOGE(TAG, "Lesen der SD-Testdatei fehlgeschlagen.");
        fclose(f);
        remove(filepath);
        return false;
    }
    fclose(f);

    remove(filepath);

    if (strncmp(buffer, marker, strlen(marker)) != 0) {
        ESP_LOGE(TAG, "SD-Testdatei enthält falschen Inhalt: %s", buffer);
        return false;
    }

    ESP_LOGI(TAG, "SD-Karten-Selbsttest bestanden.");
    return true;
}

void sd_card_monitor_task(void *pvParameters) {
    while (1) {
        if (card != NULL) {
            struct stat st;
            if (stat(SD_MOUNT_POINT, &st) != 0) {
                ESP_LOGW(TAG, "SD-Mount nicht mehr erreichbar, versuche Unmount und erneuten Mount.");
                unmount_sd_card_internal();
            }
        }

        if (card == NULL) {
            if (mount_sd_card_internal()) {
                ESP_LOGI(TAG, "SD-Karte erfolgreich nach Einstecken gemountet.");
            } else {
                ESP_LOGD(TAG, "SD-Karte nicht verfügbar. Prüfe erneut in 5 Sekunden.");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
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