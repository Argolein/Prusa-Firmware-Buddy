; --- Printer Initialization ---
M17 ; enable steppers
M862.1 P[nozzle_diameter] A{(printer_notes=~/.*ABRASIVE_NOZZLE.*/ ? 1 : 0)} F{(printer_notes=~/.*HF_NOZZLE.*/ ? 1 : 0)} ; nozzle check
M862.3 P "COREONE" ; model check
M862.5 P2 ; g-code level check
M862.6 P"Input shaper" ; FW feature check
M115 U6.3.0+10073

; --- Print Area ---
M555 X{(min(print_bed_max[0], first_layer_print_min[0] + 32) - 32)} Y{(max(0, first_layer_print_min[1]) - 4)} W{((min(print_bed_max[0], max(first_layer_print_min[0] + 32, first_layer_print_max[0])))) - ((min(print_bed_max[0], first_layer_print_min[0] + 32) - 32))} H{((first_layer_print_max[1])) - ((max(0, first_layer_print_min[1]) - 4))}

G90 ; absolute coordinates
M83 ; relative extruder mode

; --- Heatup ---
M140 S[first_layer_bed_temperature] ; set target bed temp
M104 S{((filament_notes[0]=~/.*HT_MBL10.*/) ? (first_layer_temperature[0] - 10) : (filament_type[0] == "PC" or filament_type[0] == "PA") ? (first_layer_temperature[0] - 25) : (filament_type[0] == "FLEX") ? 210 : 170)} ; set MBL nozzle temp

G28 ; home axes

; --- Fan & Chamber Logic ---
{if first_layer_bed_temperature[initial_tool] <= 65}
M141 S40    ; dynamic: keep chamber < 40C
{else}
M148        ; disable auto-filtration
M141 S100   ; disable auto-cooling
M106 P4 S51 ; fixed: exhaust fan 20%
{endif}

; --- Wait for Temperatures ---
G0 Z40 F10000
M190 R[first_layer_bed_temperature] ; wait for bed
M109 R{((filament_notes[0]=~/.*HT_MBL10.*/) ? (first_layer_temperature[0] - 10) : (filament_type[0] == "PC" or filament_type[0] == "PA") ? (first_layer_temperature[0] - 25) : (filament_type[0] == "FLEX") ? 210 : 170)} ; wait for nozzle

G29 G ; absorb heat

M302 S160 ; low temp extrusion limit

{if filament_type[initial_tool]=="FLEX"}
G1 E-4 F2400
{else}
G1 E-2 F2400
{endif}

M84 E
G29 P9 X208 Y-2.5 W32 H4 ; limited MBL
M84 E
G29 P1 ; probe area
G29 P1 X150 Y0 W100 H20 C
G29 P3.2 ; interpolate
G29 P3.13 ; extrapolate
G29 A ; activate MBL

; --- Purge Line ---
M104 S{first_layer_temperature[initial_extruder]}
G0 X249 Y-2.5 Z15 F4800
M109 S{first_layer_temperature[initial_extruder]}

G92 E0
M569 S0 E ; spreadcycle
G1 E{(filament_type[0] == "FLEX" ? 4 : 2)} F2400 ; deretract
G0 E5 X235 Z0.2 F500 ; purge
G0 X225 E4 F500
G0 X215 E4 F650
G0 X205 E4 F800
G0 X202 Z0.05 F8000 ; wipe
G0 X199 Z0.2 F8000

G92 E0
M221 S100 ; flow 100%
