# 🪴 SmartIrrigation C3 – Smartes Balkon-Bewässerungssystem

Dieses Projekt baut ein lokal laufendes Bewässerungssystem für Balkonpflanzen auf Basis eines **ESP32-C3 (RISC-V)**. Es nutzt **FreeRTOS** für Multitasking, stellt ein **lokales Web-Dashboard** bereit und speichert **Logs und Bewässerungsprofile auf einer SD-Karte**.

## 🎯 Projektidee

Das System ist bewusst ohne Cloud konzipiert:
* lokale Entscheidungslogik für smarte Bewässerung
* Web-Dashboard direkt auf dem ESP32-C3
* Profile und Logs auf SD-Karte gespeichert
* thread-sicherer globaler Systemzustand über FreeRTOS-Mutex

## 📌 Hardware & Pin-Konfiguration

Die Hardware-Pins sind zentral in `main/pin_config.h` abgelegt. Dort findest du die wichtigsten Zuordnungen für Relais, SD-Karte und I2C.

### Aktuelle GPIO-Belegung

| Funktion | Makro | Pin |
|---|---|---|
| Relais / Pumpe | `GPIO_RELAY` | GPIO 10 |
| SD-Karte MISO | `GPIO_SD_MISO` | GPIO 5 |
| SD-Karte MOSI | `GPIO_SD_MOSI` | GPIO 6 |
| SD-Karte SCLK | `GPIO_SD_SCLK` | GPIO 4 |
| SD-Karte CS | `GPIO_SD_CS` | GPIO 7 |
| I2C SDA (Sensor) | `GPIO_I2C_SDA` | GPIO 8 |
| I2C SCL (Sensor) | `GPIO_I2C_SCL` | GPIO 9 |

### SD-Dateipfade

`main/pin_config.h` definiert auch die Pfade für das SD-Dateisystem:
* `SD_MOUNT_POINT` → `/sdcard`
* `SD_LOG_DIR` → `/sdcard/logs`
* `SD_PROFILE_DIR` → `/sdcard/profiles`
* `SD_LOG_FILE` → `/sdcard/logs/sensor_log.csv`

Diese Pfade werden im SD-Modul verwendet, damit alle Dateizugriffe konsistent bleiben.


## 🧱 Software-Architektur

Das Projekt ist modular aufgebaut, damit jede Komponente nur eine Aufgabe übernimmt.

### Wichtige Module

* `main.c` – Systemstart, Initialisierung, zyklische Smart-Logik
* `state.c` / `state.h` – globaler Systemzustand mit Mutex
* `actor.c` / `actor.h` – Pumpen-/Relaissteuerung
* `wlan.c` / `wlan.h` – WiFi-Station und Webserver-Start
* `webserver.c` / `webserver.h` – HTTP-Server und REST-API
* `timer.c` / `timer.h` – Bewässerungs- und Logging-Timer
* `logger.c` / `logger.h` – CSV-Logging von Sensordaten
* `sd_storage.c` / `sd_storage.h` – SD-Karten-Mount, Log- und Profilzugriff
* `profile_manager.c` / `profile_manager.h` – Scannen und Aktivieren von Bewässerungsprofilen
* `temp.c` / `temp.h` – Temperaturmessung und Filterung
* `humid.c` / `humid.h` – Feuchtigkeitsmessung und Filterung

## 📊 Kernfunktionen

### Web-Dashboard
Das Web-Dashboard wird eingebettet und lokal ausgeliefert. Es zeigt:
* Temperatur
* Luftfeuchtigkeit
* Bodenfeuchtigkeit
* Countdown bis zur nächsten automatischen Bewässerung
* Profilauswahl
* Zyklus- und Dauersteuerung
* Direkten Pumpenschalter

### SD-Karte & Profile
Die SD-Karte wird per SPI eingebunden und automatisiert in folgende Ordner strukturiert:
* `/sdcard/logs`
* `/sdcard/profiles`

Profildateien liegen als `.json` im Profilordner und können per Web-API geladen werden.

### Thread-sicherer Zustand
Alle gemeinsamen Daten werden in `sys_state` gehalten und nur nach erfolgreichem `xSemaphoreTake(state_mutex, ...)` gelesen oder geschrieben. So werden Race Conditions vermieden.

## 🔧 Build & Nutzung

### Voraussetzungen
* Installierte ESP-IDF Umgebung
* Funktionierende SD-Karte
* WLAN-Zugangsdaten via `menuconfig`

### Build
```bash
cd "Smart_Plant_Irigation"
idf.py menuconfig
idf.py build
idf.py flash
```

### Laufend
* Das Web-Dashboard ist nach dem Start über die IP-Adresse des ESP erreichbar oder auch über bewaesserung.local
* Profile werden beim Start von der SD-Karte eingelesen
* Logs werden periodisch in `sensor_log.csv` geschrieben

## 💡 Hinweise

* `humid.c` ist als I2C-Sensor vorbereitet, liefert aktuell aber simulierte Messwerte.
* `temp.c` nutzt den internen Temperatursensor des ESP32-C3.
* Alle Hardware-Pins werden aus `main/pin_config.h` geladen.

---

Mit dieser Struktur kannst du das Projekt leicht erweitern: echte I2C-Sensoren, Profil-Logik oder weitere Relaisausgänge lassen sich zentral konfigurieren.