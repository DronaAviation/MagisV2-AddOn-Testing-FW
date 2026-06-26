/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2-AddOn-Testing-FW                                          #
 #  File: src/addon/page_servo.cpp                                            #
 #  Brief: Servo Test page. One item + Back. Servo Out drives OUT_PWM via      #
 #         Servo_Write (1000..2000us):                                         #
 #           OK on Servo Out : start/stop a 1000->1600->1000 sweep (def 1000)  #
 #         Leaving the page stops the sweep and parks at 1000.                 #
 *******************************************************************************/

#include "page_servo.h"

#include "addon_common.h"

#define SERVO_STEP_MS 50       // sweep step + page redraw cadence (frame)
#define SERVO_STEP    50       // us advanced per frame; sweep time each way = (SERVO_MAX-SERVO_MIN)/SERVO_STEP * SERVO_STEP_MS
#define SERVO_MIN     1000
#define SERVO_MAX     1600

// Servo-page item indices (Back is the last item).
enum { SRV_SERVO = 0,
       SRV_BACK  = 1 };

static bool     servoActive = false;     // sweep running?
static int16_t  servoVal    = SERVO_MIN; // current pulse 1000..2000
static int8_t   servoDir    = 1;         // sweep direction (+1 up, -1 down)

uint8_t ServoPage_ItemCount ( void ) {
  return 2;    // Servo Out, Back
}

uint16_t ServoPage_FrameMs ( void ) {
  return SERVO_STEP_MS;
}

void ServoPage_Reset ( void ) {
  servoActive = false;
  servoVal    = SERVO_MIN;
  servoDir    = 1;
  Servo_Write ( OUT_PWM, SERVO_MIN );
}

void ServoPage_Enter ( void ) {
  ServoPage_Reset ( );
  waveReset ( );
}

/**
 * @brief Advance the servo sweep (when active) and drive the output.
 */
static void servoUpdate ( void ) {
  if ( servoActive ) {
    int16_t v = ( int16_t ) ( servoVal + servoDir * SERVO_STEP );
    if ( v >= SERVO_MAX ) {
      v        = SERVO_MAX;
      servoDir = -1;
    } else if ( v <= SERVO_MIN ) {
      v        = SERVO_MIN;
      servoDir = 1;
    }
    servoVal = v;
  }
  Servo_Write ( OUT_PWM, ( uint16_t ) servoVal );
}

void ServoPage_Draw ( uint8_t pageSel ) {
  servoUpdate ( );    // advance sweep + drive hardware

  drawHeader ( "Servo Test" );

  // Servo Out row (value is always 4 digits)
  if ( pageSel == SRV_SERVO ) Oled_Text ( 2, 15, ">" );
  Oled_Text ( 10, 15, "Servo Out" );
  Oled_Number ( 84, 15, servoVal );

  // Current consumption (read-only), offset by the board+OLED baseline draw,
  // right-aligned before "mA"
  uint16_t mA  = Bms_Get ( Current );
  if ( mA > 32767 ) mA = 32767;
  uint16_t off = currentOffset ( );    // measured board+OLED baseline
  mA = ( mA > off ) ? ( uint16_t ) ( mA - off ) : 0;
  Oled_Text ( 10, 26, "Current" );
  int16_t d = ( mA >= 10000 ) ? 5 : ( mA >= 1000 ) ? 4 : ( mA >= 100 ) ? 3 : ( mA >= 10 ) ? 2 : 1;
  Oled_Number ( ( int16_t ) ( 108 - d * 6 ), 27, ( int16_t ) mA );
  Oled_Text ( 110, 27, "mA" );

  // One sample per frame (the page is drawn at SERVO_STEP_MS), then plot it.
  wavePush ( mA );
  waveDraw ( 36, 16 );    // current-vs-time graph, rows 36..51

  drawFooter ( pageSel == SRV_BACK );
}

void ServoPage_Action ( uint8_t sel ) {
  if ( sel == SRV_SERVO ) {
    servoActive = ! servoActive;               // start/stop sweep
    if ( ! servoActive ) {                     // stopping -> park at default 1000
      servoVal = SERVO_MIN;
      servoDir = 1;
    }
  }
}
