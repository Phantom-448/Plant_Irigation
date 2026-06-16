# 🪴 SmartIrrigation C3 – Projektdokumentation

## 📋 Projektübersicht

**SmartIrrigation C3** ist ein intelligentes, lokal laufendes Bewässerungssystem für Balkonpflanzen, basierend auf einem **ESP32-C3 Microcontroller**. Das System arbeitet völlig offline ohne Cloud-Abhängigkeit, verfügt über ein Web-Dashboard und speichert alle Logs und Bewässerungsprofile auf einer **SD-Karte**.

### 🎯 Kernziele

- ✅ **Lokale Intelligenz**: Smart-Logik zur Bewässerungsentscheidung basierend auf Bodenfeuchtigkeit und Luftfeuchte
- ✅ **Offline-Betrieb**: Keine Cloud-Abhängigkeit, lokales Web-Dashboard auf dem ESP32
- ✅ **Datenspeicherung**: Vollständige Sensordaten und Bewässerungsprotokolle auf SD-Karte
- ✅ **Multitasking**: FreeRTOS für parallele Sensorauslesungen und Systemoperationen
- ✅ **Manuell & Auto-Modi**: Taster für manuelle Bewässerung, zyklische automatische Bewässerung

---

## 🏗️ System-Architektur

Das Projekt folgt einer **modularen Architektur** mit klarer Separation of Concerns:

```
┌─────────────────────────────────────────────────────────────────┐
│                      APPLIKATIONSEBENE                          │
│  main.c – Systemstart, Initialisierung, Task-Verwaltung         │
└─────────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
┌───────▼──────┐   ┌──────────▼────────┐   ┌──────▼─────────┐
│  HARDWARE    │   │  LOGIK & STATE    │   │   KOMMUNIKAT.  │
│  LAYER       │   │                   │   │   & SPEICHER    │
├──────────────┤   ├───────────────────┤   ├─────────────────┤
│• actor       │   │• state            │   │• webserver      │
│• button      │   │• timer            │   │• wlan           │
│• soil        │   │• smart_logic      │   │• sd_storage     │
│• air_sensor  │   │• profile_manager  │   │• logger         │
│• pin_config  │   │• logger           │   │                 │
└──────────────┘   └───────────────────┘   └─────────────────┘
```

---

## 📍 Hardware-Konfiguration

Alle GPIO-Zuordnungen sind zentral in `main/pin_config.h` definiert:

| Komponente | Funktion | GPIO-Pin | Typ |
|---|---|---|---|
| **Pumpen-Relais** | Wasserpumpe steuern | GPIO 4 | Digital Output |
| **Bodenfeuchtesensor** | Bodenfeuchtigkeit messen | GPIO 36 (ADC1) | Analog Input |
| **Air Sensor (DHT22)** | Temperatur & Luftfeuchte | GPIO 12 | Digital (1-Wire) |
| **Manueller Button** | Manuelle Bewässerung | GPIO 13 (Pulldown) | Digital Input |
| **SD-Karte MOSI** | SPI Datenleitung | GPIO 27 | SPI |
| **SD-Karte MISO** | SPI Datenleitung | GPIO 25 | SPI |
| **SD-Karte SCLK** | SPI Clock | GPIO 26 | SPI |
| **SD-Karte CS** | SPI Chip Select | GPIO 14 | SPI |

### 📁 SD-Karte Dateisystem

```
/sdcard/
├── logs/
│   └── sensor_log.csv        # Sensor-Telemetrie (Temp, Luftfeuchte, Bodenfeuchte)
└── profiles/
    └── [profile_name].json   # Bewässerungsprofile mit Schwellwerten
```

---

## 🧩 Modulare Komponenten

### 1. **Core State Management** (`state.c/h`)
- Globaler Systemzustand (`sys_state`)
- **Thread-Sicherheit**: FreeRTOS Mutex zur Vermeidung von Race Conditions
- Struktur enthält: Sensordaten, aktive Profile, Systemstatus

### 2. **Hardware-Treiber** (`hardware/`)

#### `actor.c/h` – Pumpensteuerung
- `actor_start_timed_watering()`: Startet Bewässerung für X Minuten
- `actor_stop()`: Stoppt Bewässerung
- Relais-Ansteuerung über GPIO 4

#### `soil.c/h` – Bodenfeuchte
- `read_soil_moisture()`: Liest ADC-Wert von GPIO 36
- Gibt Wert 0-100% zurück
- Fallback: Simuliert Werte bei Sensorfehler

