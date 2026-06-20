/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

extern bool z_auto_align_done;
/**
 * stepper/indirection.h
 *
 * Stepper motor driver indirection to allow some stepper functions to
 * be done via SPI/I2c instead of direct pin manipulation.
 *
 * Copyright (c) 2015 Dominik Wenger
 */

#include "../../inc/MarlinConfigPre.h"

#if HAS_DRIVER(TMC26X)
  // #error dead code found by automatic analyses (see BFW-5461)
  #include "TMC26X.h"
#endif

#if HAS_TRINAMIC
  #include "trinamic.h"
#endif

#include <option/has_mmu2.h>
#include <option/has_toolchanger.h>

//
// X, Y, Z Stepper enable / disable
//

#include <option/has_planner.h>
#if HAS_PLANNER()
  #include <module/motion.h>
#endif

#if HAS_X_ENABLE
  #define X_enable  X_ENABLE_WRITE( X_ENABLE_ON)
  #define X_disable X_ENABLE_WRITE(!X_ENABLE_ON)
#else
  #define X_enable  NOOP
  #define X_disable NOOP
#endif

#define  enable_X() do{ X_enable; }while(0)
#define disable_X() do{ X_disable; TERN_(HAS_PLANNER(), axes_home_level[X_AXIS] = AxisHomeLevel::not_homed;) }while(0)

#if HAS_Y_ENABLE
  #define Y_enable  Y_ENABLE_WRITE( Y_ENABLE_ON)
  #define Y_disable Y_ENABLE_WRITE(!Y_ENABLE_ON)
#else
  #define Y_enable  NOOP
  #define Y_disable NOOP
#endif

#if DISABLED(XY_LINKED_ENABLE)
#define  enable_Y() do{ Y_enable; }while(0)
#define disable_Y() do{ Y_disable; TERN_(HAS_PLANNER(), axes_home_level[Y_AXIS] = AxisHomeLevel::not_homed;) }while(0)
#endif

#if ENABLED(XY_LINKED_ENABLE)
  #define  enable_XY() enable_X()
  #define disable_XY() []{ disable_X(); TERN_(HAS_PLANNER(), axes_home_level[Y_AXIS] = AxisHomeLevel::not_homed;) }()
#else
  #define  enable_XY() do{enable_X(); enable_Y(); }while(0)
  #define disable_XY() do{disable_X(); disable_Y(); }while(0)
#endif

#if HAS_Z_ENABLE
  #define Z_enable  Z_ENABLE_WRITE( Z_ENABLE_ON)
  #define Z_disable Z_ENABLE_WRITE(!Z_ENABLE_ON)
#else
  #define Z_enable  NOOP
  #define Z_disable NOOP
#endif

#if HAS_Z2_ENABLE
  // #error dead code found by automatic analyses (see BFW-5461)
  #define Z2_enable  Z2_ENABLE_WRITE( Z_ENABLE_ON)
  #define Z2_disable Z2_ENABLE_WRITE(!Z_ENABLE_ON)
#else
  #define Z2_enable  NOOP
  #define Z2_disable NOOP
#endif

#if HAS_Z3_ENABLE
  // #error dead code found by automatic analyses (see BFW-5461)
  #define Z3_enable  Z3_ENABLE_WRITE( Z_ENABLE_ON)
  #define Z3_disable Z3_ENABLE_WRITE(!Z_ENABLE_ON)
#else
  #define Z3_enable  NOOP
  #define Z3_disable NOOP
#endif

#define  enable_Z() do{ Z_enable; Z2_enable; Z3_enable; }while(0)
#define disable_Z() do{ Z_disable; Z2_disable; Z3_disable; TERN_(HAS_PLANNER(), axes_home_level[Z_AXIS] = AxisHomeLevel::not_homed;) z_auto_align_done = false; }while(0)

//
// Extruder Stepper enable / disable
//

#if BOARD_IS_DWARF()
    #define X_APPLY_DIR(v) X_DIR_WRITE(v)
    #define Y_APPLY_DIR(v) Y_DIR_WRITE(v)
    #define Z_APPLY_DIR(v) Z_DIR_WRITE(v)
    #define E_APPLY_DIR(v) E0_DIR_WRITE(v)
