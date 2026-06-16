# 🪴 SmartIrrigation C3 – Smartes Balkon-Bewässerungssystem

Dieses Projekt baut ein lokal laufendes Bewässerungssystem für Balkonpflanzen auf Basis eines **ESP32-C3 (RISC-V)**. Es nutzt **FreeRTOS** für Multitasking, stellt ein **lokales Web-Dashboard** bereit und speichert **Logs und Bewässerungsprofile auf einer SD-Karte**.

## 🎯 Projektidee

* lokale Entscheidungslogik für smarte Bewässerung
* Web-Dashboard direkt auf dem ESP32-C3
* Profile und Logs auf SD-Karte gespeichert
* thread-sicherer globaler Systemzustand über FreeRTOS-Mutex

## 📌 Hardware & Pin-Konfiguration

Die Hardware-Pins sind zentral in `main/pin_config.h` abgelegt. Dort findest du die wichtigsten Zuordnungen für Relais, SD-Karte, Sensoren und den manuellen Taster.

### Aktuelle GPIO-Belegung

| Funktion | Makro | Pin |
|---|---|---|
| Relais / Pumpe | `GPIO_RELAY` | GPIO 4 |
| SD-Karte MOSI | `GPIO_SD_MOSI` | GPIO 27 |
| SD-Karte MISO | `GPIO_SD_MISO` | GPIO 25 |
| SD-Karte SCLK | `GPIO_SD_SCLK` | GPIO 26 |
| SD-Karte CS | `GPIO_SD_CS` | GPIO 14 |
| Air Sensor (DHT22) | `GPIO_DHT22` | GPIO 12 |
| Bodenfeuchtigkeit (ADC1) | `GPIO_SOIL_MOISTURE` | GPIO 36 |
| Manueller Taster (Pulldown) | `GPIO_BUTTON` | GPIO 13 |

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

* `main.c` – Systemstart, Initialisierung und zyklische Smart-Logik
* `state.c` / `state.h` – globaler Systemzustand mit Mutex-Schutz
* `actor.c` / `actor.h` – Pumpen-/Relaissteuerung
* `wlan.c` / `wlan.h` – WiFi-Station und Webserver-Start
* `webserver.c` / `webserver.h` – HTTP-Server und REST-API
* `timer.c` / `timer.h` – Bewässerungs- und Logging-Timer
* `logger.c` / `logger.h` – CSV-Logging von Sensordaten
* `sd_storage.c` / `sd_storage.h` – SD-Karten-Mount, Log- und Profilzugriff
* `profile_manager.c` / `profile_manager.h` – Scannen und Aktivieren von Bewässerungsprofilen
* `air_sensor.c` / `air_sensor.h` – Air Sensor-Temperatur-/Feuchtigkeitsmessung
* `soil.c` / `soil.h` – Bodenfeuchtemessung
* `pin_config.h` – zentrale Pin- und Pfaddefinitionen

## 🔄 Prozessablauf

Der Projektfluss umfasst die Initialisierung, zyklische Sensorauswertung, automatische Bewässerung und Web-Dashboard-Steuerung.

```mermaid
flowchart TD
    A[Start: ESP32-C3 bootet] --> B[NVS & Systeminit]
    B --> C[Pin- & Peripherie-Initialisierung]
    C --> D[Button init + SD-Mount]
    D --> E[Timer & Logging starten]
    E --> F[WiFi / Webserver optional]
    F --> H[Sensorlese-Task startet]
    

    subgraph Web UI
        W[Web-Dashboard wird geladen]
        W --> X[HTTP-API-Anfragen an /api/status]
        X --> Y[Sensor- und Timerwerte anzeigen]
        W --> Z[Steuerbefehle an API senden]
    end

    subgraph Automatische Bewässerung
        I[Timer-Event] --> J[State lesen]
        J --> K{Bodenfeuchte OK?}
        K -- Ja --> L[Keine Bewässerung]
        K -- Nein --> M[Dauer berechnen]
        M --> N[Pumpe für Dauer einschalten]
    end

    subgraph Manueller Taster
        O[Button gedrückt] --> P[Direkter Bewässerungsstart]
        P --> N
    end

    H --> I
    F --> W
    Z --> N
```

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

### Webserver-API
Die Weboberfläche kommuniziert über lokale REST-API-Endpunkte mit dem ESP32.

| Methode | Pfad | Payload | Beschreibung |
|---|---|---|---|
| GET | `/api/status` | - | Systemstatus, Sensorwerte, Timer, Pumpenstatus |
| POST | `/api/cycle` | `{ "cycle_interval_minutes": 60, "watering_duration_minutes": 5 }` | Zyklus- und Bewässerungsdauer setzen |
| POST | `/api/relay` | `{ "state": true }` | Relaiszustand ein-/ausschalten |
| POST | `/api/start_watering` | `{ "duration_min": 10 }` | Manuelle Bewässerung starten |
| GET | `/api/profiles` | - | Liste verfügbarer Profile als JSON-Array |
| POST | `/api/profile/activate` | `{ "profile_name": "Tomaten" }` | Profil laden und aktivieren |
| GET | `/api/logs` | - | Sensorlog als CSV-Datei herunterladen |
| GET | `/api/sdcard/test` | - | SD-Karten-Integrität prüfen |

### Beispiel-API-Aufrufe
```bash
curl http://<esp-ip>/api/status
curl -X POST http://<esp-ip>/api/cycle -H 'Content-Type: application/json' -d '{"cycle_interval_minutes":45,"watering_duration_minutes":6}'
curl -X POST http://<esp-ip>/api/relay -H 'Content-Type: application/json' -d '{"state":true}'
curl -X POST http://<esp-ip>/api/start_watering -H 'Content-Type: application/json' -d '{"duration_min":8}'
curl http://<esp-ip>/api/profiles
curl -X POST http://<esp-ip>/api/profile/activate -H 'Content-Type: application/json' -d '{"profile_name":"Tomaten"}'
curl http://<esp-ip>/api/logs
curl http://<esp-ip>/api/sdcard/test
```

### Web-Dashboard-Integration
Die HTML-Oberfläche wird über `/` ausgeliefert und verwendet diese Endpunkte, um live Sensorwerte zu laden, den Laufzeitzyklus zu konfigurieren, Profile zu wechseln und die Pumpe zu steuern.

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
* Das Web-Dashboard ist nach dem Start über die IP-Adresse des ESP erreichbar oder auch über `bewaesserung.local`
* Profile werden beim Start von der SD-Karte eingelesen
* Logs werden periodisch in `sensor_log.csv` geschrieben

## 💡 Hinweise

* `air_sensor.c` ist die aktuelle Sensoranbindung für Temperatur und Luftfeuchte.
* `temp.c` kann zusätzlich den internen C3-Temperatursensor nutzen.
* Der manuelle Taster (`GPIO_BUTTON`) ist als Pulldown-Taster ausgelegt.
* Alle Hardware-Pins werden aus `main/pin_config.h` geladen.

---

Mit dieser Struktur kannst du das Projekt leicht erweitern: zusätzliche Sensoren, neue Profile oder weitere Relaisausgänge lassen sich zentral konfigurieren.