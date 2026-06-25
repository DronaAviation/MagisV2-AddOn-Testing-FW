# Memory index

- [AddOn Testing UI](addon-testing-ui.md) — button+OLED menu in PlutoPilot.cpp; 4 test pages + low-battery guard + custom boot splash
- [OLED UI workflow](oled-ui-workflow.md) — user refines layout pixel-by-pixel, build-verify each step
- [OLED System vs User rendering](oled-system-vs-user-rendering.md) — two ownership modes; Oled_Print unlinkable from C++; boot splash must redraw every frame
- [Windows PlutoIDE toolchain path](windows-pluto-toolchain-path.md) — ARM toolchain at C:\PlutoIDE; extension sets PATH only in its own terminal, not system-wide
- [Clang diagnostics are noise](clang-diagnostics-are-noise.md) — ignore IDE include/identifier errors; trust the Makefile build
- [BMS current units ambiguous](bms-current-units-ambiguous.md) — Voltage=mV; Current(mAmpRaw) units unclear (mA vs centiamp)
- [FW development reference docs](fw-development-reference-docs.md) — docs/fw-development-reference/ holds DMA/timer/pin maps + datasheets; consult & keep in sync with code
