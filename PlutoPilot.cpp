// Do not remove the include below
#include "PlutoPilot.h"
#include <string.h>

/* ============================================================================
 *  AddOn Testing — UI state
 * ----------------------------------------------------------------------------
 *  Buttons (external pull-down: pressed = HIGH):
 *    GPIO_3 = OK / Accept   GPIO_4 = Navigate (next item)
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

static screen_e screen      = SCREEN_MENU;    // current screen
static uint8_t  selected    = 0;              // highlighted menu item (0..MENU_COUNT-1)
static uint8_t  activePage  = 0;              // page entered after OK
static uint8_t  pageSel     = 0;              // selected item within the active page

// Button edge-detection state
static bool     prevOk      = false;
static bool     prevNav     = false;
static uint32_t lastActionMs = 0;
#define BTN_COOLDOWN_MS 200

/* ---- Input Test page ---------------------------------------------------- */
#define INPUT_ADC          ADC_1     // analog source (initialised in plutoInit)
#define INPUT_DIGITAL_THR  2048      // ADC >= threshold -> digital HIGH

// Scrolling-waveform plot area (no frame). Symmetric 2px side margins; spans
// from just below the value line down to 1px above the footer divider.
#define PLOT_X  2
#define PLOT_Y  27                   // rows 27..52
#define PLOT_W  124                  // samples wide
#define PLOT_H  26                   // 27 + 26 - 1 = 52 (1px gap above footer line @54)

#define WAVE_SAMPLE_MS 25            // one sample every 25ms -> ~3s window
#define WAVE_MIN_SPAN  64            // min vertical range so flat signals aren't amplified

static uint16_t waveBuf [ PLOT_W ];  // ring buffer of raw ADC samples (0..4095)
static uint16_t waveHead   = 0;      // next write index
static uint16_t waveCount  = 0;      // valid samples (caps at PLOT_W)
static uint32_t lastSampleMs = 0;

/**
 * @brief Clear the waveform history (called when entering the Input page).
 */
static void waveReset ( void ) {
  waveHead     = 0;
  waveCount    = 0;
  lastSampleMs = 0;
}

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

/**
 * @brief Draw the title bar (page title left, battery voltage right) + divider.
 */
static void drawHeader ( const char *title ) {
  Oled_Text ( 0, 0, title );

  char vbuf [ 8 ];
  formatVoltage ( vbuf, Bms_Get ( Voltage ) );
  int16_t vw = ( int16_t ) ( strlen ( vbuf ) * 6 );    // 6px advance per char
  Oled_Text ( ( int16_t ) ( 128 - vw ), 0, vbuf );     // right-aligned

  Oled_Line ( 0, 10, 127, 10 );                        // divider
}

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
      Oled_Rect ( 0, slotTop, 128, slotH );        // full-width white bar fills the slot
      Oled_Text ( 2, textY, menuItems [ i ], true );    // black text on the bar
    } else {
      Oled_Text ( 2, textY, menuItems [ i ] );
    }
  }
}

/**
 * @brief Standard page footer: a divider line + "Back".
 *
 * The leading ">" cursor is shown only when Back is the current selection.
 * OK while Back is selected returns to the menu.
 *
 * @param selected true when the page's cursor is on Back.
 */
static void drawFooter ( bool selected ) {
  Oled_Line ( 0, 54, 127, 54 );                          // footer divider
  Oled_Text ( 2, 57, selected ? "> Back" : "  Back" );   // ">" marks the selected item
}

/**
 * @brief Push a sample into the waveform ring buffer (any 0..65535 value).
 */
static void wavePush ( uint16_t sample ) {
  waveBuf [ waveHead ] = sample;
  waveHead             = ( uint16_t ) ( ( waveHead + 1 ) % PLOT_W );
  if ( waveCount < PLOT_W ) waveCount++;
}

/**
 * @brief Plot the stored samples as a scrolling, auto-scaled line.
 *
 * The vertical axis auto-scales to the min/max of the visible window so small
 * variations fill the box. A minimum span (WAVE_MIN_SPAN) keeps a flat/quiet
 * signal from being amplified into noise. Newest sample is on the right.
 *
 * @param plotY Top row of the plot area.
 * @param plotH Height of the plot area in pixels.
 */
