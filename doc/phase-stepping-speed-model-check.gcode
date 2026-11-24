; Phase stepping check (Prusa Core One INDX)
; Instructions: doc/phase-stepping-speed-model.md, section "Check"
;
; Cold: no heating, no extrusion, no tool change. M976 measures each motor
; again at the calibration speeds and shows the result on the screen
; ("Phase stepping check: OK" or "... recalibration suggested"); PrusaLink
; shows the details on the phase stepping page.
; Firmware without M976 reports it as an unknown command and carries on.
;
; M976 homes X and Y if needed (like the phase stepping calibration, no Z
; probing), lowers the bed to Z 50 and runs its moves around the bed
; centre, the same moves as the calibration's verification.
; Park position: the middle of the sound test's safe area
; (X 30..230, Y 30..190, see phase-stepping-speed-model-sound-test.gcode).

G90 ; absolute positioning
G21 ; millimetres
G28 O X Y ; home X and Y if needed (no Z probing), so the park move is safe on any firmware
M400

M117 Phase stepping check: measuring
M976 ; check both motors, about 1 minute
M400
G4 S30 ; keep the result on the screen

G1 X120 Y110 F3000 ; park in the middle of the safe area
M400
M84 ; motors off
