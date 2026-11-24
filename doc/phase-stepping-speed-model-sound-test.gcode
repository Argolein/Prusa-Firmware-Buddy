; Phase stepping speed model - sound test (Prusa Core One INDX)
; Instructions: doc/phase-stepping-speed-model.md
;
; Cold test: no heating, no extrusion, no tool change. Runs every block twice:
; first with the speed model off (M975 S0), then on (M975 S1).
; Firmware without M975 reports it as an unknown command and carries on,
; so both halves then sound the same.
;
; Motor speed vs. feed rate (Core One is CoreXY):
;   +45/-45 diagonal head move: only motor A / only motor B runs,
;     motor speed = sqrt(2) x head speed -> F = motor_speed / sqrt(2) x 60
;   X or Y head move: both motors run at head speed -> F = motor_speed x 60
;   motor mm/s   diagonal F   X/Y F
;       50          2121       3000
;      100          4243       6000
;      150          6364       9000
;      200          8485      12000
;
; Motion limits are left at the firmware defaults (no M201/M203/M204).
; Moves without extrusion use the travel acceleration, 1500 mm/s^2
; (DEFAULT_TRAVEL_ACCELERATION, include/marlin/Configuration_COREONE.h;
; selected in lib/Marlin/Marlin/src/module/planner.cpp). At 200 mm/s
; motor speed about 87 % (X), 83 % (Y) and 94 % (diagonals) of each pass
; runs at constant speed.
;
; Safe area used: X 30..230, Y 30..190, Z 30. INDX limits (HAS_INDX branch):
;   X_MIN_POS -1                        Configuration_COREONE.h
;   X_MAX_POS 262, but the nozzle cleaner / wastebin side starts at
;   X_WASTEBIN_SAFE_POINT 250           src/feature/nozzle_cleaner/include/nozzle_cleaner.hpp
;                                       (X_MAX_PRINT_POS, Configuration_COREONE.h)
;   Y_MAX_POS 206.5                     Configuration_COREONE.h
;   Tool docks at the front (low Y): dock Y -29.0
;     (lib/Marlin/Marlin/src/module/prusa/indx_dock_position_defaults.hpp),
;     safe distance 28.6 (toolchanger_utils.h) -> keep Y above
;     Y_DOCK_PARKING_MIN_SAFE_POS -0.9   Configuration_COREONE.h
; Margins: 20 mm to the cleaner side, 31 mm to the dock line, 16.5 mm to Y max.

G90 ; absolute positioning
G21 ; millimetres
M117 Sound test: homing
G28 ; home all axes
G1 Z30 F600 ; lower the bed to a safe height
M400

; ==== Speed model OFF ====
M400
M975 S0 ; speed model off
M117 Speed model OFF
G4 S3

; ---- 50 mm/s motor speed, speed model off ----
; +45 diagonal (motor A only), F2121
G1 X40 Y30 F3000 ; to start
M400
M117 OFF 50mm/s +45
G4 S1
G1 X200 Y190 F2121
G1 X40 Y30 F2121
G1 X200 Y190 F2121
G1 X40 Y30 F2121
G1 X200 Y190 F2121
G1 X40 Y30 F2121
G4 S1
; -45 diagonal (motor B only), F2121
G1 X40 Y190 F3000 ; to start
M400
M117 OFF 50mm/s -45
G4 S1
G1 X200 Y30 F2121
G1 X40 Y190 F2121
G1 X200 Y30 F2121
G1 X40 Y190 F2121
G1 X200 Y30 F2121
G1 X40 Y190 F2121
G4 S1
; X (both motors), F3000
G1 X30 Y110 F3000 ; to start
M400
M117 OFF 50mm/s X
G4 S1
G1 X230 Y110 F3000
G1 X30 Y110 F3000
G1 X230 Y110 F3000
G1 X30 Y110 F3000
G1 X230 Y110 F3000
G1 X30 Y110 F3000
G4 S1
; Y (both motors), F3000
G1 X120 Y30 F3000 ; to start
M400
M117 OFF 50mm/s Y
G4 S1
G1 X120 Y190 F3000
G1 X120 Y30 F3000
G1 X120 Y190 F3000
G1 X120 Y30 F3000
G1 X120 Y190 F3000
G1 X120 Y30 F3000
G4 S1

; ---- 100 mm/s motor speed, speed model off ----
; +45 diagonal (motor A only), F4243
G1 X40 Y30 F3000 ; to start
M400
M117 OFF 100mm/s +45
G4 S1
G1 X200 Y190 F4243
G1 X40 Y30 F4243
G1 X200 Y190 F4243
G1 X40 Y30 F4243
G1 X200 Y190 F4243
G1 X40 Y30 F4243
G4 S1
; -45 diagonal (motor B only), F4243
G1 X40 Y190 F3000 ; to start
M400
M117 OFF 100mm/s -45
G4 S1
G1 X200 Y30 F4243
G1 X40 Y190 F4243
G1 X200 Y30 F4243
G1 X40 Y190 F4243
G1 X200 Y30 F4243
G1 X40 Y190 F4243
G4 S1
; X (both motors), F6000
G1 X30 Y110 F3000 ; to start
M400
M117 OFF 100mm/s X
G4 S1
G1 X230 Y110 F6000
G1 X30 Y110 F6000
G1 X230 Y110 F6000
G1 X30 Y110 F6000
G1 X230 Y110 F6000
G1 X30 Y110 F6000
G4 S1
; Y (both motors), F6000
G1 X120 Y30 F3000 ; to start
M400
M117 OFF 100mm/s Y
G4 S1
G1 X120 Y190 F6000
G1 X120 Y30 F6000
G1 X120 Y190 F6000
G1 X120 Y30 F6000
G1 X120 Y190 F6000
G1 X120 Y30 F6000
G4 S1