#### `air_sensor.c/h` – DHT22
- `air_sensor_read()`: Liest Temperatur und Luftfeuchte
- Fallback-Werte bei Fehler
- Unterstützt GPIO 12 (1-Wire Protokoll)

#### `button.c/h` – Manuelle Steuerung
- Interrupt-basiert auf GPIO 13
- Callback: `button_pressed_handler()`
- Startet sofort Bewässerung + neuen Zyklus

### 3. **Logik & Timer** (`logic/`)

#### `timer.c/h` – Zyklisches System
- `timer_start_cycle()`: Startet periodischen Bewässerungs-Check
- Registriert Callbacks für jeden Zyklus
- Nutzt `smart_logic_evaluate()` zur Entscheidung

#### `smart_logic.c/h` – Intelligente Bewässerung
- **Entscheidungslogik**:
  - ✅ Gießen wenn: Bodenfeuchte < Schwelle UND Luftfeuchte < Max
  - ❌ Nicht gießen wenn: Zu nass ODER Luft zu feucht (Staunässe-Prävention)
- Nutzt aktives Profil aus `profile_manager`

#### `profile_manager.c/h` – Bewässerungsprofile
- Speichert Schwellwerte und Check-Intervalle
- Lädt Profile von SD-Karte
- Mehrere Profile möglich (z.B. "Tomaten", "Erdbeeren", "Basilikum")

#### `logger.c/h` – Datenlogging
- Schreibt Sensordaten periodisch auf SD-Karte
- CSV-Format für einfache Analyse
- Task: `xTaskCreate(..., "logging_task", ...)`

### 4. **Kommunikation** (`web/`)

#### `wlan.c/h` – WiFi-Verbindung
- `wifi_init_sta()`: Verbindung zu SSID/Passwort
- Startet mDNS-Service (Zugriff via `http://smartirrig.local`)

#### `webserver.c/h` – REST-API
- HTML-Dashboard (`index.html`)
- REST-Endpoints für:
  - GET `/api/state` → Aktuelle Sensordaten
  - POST `/api/water` → Sofort Bewässerung
  - GET `/api/logs` → Letzte Logs

#### `sd_storage.c/h` – SD-Kartenverwaltung
- Initialisierung und Fehlerbehandlung
- Dateischreib/-lesevorgänge
- Self-Test vor Verwendung

---

## 🔄 Systemfluss & Ablauf

### Startup-Sequenz

```
app_main() aufgerufen
    ↓
1. NVS-Flash initialisieren (Persistente Speicherung)
    ↓
2. State & Actor-System initialisieren
    ↓
3. Button-Interrupt einrichten
    ↓
4. SD-Karte initialisieren + self-test
    ↓
5. Timer starten (Bewässerungs-Zyklen)
    ↓
6. WiFi & Webserver starten (optional, wenn WIFI_STATUS=1)
    ↓
7. Sensor-Reading-Task starten (FreeRTOS)
    ↓
✅ System läuft
```

### Normaler Bewässerungs-Zyklus

```
Timer-Callback aufgerufen (alle X Minuten)
    ↓
smart_logic_evaluate() ausgeführt
    ↓
Liegt Bodenfeuchte unter Schwelle?
    ├─ JA → Prüfe Luftfeuchte
    │   ├─ Zu feucht? → Nicht gießen (Staunässe-Prävention)
    │   └─ Normal? → actor_start_timed_watering()
    │       ↓
    │       Pumpe läuft für X Minuten
    │
    └─ NEIN → Nichts tun (zu feucht)
```

### Sensor-Auslesung

```
Sensor-Reading-Task (2s Intervall, FreeRTOS)
    ↓
Lese DHT22 (Temperatur, Luftfeuchte)
Lese Bodenfeuchtesensor (ADC)
    ↓
Mutex sperren (thread-safe)
    ↓
Schreibe in sys_state Struktur
    ↓
Mutex freigeben
    ↓
Logging (async, zur SD-Karte)
```

### Manuelle Bedienung

```
Button gedrückt (Interrupt auf GPIO 13)
    ↓
button_pressed_handler() aufgerufen
    ↓
actor_start_timed_watering() mit konfig. Dauer
    ↓
timer_start_cycle() mit neuem Check-Intervall
    ↓
Pumpe läuft, dann nächster Zyklus startet
```

---

## 🔄 Detailliertes Systemflowchart

### Flowchart 1: Systemstart (app_main)