#else
    #define X_APPLY_DIR(v) buddy::hw::xDir.writeb(v)
    #define Y_APPLY_DIR(v) buddy::hw::yDir.writeb(v)
    #define Z_APPLY_DIR(v) buddy::hw::zDir.writeb(v)
    #define E_APPLY_DIR(v) buddy::hw::e0Dir.writeb(v)
#endif

#if ENABLED(SQUARE_WAVE_STEPPING)
    #if PRINTER_IS_PRUSA_XL() && !BOARD_IS_DWARF()
        // on XLBuddy the XY pin assignment is dynamic depending on board revision
        #define X_STEP_SET() buddy::hw::XStep->toggle();
        #define Y_STEP_SET() buddy::hw::YStep->toggle();
    #else
        #define X_STEP_SET() buddy::hw::xStep.toggle();
        #define Y_STEP_SET() buddy::hw::yStep.toggle();
    #endif
    #define Z_STEP_SET() buddy::hw::zStep.toggle();
    #define E_STEP_SET() buddy::hw::e0Step.toggle();

    #define X_STEP_RESET()
    #define Y_STEP_RESET()
    #define Z_STEP_RESET()
    #define E_STEP_RESET()
#else
    #define X_STEP_SET() X_STEP_WRITE(1)
    #define Y_STEP_SET() Y_STEP_WRITE(1)
    #define Z_STEP_SET() Z_STEP_WRITE(1)
    #define E_STEP_SET() E0_STEP_WRITE(1)

    #define X_STEP_RESET() X_STEP_WRITE(0)
    #define Y_STEP_RESET() Y_STEP_WRITE(0)
    #define Z_STEP_RESET() Z_STEP_WRITE(0)
    #define E_STEP_RESET() E0_STEP_WRITE(0)
#endif

// X Stepper
#ifndef X_ENABLE_INIT
  #define X_ENABLE_INIT SET_OUTPUT(X_ENABLE_PIN)
  #define X_ENABLE_WRITE(STATE) WRITE(X_ENABLE_PIN,STATE)
  #define X_ENABLE_READ() READ(X_ENABLE_PIN)
#endif
#ifndef X_DIR_INIT
  #define X_DIR_INIT SET_OUTPUT(X_DIR_PIN)
  #define X_DIR_WRITE(STATE) WRITE(X_DIR_PIN,STATE)
  #define X_DIR_READ() READ(X_DIR_PIN)
#endif
#define X_STEP_INIT SET_OUTPUT(X_STEP_PIN)
#ifndef X_STEP_WRITE
  #define X_STEP_WRITE(STATE) WRITE(X_STEP_PIN,STATE)
#endif
#define X_STEP_READ READ(X_STEP_PIN)

// Y Stepper
#ifndef Y_ENABLE_INIT
  #if DISABLED(XY_LINKED_ENABLE)
    #define Y_ENABLE_INIT SET_OUTPUT(Y_ENABLE_PIN)
  #else
    #define Y_ENABLE_INIT NOOP
  #endif
  #define Y_ENABLE_WRITE(STATE) WRITE(Y_ENABLE_PIN,STATE)
  #define Y_ENABLE_READ() READ(Y_ENABLE_PIN)
#endif
#ifndef Y_DIR_INIT
  #define Y_DIR_INIT SET_OUTPUT(Y_DIR_PIN)
  #define Y_DIR_WRITE(STATE) WRITE(Y_DIR_PIN,STATE)
  #define Y_DIR_READ() READ(Y_DIR_PIN)
#endif
#define Y_STEP_INIT SET_OUTPUT(Y_STEP_PIN)
#ifndef Y_STEP_WRITE
  #define Y_STEP_WRITE(STATE) WRITE(Y_STEP_PIN,STATE)
#endif
#define Y_STEP_READ READ(Y_STEP_PIN)

// Z Stepper
#ifndef Z_ENABLE_INIT
  #define Z_ENABLE_INIT SET_OUTPUT(Z_ENABLE_PIN)
  #define Z_ENABLE_WRITE(STATE) WRITE(Z_ENABLE_PIN,STATE)
  #define Z_ENABLE_READ() READ(Z_ENABLE_PIN)