; ---- 150 mm/s motor speed, speed model off ----
; +45 diagonal (motor A only), F6364
G1 X40 Y30 F3000 ; to start
M400
M117 OFF 150mm/s +45
G4 S1
G1 X200 Y190 F6364
G1 X40 Y30 F6364
G1 X200 Y190 F6364
G1 X40 Y30 F6364
G1 X200 Y190 F6364
G1 X40 Y30 F6364
G4 S1
; -45 diagonal (motor B only), F6364
G1 X40 Y190 F3000 ; to start
M400
M117 OFF 150mm/s -45
G4 S1
G1 X200 Y30 F6364
G1 X40 Y190 F6364
G1 X200 Y30 F6364
G1 X40 Y190 F6364
G1 X200 Y30 F6364
G1 X40 Y190 F6364
G4 S1
; X (both motors), F9000
G1 X30 Y110 F3000 ; to start
M400
M117 OFF 150mm/s X
G4 S1
G1 X230 Y110 F9000
G1 X30 Y110 F9000
G1 X230 Y110 F9000
G1 X30 Y110 F9000
G1 X230 Y110 F9000
G1 X30 Y110 F9000
G4 S1
; Y (both motors), F9000
G1 X120 Y30 F3000 ; to start
M400
M117 OFF 150mm/s Y
G4 S1
G1 X120 Y190 F9000
G1 X120 Y30 F9000
G1 X120 Y190 F9000
G1 X120 Y30 F9000
G1 X120 Y190 F9000
G1 X120 Y30 F9000
G4 S1

; ---- 200 mm/s motor speed, speed model off ----
; +45 diagonal (motor A only), F8485
G1 X40 Y30 F3000 ; to start
M400
M117 OFF 200mm/s +45
G4 S1
G1 X200 Y190 F8485
G1 X40 Y30 F8485
G1 X200 Y190 F8485
G1 X40 Y30 F8485
G1 X200 Y190 F8485
G1 X40 Y30 F8485
G4 S1
; -45 diagonal (motor B only), F8485
G1 X40 Y190 F3000 ; to start
M400
M117 OFF 200mm/s -45
G4 S1
G1 X200 Y30 F8485
G1 X40 Y190 F8485
G1 X200 Y30 F8485
G1 X40 Y190 F8485
G1 X200 Y30 F8485
G1 X40 Y190 F8485
G4 S1
; X (both motors), F12000
G1 X30 Y110 F3000 ; to start
M400
M117 OFF 200mm/s X
G4 S1
G1 X230 Y110 F12000
G1 X30 Y110 F12000
G1 X230 Y110 F12000
G1 X30 Y110 F12000
G1 X230 Y110 F12000
G1 X30 Y110 F12000
G4 S1
; Y (both motors), F12000
G1 X120 Y30 F3000 ; to start
M400
M117 OFF 200mm/s Y
G4 S1
G1 X120 Y190 F12000
G1 X120 Y30 F12000
G1 X120 Y190 F12000
G1 X120 Y30 F12000
G1 X120 Y190 F12000
G1 X120 Y30 F12000
G4 S1

; ==== Speed model ON ====
M400
M975 S1 ; speed model on
M117 Speed model ON
G4 S3

; ---- 50 mm/s motor speed, speed model on ----
; +45 diagonal (motor A only), F2121
G1 X40 Y30 F3000 ; to start
M400
M117 ON 50mm/s +45
G4 S1
G1 X200 Y190 F2121
G1 X40 Y30 F2121
G1 X200 Y190 F2121
G1 X40 Y30 F2121
G1 X200 Y190 F2121
G1 X40 Y30 F2121
G4 S1
; -45 diagonal (motor B only), F2121
G1 X40 Y190 F3000 ; to start
M400
M117 ON 50mm/s -45
G4 S1
G1 X200 Y30 F2121
G1 X40 Y190 F2121
G1 X200 Y30 F2121
G1 X40 Y190 F2121
G1 X200 Y30 F2121
G1 X40 Y190 F2121
G4 S1
; X (both motors), F3000
G1 X30 Y110 F3000 ; to start
M400
M117 ON 50mm/s X
G4 S1
G1 X230 Y110 F3000
G1 X30 Y110 F3000
G1 X230 Y110 F3000
G1 X30 Y110 F3000
G1 X230 Y110 F3000
G1 X30 Y110 F3000
G4 S1
; Y (both motors), F3000
G1 X120 Y30 F3000 ; to start
M400
M117 ON 50mm/s Y
G4 S1
G1 X120 Y190 F3000
G1 X120 Y30 F3000
G1 X120 Y190 F3000
G1 X120 Y30 F3000
G1 X120 Y190 F3000
G1 X120 Y30 F3000
G4 S1