```mermaid
flowchart TD
    A["START: app_main()"] --> B["NVS-Flash initialisieren"]
    B --> C["state_init() - Globaler State"]
    C --> D["actor_init() - Pumpensteuerung"]
    D --> E["button_init() - Manueller Taster"]
    E --> F["SD-Karte initialisieren"]
    F --> G{SD-Karte OK?}
    G -->|JA| H["Profile scannen von SD"]
    G -->|NEIN| I["Fallback ohne SD"]
    H --> J["timer_init() - Timer-System"]
    I --> J
    J --> K["timer_start_cycle() - Zyklus starten"]
    K --> L["WiFi-Verbindung prüfen"]
    L -->|WIFI_STATUS=1| M["WiFi initialisieren"]
    L -->|WIFI_STATUS=0| N["Nur lokaler Betrieb"]
    M --> O["mDNS & Webserver starten"]
    N --> P["sensor_reading_task starten"]
    O --> P
    P --> Q["logging_task starten"]
    Q --> R["✅ System ready"]
```

### Flowchart 2: Bewässerungs-Entscheidungslogik

```mermaid
flowchart TD
    A["🔄 Timer-Callback\nsmart_logic_evaluate()"] --> B["Lese Bodenfeuchte\nLese Luftfeuchte\nLese Profil-Schwellwerte"]
    B --> C{Bodenfeuchte <br/> Schwelle?}
    C -->|NEIN| D["Boden zu feucht ❌"]
    C -->|JA| E{Luftfeuchte ><br/> MAX_HUMIDITY?}
    E -->|JA| F["Luft zu feucht<br/>Staunässe-Prävention ❌"]
    E -->|NEIN| G["Bedingungen erfüllt ✅"]
    D --> H["Nichts tun<br/>nächster Check in X min"]
    F --> H
    G --> I["actor_start_timed_watering()"]
    I --> J["Pumpe läuft für<br/>X Minuten"]
    J --> K["Protokolliere Bewässerung"]
    K --> H
    H --> L["⏱️ Nächster Zyklus"]
```

### Flowchart 3: Sensor-Auslesung (Multitasking)

```mermaid
flowchart TD
    A["🔄 sensor_reading_task()\nInterval: 2 Sekunden"] --> B["air_sensor_read()"]
    B --> C{DHT22<br/>erfolgreich?}
    C -->|NEIN| D["Temp/Humidity<br/>fallback generieren"]
    C -->|JA| E["Temp & Humidity<br/>erhalten"]
    D --> F["soil_moisture_read()"]
    E --> F
    F --> G{ADC Wert<br/>gültig?}
    G -->|NEIN| H["Bodenfeuchtigkeit<br/>fallback: 40-70%"]
    G -->|JA| I["Bodenfeuchtigkeit<br/>0-100%"]
    H --> J["Mutex sperren"]
    I --> J
    J --> K["sys_state aktualisieren:\n- current_temp\n- air_humidity\n- soil_moisture_1"]
    K --> L["Mutex freigeben"]
    L --> M["ESP_LOGI() - Log Daten"]
    M --> N["logger_write_sensor_data()"]
    N --> O["⏱️ Warte 2 Sekunden"]
    O --> A
```

### Flowchart 4: Manueller Button-Betrieb

```mermaid
flowchart TD
    A["👆 Button Interrupt<br/>GPIO 13"] --> B["button_pressed_handler()"]
    B --> C["Lese Config:\nwatering_duration\ncycle_interval"]
    C --> D{Konfiguration<br/>gültig?}
    D -->|NEIN| E["Setze Defaults:\n5 min Bewässerung\n60 min Zyklus"]
    D -->|JA| F["Nutze gespeicherte Werte"]
    E --> G["actor_start_timed_watering()"]
    F --> G
    G --> H["Pumpe startet SOFORT"]
    H --> I["timer_start_cycle()"]
    I --> J["Neuer Zyklus wird initiiert"]
    J --> K["⏱️ Nach X Minuten: Pumpe stoppt"]
    K --> L["Nächster Zyklus startet"]
```

### Flowchart 5: Web-Dashboard & API

```mermaid
flowchart TD
    A["Browser: smartirrig.local"] --> B["webserver.c\nHTTP-Server"]
    B --> C{Request-Typ?}
    C -->|GET /| D["index.html<br/>Dashboard UI"]
    C -->|GET /api/state| E["sys_state mutex lesen"]
    C -->|POST /api/water| F["Sofort gießen"]
    C -->|GET /api/logs| G["SD-Karte logs/ lesen"]
    D --> H["Zeige aktuelle Werte:\n- Temp, Luftfeuchte\n- Bodenfeuchte\n- Status"]
    E --> H
    F --> I["actor_start_timed_watering()"]
    I --> J["API Response: OK"]
    G --> K["CSV-Daten zurückgeben"]
    J --> L["Browser zeigt Update"]
    K --> L
```

