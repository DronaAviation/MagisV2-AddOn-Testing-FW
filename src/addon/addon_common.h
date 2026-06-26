/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2026 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Author: Ashish Jaiswal (MechAsh) <AJ>                                      #
 #  Project: MagisV2-AddOn-Testing-FW                                          #
 #  File: \src\addon\addon_common.h                                            #
 #  Created Date: Thu, 25th Jun 2026                                           #
 #  Brief: Shared OLED helpers, scrolling waveform, and layout constants used  #
 #         by the AddOn Testing menu and its test pages.                       #
 #  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  #
 #  Last Modified: Thu, 25th Jun 2026                                          #
 #  Modified By: AJ                                                            #
 #  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  #
 #  HISTORY:                                                                   #
 #  Date      	By	Comments                                                   #
 #  ----------	---	---------------------------------------------------------  #
*******************************************************************************/


#ifndef ADDON_COMMON_H
#define ADDON_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "API/BMS.h"
#include "API/Oled.h"
#include "API/Peripherals.h"
#include "API/Scheduler-Timer.h"

/* ---- Shared layout / tuning constants ----------------------------------- */

// Scrolling-waveform plot area (no frame). Symmetric 2px side margins; spans
// from just below the value line down to 1px above the footer divider.
#define PLOT_X  2
#define PLOT_Y  27                   // rows 27..52
#define PLOT_W  124                  // samples wide
#define PLOT_H  26                   // 27 + 26 - 1 = 52 (1px gap above footer line @54)

#define WAVE_MIN_SPAN  64            // min vertical range so flat signals aren't amplified
// One sample is pushed per page frame (see the per-page *_FrameMs cadence), so
// the visible window spans PLOT_W * frameMs.

#define CURRENT_OFFSET_MA 0          // optional manual trim added on top of the measured baseline
#define OUT_PWM           PWM_1      // PWM pin shared by the Output + Servo pages

/* ---- Shared UI helpers --------------------------------------------------- */

/**
 * @brief Draw the title bar (page title left, battery voltage right) + divider.
 */
void drawHeader ( const char *title );

/**
 * @brief Standard page footer: a divider line + "Back".
 * @param selected true when the page's cursor is on Back.
 */
void drawFooter ( bool selected );

/* ---- Shared scrolling waveform ------------------------------------------- */

/**
 * @brief Clear the waveform history (called when entering a graphed page).
 */
void waveReset ( void );

/**
 * @brief Push a sample into the waveform ring buffer (any 0..65535 value).
 */
void wavePush ( uint16_t sample );

/**
 * @brief Plot the stored samples as a scrolling, auto-scaled line.
 * @param plotY Top row of the plot area.
 * @param plotH Height of the plot area in pixels.
 */
void waveDraw ( int16_t plotY, int16_t plotH );

/* ---- Current baseline (board + OLED), measured at boot ------------------- */

/**
 * @brief Accumulate one current sample into the boot-time baseline average.
 *        Call repeatedly during the splash, when no servo/motor/PWM load is
 *        active, so the reading reflects only the board + OLED draw. Zero
 *        readings (sensor not yet settled) are ignored.
 */
void currentBaselineSample ( void );

/**
 * @brief Board + OLED baseline current (mA) to subtract from load readings:
 *        the value measured during the splash plus the CURRENT_OFFSET_MA trim.
 *        Returns just the trim until the first valid sample is captured.
 */
uint16_t currentOffset ( void );

#endif    // ADDON_COMMON_H
