/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2 — AddOn Testing                                           #
 #  File: src/addon/page_output.h                                             #
 #  Brief: Output Test page — Digital Out toggle + ramping Dimmer on PWM_1.    #
 *******************************************************************************/

#ifndef ADDON_PAGE_OUTPUT_H
#define ADDON_PAGE_OUTPUT_H

#include <stdint.h>

/** @brief Selectable items: Digital, Dimmer, Back. */
uint8_t OutputPage_ItemCount ( void );

/** @brief Dimmer-step + page redraw cadence, in ms. */
uint16_t OutputPage_FrameMs ( void );

/** @brief Drive outputs to safe defaults (digital LOW, dimmer 0) and clear run
 *         state. Used on page entry/exit and by the low-battery guard. */
void OutputPage_Reset ( void );

/** @brief Render the Output Test page. @param pageSel highlighted item. */
void OutputPage_Draw ( uint8_t pageSel );

/** @brief Handle OK on a non-Back item. @param sel selected item index. */
void OutputPage_Action ( uint8_t sel );

#endif    // ADDON_PAGE_OUTPUT_H
