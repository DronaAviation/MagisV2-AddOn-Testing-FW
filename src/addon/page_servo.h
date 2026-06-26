/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2 — AddOn Testing                                           #
 #  File: src/addon/page_servo.h                                              #
 #  Brief: Servo Test page — one Servo Out sweep + current graph on PWM_1.     #
 *******************************************************************************/

#ifndef ADDON_PAGE_SERVO_H
#define ADDON_PAGE_SERVO_H

#include <stdint.h>

/** @brief Selectable items: Servo Out, Back. */
uint8_t ServoPage_ItemCount ( void );

/** @brief Sweep-step + page redraw cadence, in ms. */
uint16_t ServoPage_FrameMs ( void );

/** @brief Reset page state on entry (park servo + clear waveform). */
void ServoPage_Enter ( void );

/** @brief Stop the sweep and park the servo. Used on exit and by the
 *         low-battery guard. */
void ServoPage_Reset ( void );

/** @brief Render the Servo Test page. @param pageSel highlighted item. */
void ServoPage_Draw ( uint8_t pageSel );

/** @brief Handle OK on a non-Back item. @param sel selected item index. */
void ServoPage_Action ( uint8_t sel );

#endif    // ADDON_PAGE_SERVO_H
