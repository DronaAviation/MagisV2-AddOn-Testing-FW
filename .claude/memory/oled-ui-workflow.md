---
name: oled-ui-workflow
description: How the user works on the OLED UI — incremental, pixel-precise layout refinement, build-verify each step
metadata:
  type: feedback
---

The user builds the OLED UI iteratively and cares about **exact pixel spacing** (gaps between header/value/graph/footer measured in 1–2 px, "add 1 px more gap", "2 px between line and back"). They refine one element at a time across many small turns rather than specifying a full layout up front.

**Why:** the 128×64 display is tight and they have a specific visual result in mind; they adjust by eye on hardware between turns.

**How to apply:**
- When changing layout, state the resulting pixel rows explicitly (e.g. a small table of element → y-rows) so they can confirm against what they see.
- Make spacing/range/threshold values `#define`s so they're easy to tweak.
- Build-verify every change (see [[windows-pluto-toolchain-path]]) — they expect `ALL BUILDS PASSED` and a flash/RAM number after each edit.
- Expect follow-up refinements; don't over-engineer the first pass. Related: [[addon-testing-ui]].
