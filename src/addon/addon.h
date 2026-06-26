/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2 — AddOn Testing                                           #
 #  File: src/addon/addon.h                                                   #
 #  Brief: Public entry points for the AddOn Testing application. PlutoPilot.cpp #
 #         forwards its firmware hooks straight to these.                      #
 *******************************************************************************/

#ifndef ADDON_H
#define ADDON_H

/** @brief One-time hardware init (OLED, buttons, shared PWM, ADC). plutoInit. */
void Addon_Init ( void );

/** @brief Switch the OLED to User mode for custom drawing. onLoopStart. */
void Addon_OnLoopStart ( void );

/** @brief Run one UI frame: low-battery guard, buttons, render. plutoLoop. */
void Addon_Loop ( void );

/** @brief Leave outputs/motors safe on Developer-Mode exit. onLoopFinish. */
void Addon_OnLoopFinish ( void );

/** @brief Custom boot splash, called every firmware loop. plutoStartUpPage. */
void Addon_StartUpPage ( void );

#endif    // ADDON_H
