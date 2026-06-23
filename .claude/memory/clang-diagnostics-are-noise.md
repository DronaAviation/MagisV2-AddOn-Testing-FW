---
name: clang-diagnostics-are-noise
description: IDE clang/IntelliSense diagnostics in this repo are false — trust the Makefile build instead
metadata:
  type: feedback
---

The IDE's clang diagnostics for this firmware repo are **not reliable** — they fire `'common/axis.h' file not found`, `'API/Oled.h' file not found`, "undeclared identifier", etc., because the clang config lacks the Makefile's include paths and `-D` defines.

**Why:** the real build is the `arm-none-eabi` GCC Makefile build, which has the right include dirs; clang IntelliSense doesn't.

**How to apply:** ignore `pp_file_not_found` / undeclared-identifier diagnostics on PlutoPilot.cpp and src/main files. Verify changes with the Makefile build instead — see [[windows-pluto-toolchain-path]]. Only treat a diagnostic as real if the GCC build also reports it. (Also now documented in the repo's CLAUDE.md.)
