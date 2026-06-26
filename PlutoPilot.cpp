// Do not remove the include below
#include "PlutoPilot.h"

#include "addon.h"


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
  Addon_Init ( );
}

// The function is called once before plutoLoop when you activate Developer Mode
void onLoopStart ( void ) {
  Addon_OnLoopStart ( );
}

// The loop function is called in an endless loop
void plutoLoop ( void ) {
  Addon_Loop ( );
}

// The function is called once after plutoLoop when you deactivate Developer Mode
void onLoopFinish ( void ) {
  Addon_OnLoopFinish ( );
}

// Custom boot splash, called from the firmware loop in place of OledStartUpPage()
void plutoStartUpPage ( void ) {
  Addon_StartUpPage ( );
}
