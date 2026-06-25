---
name: oled-system-vs-user-rendering
description: OLED has System vs User ownership modes; Oled_Print is unlinkable from C++; boot splash must redraw every frame
metadata:
  type: reference
---

The 128×64 OLED (`io/oled_display.c`, driver `drivers/display_ug2864hsweg01.c`) has two ownership modes: `OLED_MODE_SYSTEM` (firmware telemetry, default at boot) and `OLED_MODE_USER` (set by `Oled_Mode(User)` in `onLoopStart`).

**Rendering paths:**
- **User mode** — public pixel API (`Oled_Clear`/`Oled_Text`/`Oled_Update`, `API/Oled.h`) draws into a framebuffer; `Oled_Update` → `Oled_display_Update` pushes via diff (`i2c_OLED_send_changed_bytes`) but **only when `oledMode == OLED_MODE_USER`** — it's a no-op in System mode. This is what `plutoLoop`'s test UI uses.
- **System mode (boot)** — must use the 8 px character grid `i2c_OLED_set_xy(col,row)` + `i2c_OLED_send_string` (21×8 cells, rows 1–6 usable), or render into your own buffer and push directly with `i2c_OLED_send_changed_bytes`. For sub-8px vertical control use a framebuffer + `Oled_DrawTextColor` (6 px/char, 7 px glyph) and push directly.

**Traps (both hit and verified):**
1. `Oled_Print()` is declared C++-linkage in `API/Oled.h` but **defined in C** (`oled_display.c`) → `undefined reference` when called from any `.cpp`. Don't use it from user code; call the `i2c_OLED_*` grid primitives (they're `extern "C"` in `display_ug2864hsweg01.h`) or push a framebuffer.
2. `i2c_OLED_send_changed_bytes(new, old, size)` diffs against a **persistent shadow** and updates it — so a static image pushes once. If anything else clears the panel afterward, a one-shot push won't repaint. To force a redraw, zero the shadow before each push.
3. `OledStartupPageEnd` is the shared handshake flag: true while a splash owns the screen (suppresses telemetry sections at `oled_display.c:267/282` and user pushes/clears at `:406/:396`). The `devmode` false→true transition (set in `mw.cpp userCode()`) makes `OledDisplaySystemData` call a **one-time full clear** early in boot — so the splash must redraw every frame to survive it.

See [[addon-testing-ui]] for the boot splash (`plutoStartUpPage`) that applies all of this. Build-verify per [[windows-pluto-toolchain-path]].