#endif
#ifndef Z_DIR_INIT
  #define Z_DIR_INIT SET_OUTPUT(Z_DIR_PIN)
  #define Z_DIR_WRITE(STATE) WRITE(Z_DIR_PIN,STATE)
  #define Z_DIR_READ() READ(Z_DIR_PIN)
#endif
#define Z_STEP_INIT SET_OUTPUT(Z_STEP_PIN)
#ifndef Z_STEP_WRITE
  #define Z_STEP_WRITE(STATE) WRITE(Z_STEP_PIN,STATE)
#endif
#define Z_STEP_READ READ(Z_STEP_PIN)

// Z2 Stepper
#if HAS_Z2_ENABLE
  // #error dead code found by automatic analyses (see BFW-5461)
  #ifndef Z2_ENABLE_INIT
    // #error dead code found by automatic analyses (see BFW-5461)
    #define Z2_ENABLE_INIT SET_OUTPUT(Z2_ENABLE_PIN)
    #define Z2_ENABLE_WRITE(STATE) WRITE(Z2_ENABLE_PIN,STATE)
    #define Z2_ENABLE_READ() READ(Z2_ENABLE_PIN)
  #endif
  #ifndef Z2_DIR_INIT
    // #error dead code found by automatic analyses (see BFW-5461)
    #define Z2_DIR_INIT SET_OUTPUT(Z2_DIR_PIN)
    #define Z2_DIR_WRITE(STATE) WRITE(Z2_DIR_PIN,STATE)
    #define Z2_DIR_READ() READ(Z2_DIR_PIN)
  #endif
  #define Z2_STEP_INIT SET_OUTPUT(Z2_STEP_PIN)
  #ifndef Z2_STEP_WRITE
    // #error dead code found by automatic analyses (see BFW-5461)
    #define Z2_STEP_WRITE(STATE) WRITE(Z2_STEP_PIN,STATE)
  #endif
  #define Z2_STEP_READ READ(Z2_STEP_PIN)
#else
  #define Z2_DIR_WRITE(STATE) NOOP
#endif

// Z3 Stepper
#if HAS_Z3_ENABLE
  // #error dead code found by automatic analyses (see BFW-5461)
  #ifndef Z3_ENABLE_INIT
    // #error dead code found by automatic analyses (see BFW-5461)
    #define Z3_ENABLE_INIT SET_OUTPUT(Z3_ENABLE_PIN)
    #define Z3_ENABLE_WRITE(STATE) WRITE(Z3_ENABLE_PIN,STATE)
    #define Z3_ENABLE_READ() READ(Z3_ENABLE_PIN)
  #endif
  #ifndef Z3_DIR_INIT
    // #error dead code found by automatic analyses (see BFW-5461)
    #define Z3_DIR_INIT SET_OUTPUT(Z3_DIR_PIN)
    #define Z3_DIR_WRITE(STATE) WRITE(Z3_DIR_PIN,STATE)
    #define Z3_DIR_READ() READ(Z3_DIR_PIN)
  #endif
  #define Z3_STEP_INIT SET_OUTPUT(Z3_STEP_PIN)
  #ifndef Z3_STEP_WRITE
    // #error dead code found by automatic analyses (see BFW-5461)
    #define Z3_STEP_WRITE(STATE) WRITE(Z3_STEP_PIN,STATE)
  #endif
  #define Z3_STEP_READ READ(Z3_STEP_PIN)
#else
  #define Z3_DIR_WRITE(STATE) NOOP
#endif

// E0 Stepper
#ifndef E0_ENABLE_INIT
  #define E0_ENABLE_INIT SET_OUTPUT(E0_ENABLE_PIN)
  #define E0_ENABLE_WRITE(STATE) WRITE(E0_ENABLE_PIN,STATE)
  #define E0_ENABLE_READ() READ(E0_ENABLE_PIN)
