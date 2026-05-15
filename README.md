# 🪴 SmartIrrigation C3 – Smarte Balkon-Bewässerung

Dieses Projekt realisiert eine automatisierte und fernsteuerbare Bewässerungsanlage für Balkonpflanzen. Herzstück des Systems ist ein **ESP32-C3 (RISC-V)**, der sowohl die Sensorik auswertet als auch die Aktoren (Pumpen/Ventile) über einen Port-Expander steuert.

## 🎯 Motivation
Pflanzen auf dem Balkon sind besonders im Sommer extremen Bedingungen ausgesetzt. Durch die starke Sonneneinstrahlung untertags reicht eine einmalige Bewässerung oft nicht aus. Dieses System sorgt dafür, dass:
* Die Feuchtigkeit kontinuierlich überwacht wird.
* Pflanzen auch bei Abwesenheit bedarfsgerecht Wasser erhalten.
* Die Steuerung bequem per Smartphone oder Home Assistant erfolgt.

---

## 🏗 Systemarchitektur

Das System ist modular aufgebaut, um eine hohe Wartbarkeit und Ausfallsicherheit zu gewährleisten. Die Software basiert auf dem **Espressif IoT Development Framework (ESP-IDF)**.

### Hardware-Komponenten
* **Mikrocontroller:** ESP32-C3 (RISC-V Architektur).
* **I/O-Erweiterung:** MCP23017 Port-Expander (via I2C) zur Ansteuerung von bis zu 16 Relais/Ventilen.
* **Sensorik:**
    * Interner System-Temperatursensor (Chip-Überwachung).
    * Externer Luftfeuchtigkeits- und Temperatursensor (via I2C).
    * Bodenfeuchtigkeitssensoren (kapazitiv).
* **Aktorik:** 12V Wasserpumpe und Magnetventile, geschaltet über Relais.

### Software-Module (C-Files)
Die Logik ist in spezialisierte Module unterteilt:
* `wlan.c`: Verwaltet die WiFi-Verbindung (Station Mode) inkl. Kconfig-Integration für sichere Credentials.
* `state.c`: Zentrales State-Management mit **FreeRTOS Mutex** zur thread-sicheren Datenhaltung.
* `temp.c` / `humid.c`: Sensor-Auslesung inklusive digitaler Signalverarbeitung (**IIR-Filter** zur Messwertglättung).
* `webserver.c`: Bereitstellung eines REST-API Backends und eines Bootstrap-basierten Dashboards.
* `mqtt_ha.c`: Integration in **Home Assistant** mittels MQTT Auto-Discovery.
* `mcp23017.c`: Treiber für die I2C-Kommunikation mit dem Port-Expander.

---

## 📊 Features

### 📱 Web-Dashboard
Ein mobiles Dashboard auf Basis von **Bootstrap 5**, das direkt vom ESP32-C3 ausgeliefert wird.
* **Monitoring:** Echtzeitanzeige von Temperatur, Luft- und Bodenfeuchtigkeit.
* **Ladebalken:** Visualisierung des Zeitraums bis zur nächsten automatischen Bewässerung.
* **Steuerung:** Manuelles Starten/Stoppen der Pumpe sowie Schieberegler für Bewässerungsdauer und Pumpenleistung (PWM).

### 🏠 Home Assistant Integration
Vollständige Einbindung in das Smart Home über den **Mosquitto MQTT Broker**:
* Automatisches Erkennen des Geräts (Auto-Discovery).
* Zustandsübermittlung und Fernsteuerung über die Home Assistant App.

### 🛡 Signalverarbeitung & Sicherheit
* **IIR-Filter:** Mathematische Glättung der Sensorwerte, um Fehlsteuerungen durch Rauschen zu vermeiden.
* **NVS Integration:** Dauerhaftes Speichern von Konfigurationen (z.B. Zeitpläne) im Flash-Speicher, sodass diese nach einem Stromausfall erhalten bleiben.

---

## 🚀 Installation & Build

### Voraussetzungen
* Installiertes **ESP-IDF** (Espressif IDE oder VS Code Plugin).
* Aktiver MQTT Broker (z.B. Home Assistant Add-on).

### Build-Prozess
1. Repository klonen.
2. Projektkonfiguration öffnen:
   ```bash
   idf.py menuconfig