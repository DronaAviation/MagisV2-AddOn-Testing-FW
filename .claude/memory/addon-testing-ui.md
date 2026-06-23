---
name: addon-testing-ui
description: The AddOn Testing button+OLED menu built in PlutoPilot.cpp — hardware wiring and page set
metadata:
  type: project
---

Ongoing work in `PlutoPilot.cpp`: a button-driven OLED menu ("AddOn Testing") with a main menu + 4 test pages (Input, Output, Servo, Motor). Shared `drawHeader`/`drawFooter`, generic page-nav (`pageItemCount`/`enterPage`/`exitPage`/`pageAction`), and a reusable scrolling auto-scaled waveform (`wavePush`/`waveDraw`).

Hardware setup (not all obvious from code):
- **Buttons**: GPIO_3 = OK/Accept, GPIO_4 = Navigate. External **pull-down** resistors → pressed = HIGH. Edge-detect + 200 ms cooldown debounce.
- **Input Test**: analog in on ADC_1; digital HIGH when ADC ≥ 2048.
- **Output Test**: single shared PWM_1 — Digital Out (0%/100%) and Dimmer (0→100→0 ramp), dimmer-priority on the pin.
- **Servo Test**: PWM_1 via `Servo_Write`, sweep 1000↔1700; shows current consumption + graph.
- **Motor Test**: coreless **0720/0820 motors, no props**. Wired on **M7/M8 but displayed as M1/M2** (mapping is `targetName[]`/`targetMotor[]={M7,M8}`). Full-range 1000↔2000 sweep, current graphed. Driven via `Motor_Set` (disarmed path; no `Motor_Init` needed for M5–M8).

Current readout subtracts a 100 mA board+OLED baseline (`CURRENT_OFFSET_MA`). See [[bms-current-units-ambiguous]]. UI built per [[oled-ui-workflow]].