#endif
#ifndef E0_DIR_INIT
  #define E0_DIR_INIT SET_OUTPUT(E0_DIR_PIN)
  #define E0_DIR_WRITE(STATE) WRITE(E0_DIR_PIN,STATE)
  #define E0_DIR_READ() READ(E0_DIR_PIN)
#endif
#define E0_STEP_INIT SET_OUTPUT(E0_STEP_PIN)
#ifndef E0_STEP_WRITE
  #define E0_STEP_WRITE(STATE) WRITE(E0_STEP_PIN,STATE)
#endif
#define E0_STEP_READ READ(E0_STEP_PIN)

// E1 Stepper
#ifndef E1_ENABLE_INIT
  #define E1_ENABLE_INIT SET_OUTPUT(E1_ENABLE_PIN)
  #define E1_ENABLE_WRITE(STATE) WRITE(E1_ENABLE_PIN,STATE)
  #define E1_ENABLE_READ() READ(E1_ENABLE_PIN)
#endif
#ifndef E1_DIR_INIT
  #define E1_DIR_INIT SET_OUTPUT(E1_DIR_PIN)
  #define E1_DIR_WRITE(STATE) WRITE(E1_DIR_PIN,STATE)
  #define E1_DIR_READ() READ(E1_DIR_PIN)
#endif
#define E1_STEP_INIT SET_OUTPUT(E1_STEP_PIN)
#ifndef E1_STEP_WRITE
  #define E1_STEP_WRITE(STATE) WRITE(E1_STEP_PIN,STATE)
#endif
#define E1_STEP_READ READ(E1_STEP_PIN)

// E2 Stepper
#ifndef E2_ENABLE_INIT
  #define E2_ENABLE_INIT SET_OUTPUT(E2_ENABLE_PIN)
  #define E2_ENABLE_WRITE(STATE) WRITE(E2_ENABLE_PIN,STATE)
  #define E2_ENABLE_READ() READ(E2_ENABLE_PIN)
#endif
#ifndef E2_DIR_INIT
  #define E2_DIR_INIT SET_OUTPUT(E2_DIR_PIN)
  #define E2_DIR_WRITE(STATE) WRITE(E2_DIR_PIN,STATE)
  #define E2_DIR_READ() READ(E2_DIR_PIN)
#endif
#define E2_STEP_INIT SET_OUTPUT(E2_STEP_PIN)
#ifndef E2_STEP_WRITE
  #define E2_STEP_WRITE(STATE) WRITE(E2_STEP_PIN,STATE)
#endif
#define E2_STEP_READ READ(E2_STEP_PIN)

// E3 Stepper
#ifndef E3_ENABLE_INIT
  #define E3_ENABLE_INIT SET_OUTPUT(E3_ENABLE_PIN)
  #define E3_ENABLE_WRITE(STATE) WRITE(E3_ENABLE_PIN,STATE)
  #define E3_ENABLE_READ() READ(E3_ENABLE_PIN)
#endif
#ifndef E3_DIR_INIT
  #define E3_DIR_INIT SET_OUTPUT(E3_DIR_PIN)
  #define E3_DIR_WRITE(STATE) WRITE(E3_DIR_PIN,STATE)
  #define E3_DIR_READ() READ(E3_DIR_PIN)
#endif
#define E3_STEP_INIT SET_OUTPUT(E3_STEP_PIN)
#ifndef E3_STEP_WRITE
  #define E3_STEP_WRITE(STATE) WRITE(E3_STEP_PIN,STATE)
#endif
#define E3_STEP_READ READ(E3_STEP_PIN)

// E4 Stepper
#ifndef E4_ENABLE_INIT
  #define E4_ENABLE_INIT SET_OUTPUT(E4_ENABLE_PIN)
  #define E4_ENABLE_WRITE(STATE) WRITE(E4_ENABLE_PIN,STATE)
  #define E4_ENABLE_READ() READ(E4_ENABLE_PIN)
#endif
#ifndef E4_DIR_INIT
  #define E4_DIR_INIT SET_OUTPUT(E4_DIR_PIN)
  #define E4_DIR_WRITE(STATE) WRITE(E4_DIR_PIN,STATE)
  #define E4_DIR_READ() READ(E4_DIR_PIN)