### Flowchart 6: SD-Karten-Logging

```mermaid
flowchart TD
    A["🔄 logger_write_sensor_data()\nInterval: z.B. 60 Sekunden"] --> B["Lese sys_state (mutex)"]
    B --> C["Formatiere CSV-Zeile:\nTimestamp, Temp, Humidity,\nSoil_Moisture, Status"]
    C --> D["Öffne /sdcard/logs/sensor_log.csv"]
    D --> E{Datei OK?}
    E -->|NEIN| F["Erstelle Verzeichnis"]
    E -->|JA| G["Schreibe Zeile"]
    F --> G
    G --> H["Schließe Datei"]
    H --> I{Profile-Manager<br/>Update?}
    I -->|JA| J["Aktualisiere aktive Profile\nauf SD"]
    I -->|NEIN| K["Fertig"]
    J --> K
```

### Flowchart 7: Gesamt-Systemarchitektur (Komponenten-Übersicht)

```mermaid
graph TB
    subgraph Hardware["🔌 HARDWARE LAYER"]
        Pin["pin_config.h\nGPIO-Definitionen"]
        Soil["soil.c/h\nBodenfeuchte ADC"]
        Air["air_sensor.c/h\nDHT22 Sensor"]
        Button["button.c/h\nGPIO-Interrupt"]
        Actor["actor.c/h\nRelais-Steuerung"]
    end
    
    subgraph State["🧠 STATE MANAGEMENT"]
        StateMod["state.c/h\nMutex-geschützter\nglobaler State"]
        Config["pin_config.h\nKonfiguration"]
    end
    
    subgraph Logic["⚙️ INTELLIGENTE LOGIK"]
        Timer["timer.c/h\nZyklisches System"]
        SmartLogic["smart_logic.c/h\nBewässerungs-\nEntscheidung"]
        Profile["profile_manager.c/h\nProfile & Schwellwerte"]
    end
    
    subgraph Storage["💾 SPEICHER"]
        SD["sd_storage.c/h\nSD-Karten-I/O"]
        Logger["logger.c/h\nSensorlog CSV"]
    end
    
    subgraph Network["🌐 NETZWERK & WEB"]
        WiFi["wlan.c/h\nWiFi-Station"]
        Server["webserver.c/h\nHTTP-Server"]
        MDns["mDNS Service"]
    end
    
    subgraph FreeRTOS_Tasks["🔄 FreeRTOS TASKS"]
        Task1["sensor_reading_task"]
        Task2["sd_card_monitor_task"]
        Task3["logging_task"]
    end
    
    Main["main.c<br/>app_main()"]
    
    Main -->|initialisiert| Hardware
    Main -->|nutzt| State
    Main -->|startet| Logic
    Main -->|konfiguriert| Storage
    Main -->|startet| Network
    Main -->|erstellt| FreeRTOS_Tasks
    
    Hardware -->|updated| State
    Logic -->|liest| State
    Logic -->|schreibt| State
    FreeRTOS_Tasks -->|arbeiten mit| State
    Logger -->|speichert| SD
    Server -->|liest| State
    Profile -->|speichert/lädt| SD
    SmartLogic -->|nutzt| Profile
    Timer -->|ruft auf| SmartLogic
```

---

## 🎯 Workflow-Beispiele

### Beispiel 1: Automatische Bewässerung

```
T=0:00   - System startet
T=0:30   - Sensor misst: Boden 35%, Luft 60%
T=1:00   - Timer-Zyklus: Boden < 40% Schwelle → GIESZEN STARTEN
T=1:05   - Pumpe läuft 5 Minuten
T=1:10   - Pumpe stoppt, nächster Zyklus in 60 min
T=2:10   - Sensor misst: Boden 65% → Trocknend
T=3:10   - Timer-Zyklus: Boden > 40% Schwelle → Nicht gießen
...
```

### Beispiel 2: Manueller Button-Druck

```
T=2:15   - Benutzer drückt Button
T=2:15   - actor_start_timed_watering(5 min) startet
T=2:15   - timer_start_cycle(60 min, 5 min) neu initiiert
T=2:20   - Nach 5 Minuten: Pumpe stoppt automatisch
T=3:20   - Nächster Zyklus beginnt
```

### Beispiel 3: Staunässe-Prävention

