/*******************************************************************************
 #  SPDX-License-Identifier: GPL-3.0-or-later                                  #
 #  SPDX-FileCopyrightText: 2025 Drona Aviation                                #
 #  -------------------------------------------------------------------------  #
 #  Project: MagisV2-AddOn-Testing-FW                                          #
 #  File: src/addon/addon.cpp                                                 #
 #  Brief: AddOn Testing main page (menu) + navigation, the low-battery safety  #
 #         guard, the boot splash, and the entry points the firmware hooks call.#
 #                                                                             #
 #  Buttons (external pull-down: pressed = HIGH):                              #
 #    GPIO_3 = OK / Accept   GPIO_4 = Navigate (next item)                     #
 *******************************************************************************/

#include "addon.h"

#include "addon_common.h"
#include "page_input.h"
#include "page_output.h"
#include "page_servo.h"
#include "page_motor.h"

// Declared in API/FC-Config.h; forward-declared here to avoid that header's
// transitive dependencies (axis_e, …). Defined in FC-Config-PID.cpp (C++).
void setUserLoopFrequency ( float frequency );

// User-loop period in ms. The default firmware rate is 100 ms (10 Hz). A fast
// loop keeps button polling snappy; see Addon_OnLoopStart for the
// setUserLoopFrequency() unit caveat.
#define USER_LOOP_MS 10

// UI frame cadence in ms. The render, the sweep steps, and the graph samples
// are all throttled to one frame so they advance together — one frame = one
// step = one sample = one OLED redraw (synced). Each page supplies its own
// cadence (its *_FrameMs, the page's step interval); the menu uses
// MENU_FRAME_MS. Buttons still poll every USER_LOOP_MS. Keep cadences a
// multiple of USER_LOOP_MS so frames land on exact loop ticks.
#define MENU_FRAME_MS 50

/* ============================================================================
 *  Menu (main page) + navigation state
 * ============================================================================ */

// Screens: the main menu, or one of the per-item test pages.
typedef enum {
  SCREEN_MENU,
  SCREEN_PAGE
} screen_e;

#define MENU_COUNT 4

static const char *const menuItems [ MENU_COUNT ] = {
    "1. Input Test",
    "2. Output Test",
    "3. Servo Test",
    "4. Motor Test"};

static screen_e screen       = SCREEN_MENU;    // current screen
static uint8_t  selected     = 0;              // highlighted menu item (0..MENU_COUNT-1)
static uint8_t  activePage   = 0;              // page entered after OK
static uint8_t  pageSel      = 0;              // selected item within the active page

// Button edge-detection state
static bool     prevOk       = false;
static bool     prevNav      = false;
static uint32_t lastActionMs = 0;
#define BTN_COOLDOWN_MS 200

/**
 * @brief Render the main menu with the selected row highlighted (inverted).
 */
static void drawMenu ( void ) {
  drawHeader ( "AddOn Testing" );

  // Evenly distribute the items across the area below the divider (y=11) down
  // to the bottom of the 128x64 display. Remainder pixels are spread so the
  // slots tile edge-to-edge and fill to y=63.
  const int16_t regionTop = 11;                  // first row sits just under the divider (line at y=10)
  const int16_t regionH   = 64 - regionTop;      // usable height below the divider

  for ( uint8_t i = 0; i < MENU_COUNT; i++ ) {
    int16_t slotTop = ( int16_t ) ( regionTop + ( i * regionH ) / MENU_COUNT );
    int16_t slotEnd = ( int16_t ) ( regionTop + ( ( i + 1 ) * regionH ) / MENU_COUNT );
    int16_t slotH   = ( int16_t ) ( slotEnd - slotTop );
    int16_t textY   = ( int16_t ) ( slotTop + ( slotH - 7 ) / 2 );    // 7px glyph, vertically centred

    if ( i == selected ) {
      Oled_Rect ( 0, slotTop, 128, slotH );             // full-width white bar fills the slot
      Oled_Text ( 2, textY, menuItems [ i ], true );    // black text on the bar
    } else {
      Oled_Text ( 2, textY, menuItems [ i ] );
    }
  }
}

/* ============================================================================
 *  Page navigation control
 * ============================================================================ */

/**
 * @brief Number of selectable items on a page (the last one is always Back).
 */
static uint8_t pageItemCount ( uint8_t page ) {
  switch ( page ) {
    case 0:  return InputPage_ItemCount ( );
    case 1:  return OutputPage_ItemCount ( );
    case 2:  return ServoPage_ItemCount ( );
    case 3:  return MotorPage_ItemCount ( );
    default: return 1;
  }
}

