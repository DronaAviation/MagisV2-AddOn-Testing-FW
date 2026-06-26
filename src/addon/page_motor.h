/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2 — AddOn Testing                                           #
 #  File: src/addon/page_motor.h                                              #
 #  Brief: Motor Test page — target select + throttle sweep + current graph.   #
 *******************************************************************************/

#ifndef ADDON_PAGE_MOTOR_H
#define ADDON_PAGE_MOTOR_H

#include <stdint.h>

/** @brief Selectable items: Target, Test, Back. */
uint8_t MotorPage_ItemCount ( void );

/** @brief Sweep-step + page redraw cadence, in ms. */
uint16_t MotorPage_FrameMs ( void );

/** @brief Reset page state on entry (motors off + clear waveform). */
void MotorPage_Enter ( void );

/** @brief Stop the sweep and set all motors off. Used on exit and by the
 *         low-battery guard. */
void MotorPage_Reset ( void );

/** @brief Render the Motor Test page. @param pageSel highlighted item. */
void MotorPage_Draw ( uint8_t pageSel );

/** @brief Handle OK on a non-Back item. @param sel selected item index. */
void MotorPage_Action ( uint8_t sel );

#endif    // ADDON_PAGE_MOTOR_H
