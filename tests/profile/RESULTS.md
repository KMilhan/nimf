# Initial hotspot investigation

Measured on 2026-11-08 from `b7e7d59` plus the optional profiler in this change.
This investigation adds instrumentation. It does not optimize input behavior or demonstrate a 100 us end-to-end result.

## Warm composition

All values are microseconds. Each cell is one run with 50 measured first-composition keys,
after ten warmup cycles. Korean uses the initial `g` to `ㅎ` step; Japanese uses `a` to `あ`.
The first cold/profile-startup cycle is excluded. CPU-clock instrumentation was enabled.

| OS | Toolkit | Language | Client filter P50 / P95 | Preedit dispatch self P50 | Hangul compute P50 | Helper wait/read P50 |
|---|---|---|---:|---:|---:|---:|
| ubuntu | gtk3 | ko | 327.17 / 841.66 | 151.81 | 1.78 | — |
| ubuntu | gtk4 | ko | 310.95 / 1817.99 | 150.02 | 1.61 | — |
| ubuntu | gtk3 | ja | 861.39 / 1056.37 | 164.97 | — | 438.12 |
| ubuntu | gtk4 | ja | 855.46 / 1491.98 | 164.88 | — | 414.03 |
| arch | gtk3 | ko | 314.08 / 415.27 | 143.91 | 1.78 | — |
| arch | gtk4 | ko | 304.83 / 413.00 | 150.02 | 1.94 | — |
| arch | gtk3 | ja | 860.31 / 1018.75 | 144.61 | — | 486.94 |
| arch | gtk4 | ja | 872.96 / 1101.84 | 159.38 | — | 470.84 |

`client.filter` includes synchronous IPC and callbacks before the API returns. It is different
from injection-to-model and injection-to-framebuffer. The model can update before filtering finishes.
Preedit dispatch self time includes Nimf state updates and toolkit signal handlers, excluding
instrumented nested IPC calls. It does not isolate GTK/Pango from Nimf bookkeeping.

The Korean `hangul_ic_process` calculation is already far below 100 us. Preedit delivery and
synchronous callback round trips dominate these measured filters. GTK4 also filters each
unhandled Enter and Backspace twice in this workload. Both calls are retained in per-key totals.

Japanese helper wait/read P50 was 414.03–486.94 us, with only 14.36–14.75 us of Nimf-thread CPU time.
That gap includes remote work and scheduling, not just socket overhead. The helper and Mozc server
are not internally instrumented. S-expression parsing added 29.93–30.88 us P50.

## Observer cost

Five alternating rounds of 100,000 minimal scopes used the same host. Initialization and file output
were excluded from the loop. These are measured costs per scope, not corrections to subtract:

| Mode | Median ns per scope | Range ns |
|---|---:|---:|
| compiled_out | 0.28 | 0.27–1.48 |
| runtime_off | 3.01 | 2.87–4.63 |
| wall | 30.61 | 25.52–41.88 |
| wall_cpu | 269.24 | 267.14–275.68 |

The default build has no profiler symbols. The compiled-out row measures the fixture loop alone.
A separate runtime experiment used the same profiling image on Arch/GTK4 with Korean input.
Each mode had three runs with 30 measured cycles and five warmups; order reversed in round two.
The three composition steps are pooled equally within each run. Values below are the median
and range of the three per-run injection-to-model P50 values, in microseconds.

| Recorder | Runtime off | Runtime on | Median difference |
|---|---:|---:|---:|
| Wall + thread CPU | 416 (415–418) | 435 (428–445) | +19 |
| Wall only | 439 (413–759) | 427 (421–617) | -12 |

CPU-clock mode showed a 19 us higher median in this comparison. Wall-only ranges overlap widely;
its negative median difference is not evidence of a speedup or zero observer cost. The shared,
unpinned host showed substantial timing variation. Runtime scheduling/cache effects cannot be
estimated from the minimal-scope calibration alone. Use wall-only mode for primary timings and
CPU mode for diagnosis; retain both raw distributions.

## Validation and evidence

- Profiling builds on Ubuntu 24.04 and Arch passed 7 core and 12 Mozc unit cases each.
- The default Ubuntu build passed the same suites and exported no profiler symbols.
- Eight collector/analyzer tests passed on both images and on the host. Host ASan/UBSan checks passed.
- Four primary runs covered both languages and GTK3/GTK4: 1,600 measured keys and 16 negative controls.
- Twelve runtime observer-cost runs added 1,800 measured keys and 24 negative controls.
- Semantic, stable-pixel, complete-trace, and graceful-shutdown checks passed. No trace overflow occurred.
- Independent Claude review found a window-crossing span could be silently excluded. The analyzer now
  rejects crossings; boundary and duplicate-call regression cases pass. Focused follow-up: APPROVED.

The first Ubuntu/Korean collection passed input checks but exposed the old one-filter-per-key
assumption during GTK4 analysis. Its raw data were retained and accepted only after the corrected
analyzer paired every sequential call. The original failing log remains intact.

Raw spans, sample CSVs, per-step summaries, image IDs, versions, and review diagnostics remain in
the ignored `build/profile-investigation/` directory. `source.sha256` records the final source files.
The small startup preflight is excluded from the tables. The primary profiles have one run per
OS/toolkit/language, so they identify candidate hotspots, not stable cross-platform rankings.

## Next measurements for a 100 us target

1. Separate preedit state copying from GTK/Pango callback CPU work, and count synchronous cursor-location round trips.
2. Profile work and scheduler delay inside the Mozc helper/server before changing the bridge or protocol.
3. Evaluate one confirmed mechanism at a time with the same correctness gates and observer settings.

For a display target, first replace the 500 us framebuffer polling endpoint with a finer validated
presentation measurement. These Xvfb results do not measure a physical keyboard, compositor, or panel.
Nanosecond timestamp units alone do not establish 100 us display accuracy.