/**
 * @brief Frame cadence (ms) for the active screen: each page's own step
 *        interval so its redraw, sweep, and graph stay in sync; MENU_FRAME_MS
 *        on the menu.
 */
static uint16_t frameMs ( void ) {
  if ( screen == SCREEN_MENU ) return MENU_FRAME_MS;
  switch ( activePage ) {
    case 0:  return InputPage_FrameMs ( );
    case 1:  return OutputPage_FrameMs ( );
    case 2:  return ServoPage_FrameMs ( );
    case 3:  return MotorPage_FrameMs ( );
    default: return MENU_FRAME_MS;
  }
}

/**
 * @brief Per-page entry: select the first item and reset that page's state.
 */
static void enterPage ( uint8_t page ) {
  pageSel = 0;
  switch ( page ) {
    case 0: InputPage_Enter ( ); break;       // Input
    case 1: OutputPage_Reset ( ); break;      // Output (enter == safe reset)
    case 2: ServoPage_Enter ( ); break;       // Servo (+ current graph)
    case 3: MotorPage_Enter ( ); break;       // Motor (+ current graph)
    default: break;
  }
}

/**
 * @brief Per-page exit: stop anything running and restore safe defaults.
 */
static void exitPage ( uint8_t page ) {
  if ( page == 1 ) OutputPage_Reset ( );        // stop & default the outputs
  else if ( page == 2 ) ServoPage_Reset ( );    // stop sweep & park servo
  else if ( page == 3 ) MotorPage_Reset ( );    // stop test & motors off
}

/**
 * @brief Handle OK on the active page's selected item.
 *
 * The last item is always "Back" (returns to the menu). Other items are
 * delegated to the page module.
 */
static void pageAction ( uint8_t page, uint8_t sel ) {
  if ( sel == pageItemCount ( page ) - 1 ) {    // Back
    exitPage ( page );
    screen = SCREEN_MENU;
    return;
  }
  switch ( page ) {
    case 1: OutputPage_Action ( sel ); break;
    case 2: ServoPage_Action ( sel ); break;
    case 3: MotorPage_Action ( sel ); break;
    default: break;    // Input has no non-Back items
  }
}

/**
 * @brief Dispatch to the active page's renderer.
 */
static void drawPage ( uint8_t page ) {
  switch ( page ) {
    case 0: InputPage_Draw ( ); break;
    case 1: OutputPage_Draw ( pageSel ); break;
    case 2: ServoPage_Draw ( pageSel ); break;
    case 3: MotorPage_Draw ( pageSel ); break;
    default:
      drawHeader ( menuItems [ page ] );
      Oled_Text ( 2, 26, "Page WIP" );
      drawFooter ( true );
      break;
  }
}

/* ============================================================================
 *  Low-battery safety guard
 * ----------------------------------------------------------------------------
 *  Below LOW_BATT_MV the add-on shuts every output down (digital/dimmer PWM,
 *  servo, motors) and shows a full-screen "Battery Low / Connect to Charger"
 *  warning, overriding whatever menu or test page was active. A hysteresis
 *  band (recover only above LOW_BATT_CLEAR_MV) keeps the warning from
 *  flickering when the pack hovers at the threshold or sags briefly on load.
 * ============================================================================ */
#define LOW_BATT_MV       3200    // trip: warn & disable below 3.2 V
#define LOW_BATT_CLEAR_MV 3300    // recover only once back above 3.3 V

static bool lowBattery = false;    // latched guard state

/**
 * @brief Refresh the latched low-battery state from the pack voltage.
 *        Latches on below LOW_BATT_MV; clears only above LOW_BATT_CLEAR_MV.
 */
static void updateLowBattery ( void ) {
  uint16_t mv = Bms_Get ( Voltage );
  if ( mv < LOW_BATT_MV )
    lowBattery = true;
  else if ( mv >= LOW_BATT_CLEAR_MV )
    lowBattery = false;
}

/**
 * @brief Force every output to its safe default: outputs off, servo parked,
 *        motors stopped. Idempotent; called each loop while the guard is on.
 */
static void disableAllOutputs ( void ) {
  OutputPage_Reset ( );    // digital LOW, dimmer 0, PWM pin -> 0
  ServoPage_Reset ( );     // sweep off, servo parked at SERVO_MIN
  MotorPage_Reset ( );     // test off, M7/M8 -> 1000
}

/**
 * @brief Full-screen low-battery warning: two centred lines (6px/char).
 */
