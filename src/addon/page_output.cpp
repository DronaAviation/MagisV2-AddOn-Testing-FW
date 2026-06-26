/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2 — AddOn Testing                                           #
 #  File: src/addon/page_output.cpp                                           #
 #  Brief: Output Test page. Both items share one PWM pin (OUT_PWM):           #
 #           Digital Out : OK toggles HIGH/LOW  -> PWM 100% / 0%  (default LOW) #
 #           Dimmer Out  : OK starts/stops a 0->100->0 PWM ramp   (default 0)   #
 #         The dimmer ramp takes priority on the pin; otherwise the digital     #
 #         level drives it. Leaving the page restores defaults.                #
 *******************************************************************************/

#include "page_output.h"

#include "addon_common.h"

#define DIM_STEP_MS 20         // dimmer step + page redraw cadence (frame); 100 steps -> ramp time = 100 * DIM_STEP_MS

// Output-page item indices (Back is always the last item).
enum { OUT_DIGITAL = 0,
       OUT_DIMMER  = 1,
       OUT_BACK    = 2 };

static bool     doutHigh     = false;    // digital-out state (default LOW)
static bool     dimmerActive = false;    // dimmer ramp running?
static uint8_t  dimmerVal    = 0;        // current dimmer duty 0..100
static int8_t   dimmerDir    = 1;        // ramp direction (+1 up, -1 down)

uint8_t OutputPage_ItemCount ( void ) {
  return 3;    // Digital, Dimmer, Back
}

uint16_t OutputPage_FrameMs ( void ) {
  return DIM_STEP_MS;
}

void OutputPage_Reset ( void ) {
  doutHigh     = false;
  dimmerActive = false;
  dimmerVal    = 0;
  dimmerDir    = 1;
  Peripheral_Write ( OUT_PWM, 0 );
}

/**
 * @brief Advance the dimmer ramp by one step (when active) and drive both
 *        outputs. Called once per frame, so the step cadence is the frame rate.
 */
static void outputUpdate ( void ) {
  if ( dimmerActive ) {
    int16_t v = ( int16_t ) ( dimmerVal + dimmerDir );
    if ( v >= 100 ) {
      v         = 100;
      dimmerDir = -1;
    } else if ( v <= 0 ) {
      v         = 0;
      dimmerDir = 1;
    }
    dimmerVal = ( uint8_t ) v;
  }
  // Shared PWM pin: dimmer ramp takes priority; else digital level (0/100%).
  uint16_t out = dimmerActive ? dimmerVal : ( doutHigh ? 100 : 0 );
  Peripheral_Write ( OUT_PWM, out );
}

void OutputPage_Draw ( uint8_t pageSel ) {
  outputUpdate ( );    // advance ramp + drive hardware

  drawHeader ( "Output Test" );

  // Digital Out row (same top spacing as the Input page value line, y=15)
  if ( pageSel == OUT_DIGITAL ) Oled_Text ( 2, 15, ">" );
  Oled_Text ( 10, 15, "Digital Out" );
  Oled_Text ( 90, 15, doutHigh ? "HIGH" : "LOW" );

  // Dimmer Out row, ~5px below (value right-aligned before the '%')
  if ( pageSel == OUT_DIMMER ) Oled_Text ( 2, 27, ">" );
  Oled_Text ( 10, 27, "Dimmer Out" );
  int16_t digits = ( dimmerVal >= 100 ) ? 3 : ( dimmerVal >= 10 ) ? 2 : 1;
  Oled_Number ( ( int16_t ) ( 104 - digits * 6 ), 27, ( int16_t ) dimmerVal );
  Oled_Text ( 104, 27, "%" );

  // Dimmer level bar (fill only, no outline): rows 39..51, 2px above footer
  int16_t fillW = ( int16_t ) ( ( uint32_t ) 124 * dimmerVal / 100 );
  if ( fillW > 0 ) Oled_Rect ( 2, 39, fillW, 13 );

  drawFooter ( pageSel == OUT_BACK );
}

void OutputPage_Action ( uint8_t sel ) {
  if ( sel == OUT_DIGITAL ) {
    doutHigh = ! doutHigh;                     // toggle digital out
  } else if ( sel == OUT_DIMMER ) {
    dimmerActive = ! dimmerActive;             // start/stop ramp
    if ( ! dimmerActive ) {                    // stopping -> back to default 0
      dimmerVal = 0;
      dimmerDir = 1;
    }
  }
}
