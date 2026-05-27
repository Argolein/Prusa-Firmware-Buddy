# PLANS-Web.md - Modernisierung der Prusa Web-Schnittstelle

## Zielsetzung
Anpassung und Erweiterung der Prusa Buddy Web-Schnittstelle (Prusa Link), um Funktionen bereitzustellen, die dem Standard von modernen Interfaces wie **Mainsail** (Klipper) entsprechen. Fokus liegt auf verbesserter Telemetrie, direkter Kontrolle und Visualisierung (z.B. Bed Mesh).

---

## 1. IST-Analyse (Findings)

### Aktuelle Architektur
- **Backend:** C++ (STM32), nutzt den `nhttp` Server in der Buddy-Firmware. Endpunkte liegen primär in `lib/WUI/link_content/`.
- **Frontend:** React SPA (Single Page Application) aus dem Repository `Prusa-Link-Web`. Die kompilierten Dateien liegen in `src/resources/web/`.
- **Datenfluss:** Die UI pollt JSON-Endpunkte (`/api/v1/status`, `/api/printer`).

### Vorhandene Funktionen
- Grundlegende Telemetrie (Temperaturen, Z-Höhe, Status).
- Drucksteuerung (Start, Stop, Pause, Resume).
- Dateiverwaltung (Upload, Delete, Directory Listing).
- Unterstützung für OctoPrint-ähnliche API-Calls (eingeschränkt).

### Fehlende Funktionen (Vergleich mit Mainsail)
- **Bed Mesh Visualisierung:** Daten liegen in Marlin (`ubl.z_values`), werden aber nicht per API exportiert.
- **G-Code Konsole (Terminal):** Kein Endpunkt zum Senden von Roh-Befehlen und Empfangen von Terminal-Output.
- **Makro-Support:** Prusa Firmware hat kein natives Makro-System wie Klipper.
- **Erweiterte Kalibrierung:** Keine geführten Web-Wizards für PID-Tuning oder Z-Offset Kalibrierung.

---

## 2. Gap-Analyse & Vorlagen (Mainsail)

| Feature | Mainsail (Klipper) | Prusa Link (Buddy) | Status |
| :--- | :--- | :--- | :--- |
| **Bed Mesh** | 3D Visualisierung | Keine Anzeige | ❌ Fehlend |
| **Konsole** | Echtzeit G-Code Terminal | Nur Log-Download | ❌ Fehlend |
| **Makros** | Beliebige G-Code Blöcke | Keine | ❌ Fehlend |
| **Presets** | Temperatur-Voreinstellungen | Fest in Firmware | ⚠️ Eingeschränkt |
| **Vorschaubild** | Vollständige Integration | Vorhanden (Thumbnails) | ✅ Vorhanden |

---

## 3. Implementierungsplan

### Phase 1: Backend-Erweiterung (Buddy Firmware)
*Ziel: Daten verfügbar machen.*

1.  **Bed Mesh API:**
    - `marlin_vars_t` in `src/common/marlin_vars.hpp` erweitern, um das Mesh-Array zu speichern.
    - In `src/common/marlin_server.cpp` (ExtUI) die `onMeshUpdate` Funktion so anpassen, dass sie das Mesh in `marlin_vars` schreibt.
    - Neuen API-Endpunkt `/api/v1/mesh` in `lib/WUI/link_content/prusa_link_api_v1.cpp` anlegen, der das Mesh als JSON-Array liefert.
2.  **G-Code Command API:**
    - Endpunkt `/api/v1/printer/command` implementieren, der einen POST mit G-Code entgegennimmt und an den Marlin-Planner (`marlin_client::enqueue_gcode`) weiterreicht.

### Phase 2: Frontend-Entwicklung (Prusa-Link-Web)
*Ziel: Visualisierung im Browser.*

1.  **Mesh-Visualisierung:**
    - Integration einer 3D-Bibliothek (z.B. eine leichtgewichtige SVG-basierte Map oder Three.js, falls Speicherplatz reicht).
    - Abruf der Daten von `/api/v1/mesh`.
2.  **Terminal-Komponente:**
    - Neues UI-Element für die Eingabe von G-Code.
    - (Optional) Websocket-Unterstützung für Echtzeit-Logs (komplex, da Buddy aktuell primär Polling nutzt).

### Phase 3: "Virtual Macros"
1.  **Konzept:** Da die Firmware keine Makros kennt, implementieren wir sie in der UI.
2.  **Umsetzung:** Die UI speichert eine Liste von G-Code-Sequenzen (z.B. "PID Tune", "Cold Pull"). Beim Klick sendet die UI die Sequenz Zeile für Zeile oder als temporäre Datei an den Drucker.

---

## 4. Nächste Schritte (Action Items)

1.  [ ] **Backend:** Prototyp des `/api/v1/mesh` Endpunkts erstellen.
2.  [ ] **Frontend:** Lokalen Build-Prozess für `Prusa-Link-Web` aufsetzen und Verbindung zur (emulierten) Firmware testen.
3.  [ ] **Integration:** Erste 3D-Ansicht des Bed Mesh im Browser rendern.

---

## Notizen & Risiken
- **Speicherplatz (Core One):** 
    - Der Core One verfügt über eine Ressourcen-Partition von **2 MB** (512 Blöcke à 4 KB). 
    - Zum Vergleich: Der Prusa MINI hat nur ca. **820 KB**.
    - Die UI wird Gzip-komprimiert gespeichert. Aktuell belegt die UI unkomprimiert ca. 210 KB.
    - Große Bibliotheken (z.B. Three.js für 3D Mesh) müssen vorsichtig eingesetzt werden (Tree Shaking), aber 2 MB bieten ausreichend Puffer für Erweiterungen.
- **Sicherheit:** Die `command` API muss gut abgesichert sein, um Fehlbedienung zu verhindern.
