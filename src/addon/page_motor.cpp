/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2-AddOn-Testing-FW                                          #
 #  File: src/addon/page_motor.cpp                                            #
 #  Brief: Motor Test page. Motors wired on M7/M8, shown as M1/M2. Pick a      #
 #         target (one motor or ALL) and run ONE 1000->2000->1000 shot:        #
 #         ramp up, hold 0.5s at the top, ramp down, then auto-stop.           #
 #           OK on Motor : cycle target M1 -> M2 -> ALL  (stops the test)      #
 #           OK on Test  : run the one-shot test (press again to abort)        #
 #         Leaving the page stops the test and sets the motors to 1000 (off).   #
 #                                                                             #
 #  SAFETY: MOTOR_TEST_MAX caps the bench throttle (1000..2000). Raise it       #
 #  deliberately - props will spin.                                            #
 *******************************************************************************/

#include "page_motor.h"

#include "addon_common.h"

#include "API/Motor.h"

#define MOTOR_TEST_MAX 2000    // top of the ramp (coreless 0720/0820, no props)
#define MOTOR_STEP_MS  50      // ramp step + page redraw cadence (frame)
#define MOTOR_STEP     25      // throttle units advanced per frame; ramp time each way = (MOTOR_TEST_MAX-1000)/MOTOR_STEP * MOTOR_STEP_MS
#define MOTOR_HOLD_MS  1000     // dwell at MOTOR_TEST_MAX before ramping back down

// Motor targets and page-item indices.
// Only M7/M8 are wired; they are shown on the display as M1/M2.
enum { MT_M1 = 0, MT_M2, MT_ALL, MT_COUNT };
enum { MOT_TARGET = 0, MOT_TEST = 1, MOT_BACK = 2 };

static const char *const           targetName  [ MT_COUNT ] = { "M1", "M2", "ALL" };
static const bidirectional_motor_e targetMotor [ 2 ]        = { M8, M7 };

// Single-shot test phases: ramp 1000->MAX, hold at MAX, ramp MAX->1000, stop.
enum { MOT_UP = 0, MOT_HOLD, MOT_DOWN };

static uint8_t  motTarget   = MT_M1;     // selected target (default M1 -> M7)
static bool     motActive   = false;     // single-shot test running?
static int16_t  motVal      = 1000;      // current throttle 1000..MOTOR_TEST_MAX
static uint8_t  motPhase    = MOT_UP;    // phase within the current run
static uint32_t holdStartMs = 0;         // millis() when the top-of-ramp hold began

uint8_t MotorPage_ItemCount ( void ) {
  return 3;    // Target, Test, Back
}

uint16_t MotorPage_FrameMs ( void ) {
  return MOTOR_STEP_MS;
}

/**
 * @brief Stop every wired motor (M7/M8 -> 1000).
 */
static void motorsAllOff ( void ) {
  Motor_Set ( M7, 1000 );
  Motor_Set ( M8, 1000 );
}

/**
 * @brief Drive the selected target at @p val; the other motor stays at 1000.
 */
static void applyMotors ( int16_t val ) {
  for ( uint8_t i = 0; i < 2; i++ ) {
    bool on = ( motTarget == MT_ALL ) || ( motTarget == i );
    Motor_Set ( targetMotor [ i ], ( int16_t ) ( on ? val : 1000 ) );
  }
}

void MotorPage_Reset ( void ) {
  motActive = false;
  motVal    = 1000;
  motPhase  = MOT_UP;
  motorsAllOff ( );
}

void MotorPage_Enter ( void ) {
  MotorPage_Reset ( );
  waveReset ( );
}

/**
 * @brief Advance the single-shot test and drive the target. Called once per
 *        frame: ramp up to MOTOR_TEST_MAX, hold there for MOTOR_HOLD_MS, ramp
 *        back to 1000, then stop (motActive cleared — test done).
 */
static void motorUpdate ( void ) {
  if ( motActive ) {
    switch ( motPhase ) {
      case MOT_UP:
        motVal = ( int16_t ) ( motVal + MOTOR_STEP );
        if ( motVal >= MOTOR_TEST_MAX ) {
          motVal      = MOTOR_TEST_MAX;
          motPhase    = MOT_HOLD;
          holdStartMs = millis ( );
        }
        break;
      case MOT_HOLD:
        motVal = MOTOR_TEST_MAX;
        if ( millis ( ) - holdStartMs >= MOTOR_HOLD_MS ) motPhase = MOT_DOWN;
        break;
      case MOT_DOWN:
        motVal = ( int16_t ) ( motVal - MOTOR_STEP );
        if ( motVal <= 1000 ) {
          motVal    = 1000;
          motActive = false;     // ramped back down -> test done
          motPhase  = MOT_UP;    // armed for the next run
        }
        break;
    }
  } else {
    motVal = 1000;
  }
  applyMotors ( motVal );
}

void MotorPage_Draw ( uint8_t pageSel ) {
  motorUpdate ( );    // advance the single-shot test + drive motors

  drawHeader ( "Motor Test" );

  // Target row
  if ( pageSel == MOT_TARGET ) Oled_Text ( 2, 14, ">" );
  Oled_Text ( 10, 14, "Motor" );
  Oled_Text ( 90, 14, targetName [ motTarget ] );

  // Test row: throttle value fed to the motor (or OFF) + current readout
  if ( pageSel == MOT_TEST ) Oled_Text ( 2, 24, ">" );
  Oled_Text ( 10, 24, "Test" );
  if ( motActive )
    Oled_Number ( 48, 24, motVal );        // live throttle value (1000..2000)
  else
    Oled_Text ( 48, 24, "OFF" );
  uint16_t mA  = Bms_Get ( Current );
  if ( mA > 32767 ) mA = 32767;
  uint16_t off = currentOffset ( );    // measured board+OLED baseline
  mA = ( mA > off ) ? ( uint16_t ) ( mA - off ) : 0;
  int16_t d = ( mA >= 10000 ) ? 5 : ( mA >= 1000 ) ? 4 : ( mA >= 100 ) ? 3 : ( mA >= 10 ) ? 2 : 1;
  Oled_Number ( ( int16_t ) ( 108 - d * 6 ), 24, ( int16_t ) mA );
  Oled_Text ( 110, 24, "mA" );

  // One sample per frame (the page is drawn at MOTOR_STEP_MS), then plot it.
  wavePush ( mA );
  waveDraw ( 34, 18 );

  drawFooter ( pageSel == MOT_BACK );
}

void MotorPage_Action ( uint8_t sel ) {
  if ( sel == MOT_TARGET ) {
    MotorPage_Reset ( );                       // switching target stops the test (safety)
    motTarget = ( uint8_t ) ( ( motTarget + 1 ) % MT_COUNT );
  } else if ( sel == MOT_TEST ) {
    if ( ! motActive ) {                       // start a fresh single-shot run from 1000
      motVal    = 1000;
      motPhase  = MOT_UP;
      motActive = true;
    } else {                                   // pressed again mid-run -> abort, motors off
      MotorPage_Reset ( );
    }
  }
}