static void waveDraw ( int16_t plotY, int16_t plotH ) {
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

/**
 * @brief Input Test page: live ADC value, derived digital level, and a
 *        scrolling waveform of the analog signal. "> Back" footer.
 */
static void drawInputPage ( void ) {
  uint16_t adc = Peripheral_Read ( INPUT_ADC );    // 0..4095

  // Sample the waveform on a fixed cadence (independent of loop rate).
  uint32_t now = millis ( );
  if ( now - lastSampleMs >= WAVE_SAMPLE_MS ) {
    wavePush ( adc );
    lastSampleMs = now;
  }

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

/**
 * @brief Placeholder for the remaining per-item test pages (built later).
 */
static void drawPagePlaceholder ( uint8_t page ) {
  drawHeader ( menuItems [ page ] );
  Oled_Text ( 2, 26, "Page WIP" );
  drawFooter ( true );
}

/* ============================================================================
 *  Output Test page
 * ----------------------------------------------------------------------------
 *  Both items share one PWM pin (PWM_1):
 *    Digital Out : OK toggles HIGH/LOW  -> PWM 100% / 0%   (default LOW)
 *    Dimmer Out  : OK starts/stops a 0->100->0 PWM ramp    (default 0)
 *  The dimmer ramp takes priority on the pin; otherwise the digital level
 *  drives it. Leaving the page (Back) stops both and restores defaults.
 * ============================================================================ */
#define OUT_PWM     PWM_1      // shared PWM output (digital + dimmer)
#define DIM_STEP_MS 20         // ramp step interval -> ~2s up, ~2s down

// Output-page item indices (Back is always the last item).
enum { OUT_DIGITAL = 0,
       OUT_DIMMER  = 1,
       OUT_BACK    = 2 };

static bool     doutHigh     = false;    // digital-out state (default LOW)
static bool     dimmerActive = false;    // dimmer ramp running?
static uint8_t  dimmerVal    = 0;        // current dimmer duty 0..100
static int8_t   dimmerDir    = 1;        // ramp direction (+1 up, -1 down)
static uint32_t lastDimMs    = 0;        // ramp timing

/**
 * @brief Drive outputs to safe defaults (digital LOW, dimmer 0) and clear run
 *        state. Used on page entry and exit.
 */
static void outputsReset ( void ) {
  doutHigh     = false;
  dimmerActive = false;
  dimmerVal    = 0;
  dimmerDir    = 1;
  lastDimMs    = 0;
  Peripheral_Write ( OUT_PWM, 0 );
}

/**
 * @brief Advance the dimmer ramp (when active) and drive both outputs.
 */
static void outputUpdate ( void ) {
  if ( dimmerActive ) {
    uint32_t now = millis ( );
    if ( now - lastDimMs >= DIM_STEP_MS ) {
      lastDimMs = now;
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
  }
  // Shared PWM pin: dimmer ramp takes priority; else digital level (0/100%).
  uint16_t out = dimmerActive ? dimmerVal : ( doutHigh ? 100 : 0 );
  Peripheral_Write ( OUT_PWM, out );
}

/**
 * @brief Output Test page: a Digital Out toggle and a ramping Dimmer (PWM),
 *        each acted on with OK when the ">" cursor is on it. "> Back" footer.
 */
static void drawOutputPage ( void ) {
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

/* ============================================================================
 *  Servo Test page
 * ----------------------------------------------------------------------------
 *  One item + Back. Servo Out drives PWM_1 via Servo_Write (1000..2000us):
 *    OK on Servo Out : start/stop a 1000->2000->1000 sweep (default 1000)
 *  Leaving the page (Back) stops the sweep and parks at 1000.
 * ============================================================================ */
#define CURRENT_OFFSET_MA 100  // board + OLED baseline draw, subtracted from reading
#define SERVO_STEP_MS 50       // sweep step interval
#define SERVO_STEP    100       // us per step -> ~1.4s each way over 1000..1700
#define SERVO_MIN     1000
#define SERVO_MAX     1600

// Servo-page item indices (Back is the last item).
enum { SRV_SERVO = 0,
       SRV_BACK  = 1 };

static bool     servoActive = false;     // sweep running?
static int16_t  servoVal    = SERVO_MIN; // current pulse 1000..2000
static int8_t   servoDir    = 1;         // sweep direction (+1 up, -1 down)
static uint32_t lastServoMs = 0;         // sweep timing

/**
 * @brief Stop the sweep and park the servo at the default (SERVO_MIN).
 *        Used on page entry and exit.
 */
static void servoReset ( void ) {
  servoActive = false;
  servoVal    = SERVO_MIN;
  servoDir    = 1;
  lastServoMs = 0;
  Servo_Write ( OUT_PWM, SERVO_MIN );
}

/**
 * @brief Advance the servo sweep (when active) and drive the output.
 */
static void servoUpdate ( void ) {
  if ( servoActive ) {
    uint32_t now = millis ( );
    if ( now - lastServoMs >= SERVO_STEP_MS ) {
      lastServoMs = now;
      int16_t v   = ( int16_t ) ( servoVal + servoDir * SERVO_STEP );
      if ( v >= SERVO_MAX ) {
        v        = SERVO_MAX;
        servoDir = -1;
      } else if ( v <= SERVO_MIN ) {
        v        = SERVO_MIN;
        servoDir = 1;
      }
      servoVal = v;
    }
  }
  Servo_Write ( OUT_PWM, ( uint16_t ) servoVal );
}

/**
 * @brief Servo Test page: one Servo Out sweep control + Back footer.
 */
static void drawServoPage ( void ) {
  servoUpdate ( );    // advance sweep + drive hardware

  drawHeader ( "Servo Test" );

  // Servo Out row (value is always 4 digits)
  if ( pageSel == SRV_SERVO ) Oled_Text ( 2, 15, ">" );
  Oled_Text ( 10, 15, "Servo Out" );
  Oled_Number ( 84, 15, servoVal );

  // Current consumption (read-only), offset by the board+OLED baseline draw,
  // right-aligned before "mA"
  uint16_t mA = Bms_Get ( Current );
  if ( mA > 32767 ) mA = 32767;
  mA = ( mA > CURRENT_OFFSET_MA ) ? ( uint16_t ) ( mA - CURRENT_OFFSET_MA ) : 0;
  Oled_Text ( 10, 26, "Current" );
  int16_t d = ( mA >= 10000 ) ? 5 : ( mA >= 1000 ) ? 4 : ( mA >= 100 ) ? 3 : ( mA >= 10 ) ? 2 : 1;
  Oled_Number ( ( int16_t ) ( 108 - d * 6 ), 27, ( int16_t ) mA );
  Oled_Text ( 110, 27, "mA" );

  // Sample the current into the waveform at a fixed cadence, then plot it.
  uint32_t now = millis ( );
  if ( now - lastSampleMs >= WAVE_SAMPLE_MS ) {
    wavePush ( mA );
    lastSampleMs = now;
  }
  waveDraw ( 36, 16 );    // current-vs-time graph, rows 36..51

  drawFooter ( pageSel == SRV_BACK );
}

/* ============================================================================
 *  Motor Test page
 * ----------------------------------------------------------------------------
 *  Motors wired on M7/M8, shown on the display as M1/M2. Pick a target (one
 *  motor or ALL) and run a 1000->MAX->1000 throttle sweep while the current
 *  draw is graphed.
 *    OK on Motor : cycle target M1 -> M2 -> ALL  (stops the test)
 *    OK on Test  : start/stop the throttle sweep on the target
 *  Leaving the page (Back) stops the test and sets the motors to 1000 (off).
 *
 *  SAFETY: MOTOR_TEST_MAX caps the bench throttle (1000..2000). Raise it
 *  deliberately - props will spin.
 * ============================================================================ */
#define MOTOR_TEST_MAX 2000    // max sweep throttle: full range (coreless 0720/0820, no props)
#define MOTOR_STEP_MS  25      // sweep step interval
#define MOTOR_STEP     25       // throttle units per step

// Motor targets and page-item indices.
// Only M7/M8 are wired; they are shown on the display as M1/M2.
enum { MT_M1 = 0, MT_M2, MT_ALL, MT_COUNT };
enum { MOT_TARGET = 0, MOT_TEST = 1, MOT_BACK = 2 };

static const char *const           targetName  [ MT_COUNT ] = { "M1", "M2", "ALL" };
static const bidirectional_motor_e targetMotor [ 2 ]        = { M7, M8 };

static uint8_t  motTarget = MT_M1;     // selected target (default M1 -> M7)
static bool     motActive = false;     // throttle sweep running?
static int16_t  motVal    = 1000;      // current throttle 1000..MOTOR_TEST_MAX
static int8_t   motDir    = 1;         // sweep direction
static uint32_t lastMotMs = 0;         // sweep timing

/**
 * @brief Stop every wired motor (M7/M8 -> 1000).
 */
static void motorsAllOff ( void ) {
  Motor_Set ( M7, 1000 );
  Motor_Set ( M8, 1000 );
}

/**
 * @brief Drive the selected target at @p val; the other motor stays at 1000.
 */
static void applyMotors ( int16_t val ) {
  for ( uint8_t i = 0; i < 2; i++ ) {
    bool on = ( motTarget == MT_ALL ) || ( motTarget == i );
    Motor_Set ( targetMotor [ i ], ( int16_t ) ( on ? val : 1000 ) );
  }
}

/**
 * @brief Stop the sweep and set all motors off. Used on page entry and exit.
 */
static void motorReset ( void ) {
  motActive = false;
  motVal    = 1000;
  motDir    = 1;
  lastMotMs = 0;
  motorsAllOff ( );
}

/**
 * @brief Advance the throttle sweep (when active) and drive the target.
 */
static void motorUpdate ( void ) {
  if ( motActive ) {
    uint32_t now = millis ( );
    if ( now - lastMotMs >= MOTOR_STEP_MS ) {
      lastMotMs = now;
      int16_t v = ( int16_t ) ( motVal + motDir * MOTOR_STEP );
      if ( v >= MOTOR_TEST_MAX ) {
        v      = MOTOR_TEST_MAX;
        motDir = -1;
      } else if ( v <= 1000 ) {
        v      = 1000;
        motDir = 1;
      }
      motVal = v;
    }
  } else {
    motVal = 1000;
  }
  applyMotors ( motVal );
}

/**
 * @brief Motor Test page: target select + throttle-sweep test + current graph.
 */
static void drawMotorPage ( void ) {
  motorUpdate ( );    // advance sweep + drive motors

  drawHeader ( "Motor Test" );

  // Target row
  if ( pageSel == MOT_TARGET ) Oled_Text ( 2, 14, ">" );
  Oled_Text ( 10, 14, "Motor" );
  Oled_Text ( 90, 14, targetName [ motTarget ] );

  // Test row: throttle value fed to the motor (or OFF) + current readout
  if ( pageSel == MOT_TEST ) Oled_Text ( 2, 24, ">" );
  Oled_Text ( 10, 24, "Test" );
  if ( motActive )
    Oled_Number ( 48, 24, motVal );        // live throttle value (1000..2000)
  else
    Oled_Text ( 48, 24, "OFF" );
  uint16_t mA = Bms_Get ( Current );
  if ( mA > 32767 ) mA = 32767;
  mA = ( mA > CURRENT_OFFSET_MA ) ? ( uint16_t ) ( mA - CURRENT_OFFSET_MA ) : 0;
  int16_t d = ( mA >= 10000 ) ? 5 : ( mA >= 1000 ) ? 4 : ( mA >= 100 ) ? 3 : ( mA >= 10 ) ? 2 : 1;
  Oled_Number ( ( int16_t ) ( 108 - d * 6 ), 24, ( int16_t ) mA );
  Oled_Text ( 110, 24, "mA" );

  // Sample current into the waveform, then plot it (rows 34..51)
  uint32_t now = millis ( );
  if ( now - lastSampleMs >= WAVE_SAMPLE_MS ) {
    wavePush ( mA );
    lastSampleMs = now;
  }
  waveDraw ( 34, 18 );

  drawFooter ( pageSel == MOT_BACK );
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
  outputsReset ( );    // digital LOW, dimmer 0, PWM pin -> 0
  servoReset ( );      // sweep off, servo parked at SERVO_MIN
  motorReset ( );      // test off, M7/M8 -> 1000
}

/**
 * @brief Full-screen low-battery warning: two centred lines (6px/char).
 */
static void drawLowBattery ( void ) {
  Oled_Text ( ( int16_t ) ( ( 128 - 11 * 6 ) / 2 ), 24, "Battery Low" );
  Oled_Text ( ( int16_t ) ( ( 128 - 18 * 6 ) / 2 ), 36, "Connect to Charger" );
}

/* ---- Page navigation control -------------------------------------------- */

/**
 * @brief Number of selectable items on a page (the last one is always Back).
 */
static uint8_t pageItemCount ( uint8_t page ) {
  switch ( page ) {
    case 1:  return 3;    // Output: Digital, Dimmer, Back
    case 2:  return 2;    // Servo: Servo Out, Back
    case 3:  return 3;    // Motor: Target, Test, Back
    default: return 1;    // Input / placeholder: Back only
  }
}

/**
 * @brief Per-page entry: select the first item and reset that page's state.
 */
static void enterPage ( uint8_t page ) {
  pageSel = 0;
  switch ( page ) {
    case 0: waveReset ( ); break;                      // Input
    case 1: outputsReset ( ); break;                   // Output
    case 2: servoReset ( ); waveReset ( ); break;      // Servo (+ current graph)
    case 3: motorReset ( ); waveReset ( ); break;      // Motor (+ current graph)
    default: break;
  }
}

/**
 * @brief Per-page exit: stop anything running and restore safe defaults.
 */
static void exitPage ( uint8_t page ) {
  if ( page == 1 ) outputsReset ( );         // stop & default the outputs
  else if ( page == 2 ) servoReset ( );      // stop sweep & park servo
  else if ( page == 3 ) motorReset ( );      // stop test & motors off
}

/**
 * @brief Handle OK on the active page's selected item.
 *
 * The last item is always "Back" (returns to the menu). Other items are
 * page-specific toggles.
 */
static void pageAction ( uint8_t page, uint8_t sel ) {
  if ( sel == pageItemCount ( page ) - 1 ) {    // Back
    exitPage ( page );
    screen = SCREEN_MENU;
    return;
  }
  if ( page == 1 ) {
    if ( sel == OUT_DIGITAL ) {
      doutHigh = ! doutHigh;                     // toggle digital out
    } else if ( sel == OUT_DIMMER ) {
      dimmerActive = ! dimmerActive;             // start/stop ramp
      if ( ! dimmerActive ) {                    // stopping -> back to default 0
        dimmerVal = 0;
        dimmerDir = 1;
      }
    }
  } else if ( page == 2 ) {
    if ( sel == SRV_SERVO ) {
      servoActive = ! servoActive;               // start/stop sweep
      if ( ! servoActive ) {                     // stopping -> park at default 1000
        servoVal = SERVO_MIN;
        servoDir = 1;
      }
    }
  } else if ( page == 3 ) {
    if ( sel == MOT_TARGET ) {
      motorReset ( );                            // switching target stops the test (safety)
      motTarget = ( uint8_t ) ( ( motTarget + 1 ) % MT_COUNT );
    } else if ( sel == MOT_TEST ) {
      motActive = ! motActive;                   // start/stop the throttle sweep
      if ( ! motActive ) motorReset ( );         // stopping -> all motors off
    }
  }
}

/**
 * @brief Dispatch to the active page's renderer.
 */
static void drawPage ( uint8_t page ) {
  switch ( page ) {
    case 0:
      drawInputPage ( );
      break;
    case 1:
      drawOutputPage ( );
      break;
    case 2:
      drawServoPage ( );
      break;
    case 3:
      drawMotorPage ( );
      break;
    default:
      drawPagePlaceholder ( page );
      break;
  }
}

/* ============================================================================
 *  Boot splash — custom startup page for the AddOn Testing firmware
 * ----------------------------------------------------------------------------
 *  Replaces the stock OledStartUpPage() at the top of the firmware loop(). For
 *  the first SPLASH_MS after power-up it shows the AddOn Testing identity and
 *  version, then clears once and hands the display back to normal rendering.
 *
 *  This runs in System mode at boot (before Developer Mode), so it draws via
 *  the 21-col text grid (Oled_Print, rows 1..6 usable) and coordinates with
 *  OledStartupPageEnd — the shared firmware flag that suppresses system
 *  telemetry and user-framebuffer pushes while a splash is on screen. It must
 *  be set true while the splash shows and cleared (with a display clear) once,
 *  exactly as the stock startup page did.
 * ============================================================================ */
const char *const testing_fw_version = "1.0.0";    // AddOn Testing FW version

#define SPLASH_MS 2000    // splash visible for 2 s after boot

// Firmware-internal startup symbols (compiled as C, not exposed through the
// public API headers) referenced directly here — the splash lives in user
// code but hooks the boot sequence. Drawn pixel-precise into a private
// framebuffer (not the 8px text grid) so line spacing is exact; pushed with
// the same diff-send the system renderer uses. The API's Oled_Print() is
// unusable here (declared C++-linkage in Oled.h but defined in C).
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
#define SPL_Y_TITLE1 16    // "Pluto AddOn"
#define SPL_Y_TITLE2 26    // "Testing FW"   (24 natural + 2px extra gap)
#define SPL_Y_VER    40    // "FW : v<version>"
#define SPL_Y_BUILD  52    // "Build : <date>"

/**
 * @brief AddOn Testing boot splash. Call once per firmware loop in place of
 *        OledStartUpPage(); self-times the SPLASH_MS window and clears once.
 */
void plutoStartUpPage ( void ) {
  if ( ! OledInitStatus ) return;        // OLED not up yet — nothing to draw

  static bool     started = false;
  static uint32_t startMs = 0;
  if ( ! started ) {                     // latch the boot instant on first call
    started = true;
    startMs = millis ( );
  }

  if ( millis ( ) - startMs < SPLASH_MS ) {
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

/**
 * Configures Pluto's receiver mode.
 * AUX channel configurations for ELRS:
 * ARM mode      : Rx_AUX1, range 1300 to 2100 (2-pos switch)
 * ANGLE mode    : Rx_AUX2, range 1300 to 2100 (3-pos switch: mid+high = ANGLE, low = ACRO)
 * MAG mode      : Rx_AUX3, range 1500 to 2100
 * DEV mode      : Rx_AUX4, range 1500 to 2100
 * ALT HOLD / THROTTLE mode : Rx_AUX5, range 1500 to 2100
 *                            (2-pos switch: low = THROTTLE mode, high = ALT HOLD mode)
 */
void plutoRxConfig ( void ) {
  // Receiver mode: Uncomment one line matching your setup.
  Receiver_Mode ( Rx_ESP );    // Onboard ESP
  // Receiver_Mode ( Rx_CAM );    // WiFi CAMERA
  // Receiver_Mode ( Rx_PPM );    // PPM based
  // Receiver_Mode ( Rx_ELRS );      // ExpressLRS (CRSF) on USART1
}

// The setup function is called once at Pluto's hardware startup
void plutoInit ( void ) {
  // Add your hardware initialization code here
  Oled_Init ( );
  Peripheral_Init ( GPIO_3, INPUT );          // OK button
  Peripheral_Init ( GPIO_4, INPUT );          // Navigate button
  Peripheral_Init ( OUT_PWM );                // Output Test: shared PWM out (digital + dimmer)
  Peripheral_Init ( ADC_1 );                  // Input Test: analog in
  Peripheral_Write ( OUT_PWM, 0 );
}

// The function is called once before plutoLoop when you activate Developer Mode
void onLoopStart ( void ) {
  // do your one time stuffs here
  Oled_Mode ( User );  // Switch to User mode for custom drawing
}

// The loop function is called in an endless loop
void plutoLoop ( void ) {
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
      selected = ( uint8_t ) ( ( selected + 1 ) % MENU_COUNT );             // cycle menu items
    } else {
      pageSel = ( uint8_t ) ( ( pageSel + 1 ) % pageItemCount ( activePage ) );    // cycle page items
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

  // --- Render current screen ---
  Oled_Clear ( );
  if ( screen == SCREEN_MENU ) {
    drawMenu ( );
  } else {
    drawPage ( activePage );
  }
  Oled_Update ( );
}

// The function is called once after plutoLoop when you deactivate Developer Mode
void onLoopFinish ( void ) {
  // do your cleanup stuffs here
  outputsReset ( );    // ensure outputs are left safe (digital LOW, dimmer 0)
  motorReset ( );      // ensure all motors are stopped (M5..M8 -> 1000)
}