# PLANS-webservice.md

## Objective
Develop "Prusa Connect Local", a service running on a Raspberry Pi that provides a Prusa Connect-like experience by communicating directly with the printer's firmware API.

## Open questions
- What are all the available API endpoints in the Buddy firmware?
- Which data points (temperatures, fan speeds, print status, etc.) can be retrieved?
- Is there a formal API documentation within the repository?
- How does Prusa Connect (cloud) communicate with the printer compared to Prusa Link (local)?

## Approved plan
- [x] **Step 1: Firmware API Research**
    - [x] Identify all data points currently available via curl/API from the firmware.
    - [x] Compare firmware capabilities with Prusa Link's display.
    - [x] Document found endpoints and data structures.
        - **Findings:**
            - **Local API (v1):** `/api/v1/status` provides basic temps, job progress, and axis positions.
            - **OctoPrint API:** `/api/printer` provides temp history and basic state.
            - **Internal Telemetry:** Found in `src/connect/render.cpp`. Contains:
                - Chamber & Enclosure (temp, target, fans).
                - Detailed Fan RPMs (hotend, print, enclosure, chamber).
                - Extra Temps (PSU, Ambient, Heatbreak).
                - Filament Sensor detailed states.
                - MMU tool/slot details.
            - **Conclusion:** The firmware *has* all the info Prusa Connect displays, but most of it is NOT exposed via the local HTTP API.
            - **Potential extraction methods:**
                - 1. **UDP Metrics:** `M334` G-code can stream real-time metrics to a UDP port.
                - 2. **Connect Redirection:** Point the printer's Connect URL to the local Pi service.
## Decisions
- **Strategy Choice:** **Option B: API Extension**.
    - We will modify the firmware to include ALL data points currently sent to Prusa Connect (Cloud) in the local `/api/v1/status` endpoint.
    - This ensures the local service has 1:1 parity with the cloud data without needing a proxy.
    - Prusa Connect Cloud functionality remains fully operational and untouched.

## Approved plan
- [x] **Step 1: Firmware API Research**
    - [x] Identify all data points currently available via curl/API from the firmware.
    - [x] Compare firmware capabilities with Prusa Link's display.
    - [x] Document found endpoints and data structures.
        - **Findings:**
            - **Local API (v1):** `/api/v1/status` provides basic temps, job progress, and axis positions.
            - **OctoPrint API:** `/api/printer` provides temp history and basic state.
            - **Internal Telemetry:** Found in `src/connect/render.cpp`. Contains:
                - Chamber & Enclosure (temp, target, fans).
                - Detailed Fan RPMs (hotend, print, enclosure, chamber).
                - Extra Temps (PSU, Ambient, Heatbreak).
                - Filament Sensor detailed states.
                - MMU tool/slot details.
            - **Conclusion:** The firmware *has* all the info Prusa Connect displays, but most of it is NOT exposed via the local HTTP API.
- [x] **Step 2: Firmware Implementation (API Expansion)**
    - [x] Map all fields from `src/connect/render.cpp` (Connect Telemetry) to `lib/WUI/nhttp/status_renderer.cpp` (Local API).
    - [x] **Fields added:**
        - Fan RPMs (Hotend, Print, Chamber, Enclosure).
        - Extra Temps (PSU, Ambient, Heatbreak, Chamber).
        - Pressure Advance & Smooth Time.
        - Filament used & Dialog IDs.
    - [x] **Bed Mesh Implementation:**
        - Created `/api/v1/mesh` endpoint.
        - Exports raw 2D array of Z-values + grid metadata (min/max, distance).
    - [x] **G-Code Console Support:**
        - Created `/api/v1/gcode` endpoint to accept POST commands.
        - Integrated with `marlin_client::gcode_try` for safe execution.
    - [x] Implement the JSON rendering for these new fields in the local web server.
- [x] **Step 3: Pi Service Implementation**
    - [x] Develop a Python service (FastAPI) for the Raspberry Pi (Running on Port 5050).
    - [x] Implement polling logic to fetch the expanded local API data.
    - [x] Create a local dashboard/interface mirroring Prusa Connect aesthetics.
    - [x] **Features implemented:**
        - Real-time Telemetry Dashboard (Nozzle, Bed, Fans, Chamber, PSU, PA).
        - 3D Bed Mesh Visualization (Interactive surface plot).
        - Functional G-Code Console.
- [ ] **Step 4: Testing & Validation**
    - [ ] Verify local API output via curl.
    - [ ] Confirm Prusa Connect Cloud still receives data correctly.
    - [ ] Test G-Code command execution from the Pi dashboard.
    - [ ] Verify local API output via curl.
    - [ ] Confirm Prusa Connect Cloud still receives data correctly.

## Handoff
- Agent: Gemini CLI
- Date: 2026-06-14
- Completed this session:
  - Created PLANS-webservice.md
- Stopped at:
  - Starting Step 1: Researching available firmware API data.
- Next step:
  - Search the codebase for API endpoints and data models.
- Open blockers:
  - Need to identify the specific API endpoints.

## Notes
- Prusa Link on-board is limited by resources; "Prusa Connect Local" on a Pi can handle more data and complex UI.