static void drawLowBattery ( void ) {
  Oled_Text ( ( int16_t ) ( ( 128 - 11 * 6 ) / 2 ), 24, "Battery Low" );
  Oled_Text ( ( int16_t ) ( ( 128 - 18 * 6 ) / 2 ), 36, "Connect to Charger" );
}

/* ============================================================================
 *  Boot splash — custom startup page for the AddOn Testing firmware
 * ----------------------------------------------------------------------------
 *  Runs in System mode at boot (before Developer Mode), so it draws pixel-
 *  precise into a private framebuffer (not the 8px text grid) and pushes with
 *  the same diff-send the system renderer uses. It coordinates with
 *  OledStartupPageEnd — the shared firmware flag that suppresses telemetry and
 *  user-framebuffer pushes while a splash is on screen. The API's Oled_Print()
 *  is unusable here (declared C++-linkage in Oled.h but defined in C).
 * ============================================================================ */
const char *const testing_fw_version = "1.0.0";    // AddOn Testing FW version

#define SPLASH_MS        2000    // splash visible for 2 s after boot
#define SPLASH_SETTLE_MS 500     // skip this much at the start before sampling baseline current

// Firmware-internal startup symbols (compiled as C, not exposed through the
// public API headers) referenced directly here.
extern "C" {
  extern bool        OledStartupPageEnd;    // true while a splash owns the display
  extern bool        OledInitStatus;        // true once the OLED I2C init completed
  extern const char *const buildDate;       // "MMM DD YYYY" build stamp (version.c)
  void Oled_display_Clear ( void );         // hardware clear (no-op while armed)
  void Oled_DrawTextColor ( uint8_t *screen, int16_t x, int16_t y, const char *text, bool on );
  void i2c_OLED_send_changed_bytes ( uint8_t *newBuf, uint8_t *oldBuf, int size );
}

// Splash layout (pixel y, 6px/char advance, 7px-tall glyphs). "Testing FW"
// sits 2px lower than its natural 8px line pitch below "Pluto AddOn".
#define SPL_Y_TITLE1 8    // "Pluto AddOn"
#define SPL_Y_TITLE2 20    // "Testing FW"   (24 natural + 2px extra gap)
#define SPL_Y_VER    35    // "FW : v<version>"
#define SPL_Y_BUILD  50    // "Build : <date>"

/* ============================================================================
 *  Public entry points (called from PlutoPilot.cpp hooks)
 * ============================================================================ */

void Addon_Init ( void ) {
  Oled_Init ( );
  Peripheral_Init ( GPIO_3, INPUT );          // OK button
  Peripheral_Init ( GPIO_4, INPUT );          // Navigate button
  Peripheral_Init ( OUT_PWM );                // shared PWM out (Output + Servo)
  Peripheral_Init ( ADC_1 );                  // Input Test: analog in
  Peripheral_Write ( OUT_PWM, 0 );
}

void Addon_OnLoopStart ( void ) {
  // Run the user loop at USER_LOOP_MS for snappy button polling; the render and
  // page sweeps are throttled separately to FRAME_MS (see Addon_Loop).
  // NOTE: setUserLoopFrequency() is mislabeled — despite the "Hz" name and doc,
  // the argument is the loop PERIOD in milliseconds (it stores arg * 1000 µs),
  // so USER_LOOP_MS = 10 -> a 10 ms / 100 Hz loop (default was 100 ms / 10 Hz).
  setUserLoopFrequency ( USER_LOOP_MS );

  Oled_Mode ( User );    // Switch to User mode for custom drawing
}