#endif
#define E4_STEP_INIT SET_OUTPUT(E4_STEP_PIN)
#ifndef E4_STEP_WRITE
  #define E4_STEP_WRITE(STATE) WRITE(E4_STEP_PIN,STATE)
#endif
#define E4_STEP_READ READ(E4_STEP_PIN)

// E5 Stepper
#ifndef E5_ENABLE_INIT
  #define E5_ENABLE_INIT SET_OUTPUT(E5_ENABLE_PIN)
  #define E5_ENABLE_WRITE(STATE) WRITE(E5_ENABLE_PIN,STATE)
  #define E5_ENABLE_READ() READ(E5_ENABLE_PIN)
#endif
#ifndef E5_DIR_INIT
  #define E5_DIR_INIT SET_OUTPUT(E5_DIR_PIN)
  #define E5_DIR_WRITE(STATE) WRITE(E5_DIR_PIN,STATE)
  #define E5_DIR_READ() READ(E5_DIR_PIN)
#endif
#define E5_STEP_INIT SET_OUTPUT(E5_STEP_PIN)
#ifndef E5_STEP_WRITE
  #define E5_STEP_WRITE(STATE) WRITE(E5_STEP_PIN,STATE)
#endif
#define E5_STEP_READ READ(E5_STEP_PIN)

/**
 * Extruder indirection for the single E axis
 */
#if HAS_MMU2()
  #define E_STEP_WRITE(E,V) E0_STEP_WRITE(V)
  #define   NORM_E_DIR(E)   E0_DIR_WRITE(!INVERT_E0_DIR)
  #define    REV_E_DIR(E)   E0_DIR_WRITE( INVERT_E0_DIR)