```
T=0:00   - Intensives Bewässerungsverbot wegen Regenwasser
T=0:30   - Sensor: Boden 75%, Luft 85% (sehr feucht!)
T=1:00   - Timer-Zyklus: Boden < Schwelle, aber Luft > 80%
         → Smart-Logik entscheidet: NICHT GIESZEN (Staunässe-Prävention)
T=2:00   - Nach 1 Stunde Verdunstung: Boden 60%, Luft 70%
T=2:00   - Nächster Zyklus: Luft normal → Falls Boden < Schwelle: GIESZEN OK
```

---

## 📊 Wichtige Konfigurationen

### `main/pin_config.h`
- GPIO-Zuordnungen für alle Sensoren und Aktoren
- SD-Karten-Mountpoint und Dateipfade
- Schwellwerte für Sensoren

### `main/state.h`
```c
struct {
    float current_temp;
    float air_humidity;
    int soil_moisture_1;
    struct active_profile active_profile;
    int watering_active;
    // ... weitere Felder
} sys_state;
```

### Profile (JSON auf SD-Karte)
```json
{
  "name": "Tomate",
  "soil_moisture_threshold": 35,
  "max_humidity": 80,
  "check_interval_minutes": 60,
  "watering_duration_minutes": 5
}
```

---

## 🔧 Erweiterungsmöglichkeiten

1. **Mehrere Sensoren**: Zweite Bodenfeuchte-Sonde für große Pflanzen
2. **Wetteranbindung**: Regen-API integrieren (lokal mit WLAN)
3. **Machine Learning**: Bewässerungsmuster lernen
4. **Benachrichtigungen**: Email/Push bei Fehler
5. **Mobile App**: Native App statt Web-Dashboard
6. **Datenexport**: Grafische Analyse der Logs
7. **PWM-Pumpen**: Variable Wassermenge statt On/Off

---

## 🐛 Debugging-Tipps

- **Logs**: `ESP_LOGI(TAG, "...")` in main.c definieren
- **Mutex-Deadlock**: Timeout `MUTEX_TIMEOUT_MS` in pin_config.h
- **SD-Fehler**: Self-Test mit `sd_card_self_test()` im Startup
- **Sensor-Fehler**: Fallback-Werte werden automatisch generiert
- **WiFi**: mDNS Service erlaubt Zugriff via `http://smartirrig.local`

---

## 📚 Dateistruktur

```
main/
├── main.c                   # Systemstart & Koordination
├── pin_config.h             # GPIO & Pfad-Konfiguration
├── state.c/h                # Globaler State (mutex-geschützt)
│
├── hardware/
│   ├── actor.c/h            # Pumpen-/Relaissteuerung
│   ├── button.c/h           # Button-Handler
│   ├── soil.c/h             # Bodenfeuchtesensor
│   └── air_sensor.c/h       # DHT22 Sensor
│
├── logic/
│   ├── timer.c/h            # Zyklisches System
│   ├── smart_logic.c/h      # Intelligente Logik
│   ├── profile_manager.c/h  # Profile & Schwellwerte
│   └── logger.c/h           # Sensorlog
│
└── web/
    ├── wlan.c/h             # WiFi-Verbindung
    ├── webserver.c/h        # HTTP-Server & API
    ├── index.html           # Web-Dashboard
    └── sd_storage.c/h       # SD-Kartenverwaltung
```

---

## ✅ Checkliste für Inbetriebnahme

- [ ] GPIO-Pins in `pin_config.h` überprüfen
- [ ] SD-Karte formatiert und bereit
- [ ] Profile auf SD-Karte erstellen (`/sdcard/profiles/`)
- [ ] WiFi-Credentials in `wlan.c` eintragen (oder extern laden)
- [ ] Sensoren kalibrieren (Bodenfeuchtigkeit 0-100%)
- [ ] Erste Bewässerung testen (manueller Button)
- [ ] Logs auf SD überprüfen
- [ ] Web-Dashboard öffnen: `http://smartirrig.local`
- [ ] Zyklische Bewässerung im Monitor beobachten

---

## 📞 Zusammenfassung

**SmartIrrigation C3** ist ein vollständiges, intelligentes Bewässerungssystem mit:
- ✅ Modularer Architektur
- ✅ Thread-sicheren Operationen (FreeRTOS Mutexes)
- ✅ Intelligenter Entscheidungslogik
- ✅ Lokaler Datenspeicherung
- ✅ Web-Interface für Fernbedienung
- ✅ Extensible für zukünftige Features

Das System ist produktionsreif und kann direkt auf einem ESP32-C3 deployed werden!