void Addon_Loop ( void ) {
  // --- Low-battery safety guard (overrides everything) ---
  // Below 3.2 V: disable all outputs and show the warning, no matter which
  // menu or test page the user is on. Latched with hysteresis (see above).
  updateLowBattery ( );
  if ( lowBattery ) {
    disableAllOutputs ( );        // kill motors/servo/PWM regardless of page
    screen   = SCREEN_MENU;       // drop back to a safe screen on recovery
    selected = 0;

    Oled_Clear ( );
    drawLowBattery ( );
    Oled_Update ( );

    // Keep edge-detect state fresh so a held button can't fire on recovery.
    prevOk  = Peripheral_Read ( GPIO_3 );
    prevNav = Peripheral_Read ( GPIO_4 );
    return;
  }

  // --- Read buttons (rising-edge detection + shared cooldown debounce) ---
  bool     okNow  = Peripheral_Read ( GPIO_3 );    // OK / Accept
  bool     navNow = Peripheral_Read ( GPIO_4 );    // Navigate
  uint32_t now    = millis ( );
  bool     ready  = ( now - lastActionMs ) > BTN_COOLDOWN_MS;

  if ( navNow && ! prevNav && ready ) {
    if ( screen == SCREEN_MENU ) {
      selected = ( uint8_t ) ( ( selected + 1 ) % MENU_COUNT );                     // cycle menu items
    } else {
      pageSel = ( uint8_t ) ( ( pageSel + 1 ) % pageItemCount ( activePage ) );     // cycle page items
    }
    lastActionMs = now;
  }

  if ( okNow && ! prevOk && ready ) {
    if ( screen == SCREEN_MENU ) {
      activePage = selected;          // enter the selected item's page
      screen     = SCREEN_PAGE;
      enterPage ( activePage );       // reset page state on entry
    } else {
      pageAction ( activePage, pageSel );    // act on the selected page item (Back returns)
    }
    lastActionMs = now;
  }

  prevNav = navNow;
  prevOk  = okNow;

  // --- Render current screen (throttled to the active screen's frame cadence) ---
  // The page draw also advances the sweep step and graph sample, so gating the
  // render here keeps the redraw, the motion, and the graph in lockstep.
  static uint32_t lastFrameMs = 0;
  if ( now - lastFrameMs >= frameMs ( ) ) {
    lastFrameMs = now;
    Oled_Clear ( );
    if ( screen == SCREEN_MENU ) {
      drawMenu ( );
    } else {
      drawPage ( activePage );
    }
    Oled_Update ( );
  }
}

void Addon_OnLoopFinish ( void ) {
  OutputPage_Reset ( );    // ensure outputs are left safe (digital LOW, dimmer 0)
  MotorPage_Reset ( );     // ensure all motors are stopped (M7/M8 -> 1000)
}

void Addon_StartUpPage ( void ) {
  if ( ! OledInitStatus ) return;        // OLED not up yet — nothing to draw

  static bool     started = false;
  static uint32_t startMs = 0;
  if ( ! started ) {                     // latch the boot instant on first call
    started = true;
    startMs = millis ( );
  }

  uint32_t now = millis ( );
  if ( now - startMs < SPLASH_MS ) {
    // Measure the board + OLED baseline current draw while the splash is up —
    // nothing else loads the supply yet (no servo/motor/PWM). Skip the first
    // SPLASH_SETTLE_MS so boot inrush and sensor warm-up don't skew it.
    if ( now - startMs >= SPLASH_SETTLE_MS ) currentBaselineSample ( );

    char ver [ 24 ];
    strcpy ( ver, "FW : v" );
    strcat ( ver, testing_fw_version );

    char build [ 28 ];                   // "Build : " + "MMM DD YYYY"
    strcpy ( build, "Build : " );
    strcat ( build, buildDate );

    // Render the static splash into a private framebuffer, centred (6px/char),
    // and re-push every frame — exactly like the stock startup page redrew its
    // text each loop. The system renderer wipes the screen once early in boot
    // (dev-mode false->true transition), so a one-shot push would vanish after
    // a frame; zeroing the shadow each call forces a full redraw so the splash
    // is restored on the very next pass and stays up for the whole window.
    static uint8_t splashBuf [ 1024 ];
    static uint8_t splashShadow [ 1024 ];
    memset ( splashBuf, 0, sizeof ( splashBuf ) );
    memset ( splashShadow, 0, sizeof ( splashShadow ) );    // defeat diff cache -> full redraw
    Oled_DrawTextColor ( splashBuf, ( int16_t ) ( ( 128 - 11 * 6 ) / 2 ), SPL_Y_TITLE1, "Pluto AddOn", true );
    Oled_DrawTextColor ( splashBuf, ( int16_t ) ( ( 128 - 10 * 6 ) / 2 ), SPL_Y_TITLE2, "Testing FW", true );
    Oled_DrawTextColor ( splashBuf, ( int16_t ) ( ( 128 - ( int16_t ) strlen ( ver ) * 6 ) / 2 ), SPL_Y_VER, ver, true );
    Oled_DrawTextColor ( splashBuf, ( int16_t ) ( ( 128 - ( int16_t ) strlen ( build ) * 6 ) / 2 ), SPL_Y_BUILD, build, true );
    i2c_OLED_send_changed_bytes ( splashBuf, splashShadow, sizeof ( splashBuf ) );

    OledStartupPageEnd = true;             // hold off telemetry/user drawing
  } else if ( OledStartupPageEnd ) {       // window elapsed -> end splash once
    OledStartupPageEnd = false;
    Oled_display_Clear ( );                // wipe before normal rendering takes over
  }
}