#elif E_STEPPERS > 1
  // #error dead code found by automatic analyses (see BFW-5461)

  #if E_STEPPERS > 5
    // #error dead code found by automatic analyses (see BFW-5461)
    #define _E_STEP_WRITE(E,V) do{ switch (E) { case 0: E0_STEP_WRITE(V); break; case 1: E1_STEP_WRITE(V); break; case 2: E2_STEP_WRITE(V); break; case 3: E3_STEP_WRITE(V); break; case 4: E4_STEP_WRITE(V); case 5: E5_STEP_WRITE(V); } }while(0)
    #define   _NORM_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE(!INVERT_E0_DIR); break; case 1: E1_DIR_WRITE(!INVERT_E1_DIR); break; case 2: E2_DIR_WRITE(!INVERT_E2_DIR); break; case 3: E3_DIR_WRITE(!INVERT_E3_DIR); break; case 4: E4_DIR_WRITE(!INVERT_E4_DIR); case 5: E5_DIR_WRITE(!INVERT_E5_DIR); } }while(0)
    #define    _REV_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE( INVERT_E0_DIR); break; case 1: E1_DIR_WRITE( INVERT_E1_DIR); break; case 2: E2_DIR_WRITE( INVERT_E2_DIR); break; case 3: E3_DIR_WRITE( INVERT_E3_DIR); break; case 4: E4_DIR_WRITE( INVERT_E4_DIR); case 5: E5_DIR_WRITE( INVERT_E5_DIR); } }while(0)
  #elif E_STEPPERS > 4
    // #error dead code found by automatic analyses (see BFW-5461)
    #define _E_STEP_WRITE(E,V) do{ switch (E) { case 0: E0_STEP_WRITE(V); break; case 1: E1_STEP_WRITE(V); break; case 2: E2_STEP_WRITE(V); break; case 3: E3_STEP_WRITE(V); break; case 4: E4_STEP_WRITE(V); } }while(0)
    #define   _NORM_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE(!INVERT_E0_DIR); break; case 1: E1_DIR_WRITE(!INVERT_E1_DIR); break; case 2: E2_DIR_WRITE(!INVERT_E2_DIR); break; case 3: E3_DIR_WRITE(!INVERT_E3_DIR); break; case 4: E4_DIR_WRITE(!INVERT_E4_DIR); } }while(0)
    #define    _REV_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE( INVERT_E0_DIR); break; case 1: E1_DIR_WRITE( INVERT_E1_DIR); break; case 2: E2_DIR_WRITE( INVERT_E2_DIR); break; case 3: E3_DIR_WRITE( INVERT_E3_DIR); break; case 4: E4_DIR_WRITE( INVERT_E4_DIR); } }while(0)
  #elif E_STEPPERS > 3
    // #error dead code found by automatic analyses (see BFW-5461)
    #define _E_STEP_WRITE(E,V) do{ switch (E) { case 0: E0_STEP_WRITE(V); break; case 1: E1_STEP_WRITE(V); break; case 2: E2_STEP_WRITE(V); break; case 3: E3_STEP_WRITE(V); } }while(0)
    #define   _NORM_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE(!INVERT_E0_DIR); break; case 1: E1_DIR_WRITE(!INVERT_E1_DIR); break; case 2: E2_DIR_WRITE(!INVERT_E2_DIR); break; case 3: E3_DIR_WRITE(!INVERT_E3_DIR); } }while(0)
    #define    _REV_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE( INVERT_E0_DIR); break; case 1: E1_DIR_WRITE( INVERT_E1_DIR); break; case 2: E2_DIR_WRITE( INVERT_E2_DIR); break; case 3: E3_DIR_WRITE( INVERT_E3_DIR); } }while(0)
  #elif E_STEPPERS > 2
    // #error dead code found by automatic analyses (see BFW-5461)
    #define _E_STEP_WRITE(E,V) do{ switch (E) { case 0: E0_STEP_WRITE(V); break; case 1: E1_STEP_WRITE(V); break; case 2: E2_STEP_WRITE(V); } }while(0)
    #define   _NORM_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE(!INVERT_E0_DIR); break; case 1: E1_DIR_WRITE(!INVERT_E1_DIR); break; case 2: E2_DIR_WRITE(!INVERT_E2_DIR); } }while(0)
    #define    _REV_E_DIR(E)   do{ switch (E) { case 0: E0_DIR_WRITE( INVERT_E0_DIR); break; case 1: E1_DIR_WRITE( INVERT_E1_DIR); break; case 2: E2_DIR_WRITE( INVERT_E2_DIR); } }while(0)
  #else
    // #error dead code found by automatic analyses (see BFW-5461)
    #define _E_STEP_WRITE(E,V) do{ if (E == 0) { E0_STEP_WRITE(V); } else { E1_STEP_WRITE(V); } }while(0)
    #define   _NORM_E_DIR(E)   do{ if (E == 0) { E0_DIR_WRITE(!INVERT_E0_DIR); } else { E1_DIR_WRITE(!INVERT_E1_DIR); } }while(0)
    #define    _REV_E_DIR(E)   do{ if (E == 0) { E0_DIR_WRITE( INVERT_E0_DIR); } else { E1_DIR_WRITE( INVERT_E1_DIR); } }while(0)
  #endif

    #define E_STEP_WRITE(E,V) _E_STEP_WRITE(E,V)
    #define   NORM_E_DIR(E)   _NORM_E_DIR(E)
    #define    REV_E_DIR(E)   _REV_E_DIR(E)

#elif E_STEPPERS
  #define E_STEP_WRITE(E,V) E0_STEP_WRITE(V)
  #define   NORM_E_DIR(E)   E0_DIR_WRITE(!INVERT_E0_DIR)
  #define    REV_E_DIR(E)   E0_DIR_WRITE( INVERT_E0_DIR)

#else
  // #error dead code found by automatic analyses (see BFW-5461)
  #define E_STEP_WRITE(E,V) NOOP
  #define   NORM_E_DIR(E)   NOOP
  #define    REV_E_DIR(E)   NOOP

#endif

// Enable/disable stepper by axis
// !!! Do not remove, Yuri needs these
void stepper_enable(AxisEnum axis, bool enabled=true);
static inline void stepper_disable(AxisEnum axis) { stepper_enable(axis, false); }

// Return stepper enabled status
// !!! Do not remove, Yuri needs these
[[nodiscard]] bool stepper_enabled(AxisEnum axis);

void enable_E(uint8_t index);
void disable_E(uint8_t index);
