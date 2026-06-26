/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2 — AddOn Testing                                           #
 #  File: src/addon/page_input.cpp                                            #
 #  Brief: Input Test page — live ADC value, derived digital level, and a      #
 #         scrolling waveform of the analog signal. "> Back" footer.           #
 *******************************************************************************/

#include "page_input.h"

#include "addon_common.h"

#define INPUT_ADC         ADC_1     // analog source (initialised in Addon_Init)
#define INPUT_DIGITAL_THR 2048      // ADC >= threshold -> digital HIGH
#define INPUT_FRAME_MS    20        // redraw + waveform-sample cadence

uint8_t InputPage_ItemCount ( void ) {
  return 1;    // Back only
}

uint16_t InputPage_FrameMs ( void ) {
  return INPUT_FRAME_MS;
}

void InputPage_Enter ( void ) {
  waveReset ( );
}

void InputPage_Draw ( void ) {
  uint16_t adc = Peripheral_Read ( INPUT_ADC );    // 0..4095

  // One sample per frame (the page is drawn at INPUT_FRAME_MS).
  wavePush ( adc );

  drawHeader ( "Input Test" );

  // Value line:  Input: <0-4095> | <HIGH/LOW>
  Oled_Text ( 2, 15, "Input :" );
  Oled_Number ( 52, 15, ( int16_t ) adc );
  Oled_Text ( 82, 15, "|" );
  Oled_Text ( 95, 15, ( adc >= INPUT_DIGITAL_THR ) ? "HIGH" : "LOW" );

  // Waveform graph (frameless)
  waveDraw ( PLOT_Y, PLOT_H );

  drawFooter ( true );    // only item on this page
}
