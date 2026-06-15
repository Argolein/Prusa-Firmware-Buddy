# API-doc.md

## Zweck

Blueprint fuer einen lokalen Dienst / ein Web-Interface auf dem Pi fuer die aktuell modifizierte Core ONE Firmware.

Wichtige Designentscheidung dieses Firmware-Stands:

- **Keine G-Code-Konsole**
- **Kein `/api/v1/gcode`**
- **Kein Request/Reply-Capture fuer G-Code**
- **Bed Mesh nur noch on-demand ueber `/api/v1/mesh`**

Wenn eine naechste Session etwas fuer "Prusa Connect Local" baut, sollte sie sich an dieser Datei orientieren und **nicht** wieder einen G-Code-Console-Pfad in die Firmware einbauen.

## Auth

Die WUI-API ist authentifiziert.

- Username fuer Digest-Auth: `maker`
- Passwort: das in PrusaLink/WUI konfigurierte Passwort
- Alternativ akzeptiert die Firmware einen API-Key ueber den Header `X-Api-Key`

Empfehlung fuer den Pi-Dienst:

- primär `X-Api-Key` verwenden
- Digest nur als Kompatibilitaets-Fallback behandeln

## Empfohlener Endpunktsatz fuer "Prusa Connect Local"

Das ist der Satz, den dein Pi-Dienst im Normalfall wirklich braucht.

### 1. Status pollen

`GET /api/v1/status`

Haupt-Endpunkt fuer das laufende UI-Polling.

Antwortstruktur:

- `job`
  - `id`
  - `progress`
  - `time_remaining`
  - `filament_change_in`
  - `time_printing`
- `storage`
  - `path`
  - `name`
  - `read_only`
- `transfer`
  - `id`
  - `time_transferring`
  - `progress`
  - `transferred`
- `printer`
  - `state`
  - `temp_bed`
  - `target_bed`
  - `temp_nozzle`
  - `target_nozzle`
  - `temp_heatbreak`
  - `axis_z`
  - `axis_x` und `axis_y` nur wenn nicht aktiv gedruckt wird
  - `flow`
  - `speed`
  - `fan_hotend`
  - `fan_print`
  - `pressure_advance`
  - `pressure_advance_smooth_time`
  - `dialog_id` nur wenn aktiv
  - `filament`

Bedingte Felder je nach Drucker-/Board-Variant:

- `temp_psu`, `temp_ambient`
- `enclosure`
- `chamber`

Empfehlung:

- Polling alle `1-2s`
- Diesen Endpunkt als primäre Datenquelle fuer Dashboard / Druckstatus verwenden

### 2. Statische Druckerinfo

`GET /api/v1/info`

Antwort:

- `nozzle_diameter`
- `mmu`
- `serial`
- `hostname`
- `min_extrusion_temp`

Empfehlung:

- einmal beim App-Start laden

### 3. Storage-Status

`GET /api/v1/storage`

Antwort:

- `storage_list[]`
  - `path`
  - `name`
  - `type`
  - `read_only`
  - `available`

Wichtig:

- aktueller Root fuer Dateien ist `/usb/`

### 4. Dateibaum / Dateidetails

`GET /api/v1/files`

`GET /api/v1/files/<pfad>`

`HEAD /api/v1/files/<pfad>`

Verhalten:

- Wenn `<pfad>` ein Ordner ist, kommt ein Folder-JSON mit `children`
- Wenn `<pfad>` eine Datei ist, kommen Dateidetails

Relevante Felder bei Ordnern:

- `type: "FOLDER"`
- `name`
- `m_timestamp`
- `children[]`
  - `name`
  - `display_name`
  - `type`
  - `ro`
  - `m_timestamp`
  - `refs.icon`
  - `refs.thumbnail`
  - `refs.download`

Relevante Felder bei Dateien:

- `name`
- `display_name`
- `type`
- `ro`
- `m_timestamp`
- `size`
- `refs.icon`
- `refs.thumbnail`
- `refs.download`

Hinweise:

- `If-None-Match` wird fuer File-/Folder-GET unterstuetzt
- bei unveraendertem Inhalt kann `304 Not Modified` kommen

### 5. Datei hochladen

`PUT /api/v1/files/<pfad>`

Request:

- Body = rohe G-Code-Datei
- `Content-Length` ist Pflicht

Unterstuetzte Zusatz-Header:

- `Print-After-Upload: true`
- `Print-After-Upload: 1`
- `Print-After-Upload: ?1`
- `Overwrite: ?1`
- `Create-Folder: ?1`

Wichtig:

- `Create-Folder` ist unterstuetzt, aber fuer einen ersten Pi-Dienst optional
- fuer einen Minimaldienst reicht: normaler Datei-Upload, optional mit `Print-After-Upload`

### 6. Druck aus Datei starten

`POST /api/v1/files/<pfad>`

Verhalten:

- startet den Druck dieser Datei
- liefert typischerweise `204 No Content` bei Erfolg

### 7. Datei loeschen

`DELETE /api/v1/files/<pfad>`

Verhalten:

- loescht Datei
- bei busy/aktiver Nutzung typischerweise `409 Conflict`

### 8. Aktuellen Job lesen

`GET /api/v1/job`

Wenn ein Job aktiv ist, kommen u.a.:

- `id`
- `state`
- `progress`
- `time_remaining`
- `time_printing`
- `file`
  - `refs.icon`
  - `refs.thumbnail`
  - `refs.download`
  - `name`
  - `display_name`
  - `path`
  - `size`
  - `m_timestamp`