; ---- 100 mm/s motor speed, speed model on ----
; +45 diagonal (motor A only), F4243
G1 X40 Y30 F3000 ; to start
M400
M117 ON 100mm/s +45
G4 S1
G1 X200 Y190 F4243
G1 X40 Y30 F4243
G1 X200 Y190 F4243
G1 X40 Y30 F4243
G1 X200 Y190 F4243
G1 X40 Y30 F4243
G4 S1
; -45 diagonal (motor B only), F4243
G1 X40 Y190 F3000 ; to start
M400
M117 ON 100mm/s -45
G4 S1
G1 X200 Y30 F4243
G1 X40 Y190 F4243
G1 X200 Y30 F4243
G1 X40 Y190 F4243
G1 X200 Y30 F4243
G1 X40 Y190 F4243
G4 S1
; X (both motors), F6000
G1 X30 Y110 F3000 ; to start
M400
M117 ON 100mm/s X
G4 S1
G1 X230 Y110 F6000
G1 X30 Y110 F6000
G1 X230 Y110 F6000
G1 X30 Y110 F6000
G1 X230 Y110 F6000
G1 X30 Y110 F6000
G4 S1
; Y (both motors), F6000
G1 X120 Y30 F3000 ; to start
M400
M117 ON 100mm/s Y
G4 S1
G1 X120 Y190 F6000
G1 X120 Y30 F6000
G1 X120 Y190 F6000
G1 X120 Y30 F6000
G1 X120 Y190 F6000
G1 X120 Y30 F6000
G4 S1

; ---- 150 mm/s motor speed, speed model on ----
; +45 diagonal (motor A only), F6364
G1 X40 Y30 F3000 ; to start
M400
M117 ON 150mm/s +45
G4 S1
G1 X200 Y190 F6364
G1 X40 Y30 F6364
G1 X200 Y190 F6364
G1 X40 Y30 F6364
G1 X200 Y190 F6364
G1 X40 Y30 F6364
G4 S1
; -45 diagonal (motor B only), F6364
G1 X40 Y190 F3000 ; to start
M400
M117 ON 150mm/s -45
G4 S1
G1 X200 Y30 F6364
G1 X40 Y190 F6364
G1 X200 Y30 F6364
G1 X40 Y190 F6364
G1 X200 Y30 F6364
G1 X40 Y190 F6364
G4 S1
; X (both motors), F9000
G1 X30 Y110 F3000 ; to start
M400
M117 ON 150mm/s X
G4 S1
G1 X230 Y110 F9000
G1 X30 Y110 F9000
G1 X230 Y110 F9000
G1 X30 Y110 F9000
G1 X230 Y110 F9000
G1 X30 Y110 F9000
G4 S1
; Y (both motors), F9000
G1 X120 Y30 F3000 ; to start
M400
M117 ON 150mm/s Y
G4 S1
G1 X120 Y190 F9000
G1 X120 Y30 F9000
G1 X120 Y190 F9000
G1 X120 Y30 F9000
G1 X120 Y190 F9000
G1 X120 Y30 F9000
G4 S1

; ---- 200 mm/s motor speed, speed model on ----
; +45 diagonal (motor A only), F8485
G1 X40 Y30 F3000 ; to start
M400
M117 ON 200mm/s +45
G4 S1
G1 X200 Y190 F8485
G1 X40 Y30 F8485
G1 X200 Y190 F8485
G1 X40 Y30 F8485
G1 X200 Y190 F8485
G1 X40 Y30 F8485
G4 S1
; -45 diagonal (motor B only), F8485
G1 X40 Y190 F3000 ; to start
M400
M117 ON 200mm/s -45
G4 S1
G1 X200 Y30 F8485
G1 X40 Y190 F8485
G1 X200 Y30 F8485
G1 X40 Y190 F8485
G1 X200 Y30 F8485
G1 X40 Y190 F8485
G4 S1
; X (both motors), F12000
G1 X30 Y110 F3000 ; to start
M400
M117 ON 200mm/s X
G4 S1
G1 X230 Y110 F12000
G1 X30 Y110 F12000
G1 X230 Y110 F12000
G1 X30 Y110 F12000
G1 X230 Y110 F12000
G1 X30 Y110 F12000
G4 S1
; Y (both motors), F12000
G1 X120 Y30 F3000 ; to start
M400
M117 ON 200mm/s Y
G4 S1
G1 X120 Y190 F12000
G1 X120 Y30 F12000
G1 X120 Y190 F12000
G1 X120 Y30 F12000
G1 X120 Y190 F12000
G1 X120 Y30 F12000
G4 S1

; ---- End ----
M400
M975 S1 ; leave the speed model on
M117 Sound test: done
G1 X120 Y110 F3000 ; park in the middle of the safe area
M400
M84 ; motors off
