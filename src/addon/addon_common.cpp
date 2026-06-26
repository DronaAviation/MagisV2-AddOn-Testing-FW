/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2026 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Author: Ashish Jaiswal (MechAsh) <AJ>                                      #
 #  Project: MagisV2-AddOn-Testing-FW                                          #
 #  File: \src\addon\addon_common.cpp                                          #
 #  Created Date: Thu, 25th Jun 2026                                           #
 #  Brief: Shared OLED header/footer, voltage formatting, and the scrolling    #
 #         auto-scaled waveform used across the test pages.                    #
 #  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  #
 #  Last Modified: Thu, 25th Jun 2026                                          #
 #  Modified By: AJ                                                            #
 #  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  #
 #  HISTORY:                                                                   #
 #  Date      	By	Comments                                                   #
 #  ----------	---	---------------------------------------------------------  #
*******************************************************************************/
#include "addon_common.h"

/* ============================================================================
 *  Header / footer
 * ============================================================================ */

/**
 * @brief Format battery millivolts as "X.YV" (e.g. 3700 -> "3.7V").
 */
static void formatVoltage ( char *out, uint16_t mv ) {
  uint16_t whole = ( uint16_t ) ( mv / 1000 );
  uint16_t tenth = ( uint16_t ) ( ( mv % 1000 ) / 100 );
  uint8_t  i     = 0;
  if ( whole >= 10 ) out [ i++ ] = ( char ) ( '0' + ( whole / 10 ) % 10 );
  out [ i++ ] = ( char ) ( '0' + ( whole % 10 ) );
  out [ i++ ] = '.';
  out [ i++ ] = ( char ) ( '0' + tenth );
  out [ i++ ] = 'V';
  out [ i ]   = '\0';
}

void drawHeader ( const char *title ) {
  Oled_Text ( 0, 0, title );

  char vbuf [ 8 ];
  formatVoltage ( vbuf, Bms_Get ( Voltage ) );
  int16_t vw = ( int16_t ) ( strlen ( vbuf ) * 6 );    // 6px advance per char
  Oled_Text ( ( int16_t ) ( 128 - vw ), 0, vbuf );     // right-aligned

  Oled_Line ( 0, 10, 127, 10 );                        // divider
}

void drawFooter ( bool selected ) {
  Oled_Line ( 0, 54, 127, 54 );                          // footer divider
  Oled_Text ( 2, 57, selected ? "> Back" : "  Back" );   // ">" marks the selected item
}

/* ============================================================================
 *  Scrolling waveform (shared by Input / Servo / Motor pages)
 * ============================================================================ */

static uint16_t waveBuf [ PLOT_W ];  // ring buffer of raw samples (0..65535)
static uint16_t waveHead  = 0;       // next write index
static uint16_t waveCount = 0;       // valid samples (caps at PLOT_W)

void waveReset ( void ) {
  waveHead  = 0;
  waveCount = 0;
}

void wavePush ( uint16_t sample ) {
  waveBuf [ waveHead ] = sample;
  waveHead             = ( uint16_t ) ( ( waveHead + 1 ) % PLOT_W );
  if ( waveCount < PLOT_W ) waveCount++;
}

/* ============================================================================
 *  Current baseline (board + OLED), measured during the boot splash
 * ============================================================================ */

static uint32_t baselineSum   = 0;    // running sum of valid samples
static uint16_t baselineCount = 0;    // number of valid samples
static uint16_t baselineMa    = 0;    // current average (mA)

void currentBaselineSample ( void ) {
  uint16_t mA = Bms_Get ( Current );
  if ( mA > 32767 ) mA = 32767;       // clamp like the page readouts
  if ( mA == 0 ) return;              // sensor not settled yet — skip
  baselineSum += mA;
  baselineCount++;
  baselineMa = ( uint16_t ) ( baselineSum / baselineCount );
}

uint16_t currentOffset ( void ) {
  return ( uint16_t ) ( baselineMa + CURRENT_OFFSET_MA );
}

void waveDraw ( int16_t plotY, int16_t plotH ) {
  if ( waveCount == 0 ) return;
  uint16_t start = ( uint16_t ) ( ( waveHead + PLOT_W - waveCount ) % PLOT_W );

  // Find the min/max over the visible window for auto-scaling.
  uint16_t lo = 0xFFFF, hi = 0;
  for ( uint16_t i = 0; i < waveCount; i++ ) {
    uint16_t v = waveBuf [ ( start + i ) % PLOT_W ];
    if ( v < lo ) lo = v;
    if ( v > hi ) hi = v;
  }

  // Enforce a minimum span, centred on the current band.
  uint16_t span = ( uint16_t ) ( hi - lo );
  if ( span < WAVE_MIN_SPAN ) {
    uint16_t mid  = ( uint16_t ) ( ( lo + hi ) / 2 );
    uint16_t half = WAVE_MIN_SPAN / 2;
    lo   = ( mid > half ) ? ( uint16_t ) ( mid - half ) : 0;
    hi   = ( uint16_t ) ( lo + WAVE_MIN_SPAN );
    span = WAVE_MIN_SPAN;
  }

  int16_t offset = ( int16_t ) ( PLOT_W - waveCount );    // right-align newest
  int16_t prevX = 0, prevY = 0;
  for ( uint16_t i = 0; i < waveCount; i++ ) {
    uint16_t v = waveBuf [ ( start + i ) % PLOT_W ];
    if ( v < lo ) v = lo;
    else if ( v > hi ) v = hi;
    uint8_t row = ( uint8_t ) ( ( uint32_t ) ( plotH - 1 ) * ( hi - v ) / span );
    int16_t x   = ( int16_t ) ( PLOT_X + offset + ( int16_t ) i );
    int16_t y   = ( int16_t ) ( plotY + row );
    if ( i > 0 )
      Oled_Line ( prevX, prevY, x, y );
    else
      Oled_Pixel ( x, y );
    prevX = x;
    prevY = y;
  }
}