Wenn kein Job aktiv ist:

- `204 No Content`

### 9. Job steuern

Pause:

`PUT /api/v1/job/<job_id>/pause`

Resume:

`PUT /api/v1/job/<job_id>/resume`

Stop / Abbrechen:

`DELETE /api/v1/job/<job_id>`

Hinweise:

- die `job_id` muss zur aktuellen Firmware-`job_id` passen
- bei falscher ID kommt `404`
- bei ungueltigem Zustand kommt `409`

### 10. Transferstatus

`GET /api/v1/transfer`

Wenn ein Upload/Transfer laeuft:

- Transfer-JSON

Wenn kein Transfer laeuft:

- `204 No Content`

### 11. Bed Mesh laden

`GET /api/v1/mesh`

Das ist der **lokale Custom-Endpunkt** dieses Firmware-Stands im registrierten V1-Handler.

Antwort:

- `valid`
- `x_min`
- `y_min`
- `x_dist`
- `y_dist`
- `points_x`
- `points_y`
- `mesh`

`mesh` ist ein 2D-Array von Hoehenwerten, `null` fuer ungueltige Punkte.

Wichtige Betriebsregel:

- Mesh **nicht pollen**
- Mesh nur laden, wenn der Benutzer die Mesh-Ansicht oeffnet oder explizit aktualisiert

### 12. Thumbnails

Klein:

`GET /thumb/s<pfad>`

Gross:

`GET /thumb/l<pfad>`

Beispiele:

- `/thumb/s/usb/test.gcode`
- `/thumb/l/usb/test.gcode`

Empfehlung:

- lazy load
- fuer den ersten Wurf optional

### 13. Direkter Dateidownload

`GET /usb/<pfad>`

Beispiel:

- `/usb/folder/file.gcode`

Das ist praktisch fuer Download-Buttons in einer lokalen UI.

## Legacy-/Kompatibilitaets-Endpunkte

Diese Endpunkte existieren weiterhin, aber fuer einen neuen Pi-Dienst solltest du **bevorzugt die `/api/v1/...`-Routen** verwenden.

### Kompatibilitaet allgemein

- `GET /api/settings`
  - Stub
  - Antwort derzeit nur `{"printer": {}}`

- `GET /api/version`
  - liefert allgemeine API-/Firmware-Infos

- `GET /api/printer`
  - OctoPrint-/PrusaLink-kompatible Druckerzustandsdarstellung

- `GET /api/job`
  - aelteres Job-JSON

- `POST /api/job`
  - JSON-Jobsteuerung
  - unterstuetzte Kommandos:
    - `{"command":"cancel"}`
    - `{"command":"pause","action":"pause"}`
    - `{"command":"pause","action":"resume"}`
    - `{"command":"pause","action":"toggle"}`

- `GET /api/files`
- `GET /api/files/<pfad>`
- `POST /api/files/<pfad>`
- `DELETE /api/files/<pfad>`
  - OctoPrint-artige Files-API

- `GET /api/download`
- `GET /api/transfer`
  - Transferstatus ausserhalb des v1-Namensraums

## Was bewusst **nicht** Teil dieser API ist

Diese Punkte sind fuer die naechste Session wichtig:

- **kein** `/api/v1/gcode`
- **kein** `/api/v1/gcode/<id>`
- **kein** "Befehl senden und spaeter Reply pollen"
- **keine** Firmware-seitige G-Code-Konsole
- **kein** dauerhafter Mesh-Hintergrundexport in `marlin_vars`

## Empfohlene Minimalimplementierung fuer den Pi-Dienst

Wenn du den lokalen Dienst in einer neuen Session bauen laesst, reicht fuer V1:

1. Beim Start:
   - `GET /api/v1/info`
   - `GET /api/v1/storage`

2. Im Dashboard:
   - regelmaessig `GET /api/v1/status`

3. Fuer Dateibrowser:
   - `GET /api/v1/files`
   - `GET /api/v1/files/<pfad>`
   - `PUT /api/v1/files/<pfad>`
   - `POST /api/v1/files/<pfad>`
   - `DELETE /api/v1/files/<pfad>`

4. Fuer Drucksteuerung:
   - `GET /api/v1/job`
   - `PUT /api/v1/job/<id>/pause`
   - `PUT /api/v1/job/<id>/resume`
   - `DELETE /api/v1/job/<id>`

5. Fuer Mesh-Ansicht:
   - `GET /api/v1/mesh` nur on-demand

6. Optional spaeter:
   - `/thumb/...`
   - `/usb/...`
   - Legacy-Kompatibilitaet ueber `/api/...`

## Source of truth im Firmware-Code

Falls spaeter etwas gegengeprueft werden muss, sind diese Dateien die Referenz:

- `lib/WUI/link_content/prusa_link_api.cpp`
- `lib/WUI/link_content/prusa_link_api_v1.cpp`
- `lib/WUI/nhttp/status_renderer.cpp`
- `lib/WUI/link_content/mesh_renderer.cpp`
- `lib/WUI/link_content/basic_gets.cpp`
- `lib/WUI/nhttp/file_info.cpp`
- `lib/WUI/nhttp/file_command.cpp`
- `lib/WUI/nhttp/job_command.cpp`
- `lib/WUI/nhttp/gcode_upload.cpp`
- `lib/WUI/link_content/previews.cpp`
- `lib/WUI/link_content/usb_files.cpp`
