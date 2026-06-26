/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2 — AddOn Testing                                           #
 #  File: src/addon/page_input.h                                              #
 #  Brief: Input Test page — live ADC value, digital level, scrolling wave.    #
 *******************************************************************************/

#ifndef ADDON_PAGE_INPUT_H
#define ADDON_PAGE_INPUT_H

#include <stdint.h>

/** @brief Selectable items on the page (Back only). */
uint8_t InputPage_ItemCount ( void );

/** @brief Page redraw + waveform-sample cadence, in ms. */
uint16_t InputPage_FrameMs ( void );

/** @brief Reset page state on entry (clears the waveform history). */
void InputPage_Enter ( void );

/** @brief Render the Input Test page. */
void InputPage_Draw ( void );

#endif    // ADDON_PAGE_INPUT_H
