---
name: bms-current-units-ambiguous
description: Bms_Get() units — Voltage is mV; Current (mAmpRaw) units are documented inconsistently (mA vs centiamp)
metadata:
  type: reference
---

`Bms_Get(Voltage)` returns **millivolts** (`vBatRaw*100`). `Bms_Get(Current)` returns `mAmpRaw`.

**Gotcha:** `mAmpRaw`'s units are documented inconsistently in `src/main/sensors/battery.cpp` — its declaration comment says *centiampere (0.01 A)* while the sag-calc code (and var name) treat it as *milliamps*. The AddOn Testing UI assumes **mA**.

**How to apply:** before trusting absolute current thresholds (e.g. motor-health limits), sanity-check the on-screen value against a clamp meter / known load. If it reads ~100× off, it's centiamps — rescale the display. Used by [[addon-testing-ui]].
