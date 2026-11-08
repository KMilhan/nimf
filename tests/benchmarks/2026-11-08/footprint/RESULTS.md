# Memory footprint comparison

Measured on 2026-11-08. No product code was changed for this measurement.

The memory-safety patch did not show a large retained-memory increase in this workload. After 10,000 conversions, the median aggregate PSS changed by less than 1 MiB on each distribution. Run ranges overlap. These results do not establish a memory saving or zero overhead.

## Before and after

Values are MiB. The total includes the Nimf daemon, Mozc helper, and Mozc server. The idle stage precedes the first input, so the lazy Mozc processes are absent.

| OS | Stage | Before PSS | After PSS | Difference | After RSS sum |
|---|---|---:|---:|---:|---:|
| ubuntu | idle | 52.44 | 51.75 | -0.69 | 84.28 |
| ubuntu | after-10000 | 73.20 | 72.86 | -0.35 | 115.92 |
| arch | idle | 48.36 | 48.82 | +0.46 | 62.16 |
| arch | after-10000 | 70.12 | 71.01 | +0.89 | 92.18 |

## Current build after 10,000 conversions

| OS | Component | PSS median | PSS range | RSS median | Anonymous PSS median |
|---|---|---:|---:|---:|---:|
| ubuntu | stack | 72.86 | 72.39–73.56 | 115.92 | 20.90 |
| ubuntu | nimf | 51.00 | 50.98–51.75 | 84.52 | 14.70 |
| ubuntu | helper | 3.72 | 3.67–3.77 | 8.47 | 1.02 |
| ubuntu | mozc | 18.08 | 17.74–18.09 | 22.98 | 5.20 |
| ubuntu | client | 1.64 | 1.61–1.64 | 5.59 | 0.39 |
| arch | stack | 71.01 | 69.52–71.08 | 92.18 | 17.11 |
| arch | nimf | 47.75 | 45.99–48.04 | 62.23 | 11.68 |
| arch | helper | 3.54 | 3.53–3.58 | 6.85 | 0.85 |
| arch | mozc | 19.67 | 19.51–19.99 | 23.10 | 4.59 |
| arch | client | 2.04 | 2.02–2.12 | 6.27 | 0.59 |

The direct libnimf probe client is reported separately and excluded from the stack total. Component medians do not necessarily sum to the median total.

## Repeated input

For the current build, changes from 100 to 10,000 completed conversions were:

| OS | PSS change across three trials, KiB | Anonymous PSS change, KiB |
|---|---|---|
| ubuntu | -113 / +11 / +19 | +0 / +0 / +0 |
| arch | +117 / +117 / +127 | +8 / +12 / +8 |

Retained anonymous PSS grew by at most 12 KiB between 100 and 10,000 conversions in the current build. This is not proof that every workload is leak-free. Allocator caches and library mappings can remain resident after a free or after the client exits. PSS can increase when another process unmaps shared pages, without new allocations in the remaining process.

## Method and scope

- Compared retained pre-safety images against current sources rebuilt on the same image for each OS. Both use `CFLAGS=-g -O2`, `CXXFLAGS=-g -O2`, and empty `LDFLAGS`. No sanitizer runtime is linked.
- Three fresh containers per build and OS, run serially. The second pair reverses the before/after order. Each stage has five `/proc/PID/smaps_rollup` samples 100 ms apart, after a 500 ms settling delay. Tables use the median within each trial, then the median and range across three trial medians.
- Waited four seconds after the daemon socket appeared, including the delayed status icon initialization. Sampled idle, the conversion-candidate stage, 100/1,000/10,000 commits, and client closure.
- One input context repeatedly converts `nihongo` to `日本語`. Every commit checks the exact string. The probe does not accumulate committed text. All 12 accepted runs passed 10,000 commits and clean daemon shutdown, for 120,000 verified conversions.
- Enabled Mozc and the system keyboard in singleton mode. Rime, Anthy, and Hangul were inactive. m17n was excluded from the build. Default Nimf UI services remained enabled.
- Used Xvfb, a private D-Bus session, fresh configuration directories, no container network, and no host display. Xvfb, D-Bus, the sampler, and complete GTK/Qt application footprints are excluded from the total.
- PSS apportions resident shared pages. RSS counts each process's resident pages fully, so summing RSS double-counts shared pages. Anonymous PSS includes heap and other anonymous mappings; it is not an exact live-allocation counter.
- These are settled resident-memory measurements, not transient allocation peaks, an exhaustive vocabulary/cache workload, or a native Wayland desktop measurement. The same phrase touches only part of Mozc's dictionaries.
- Ubuntu reports 24.04.5 LTS, GLib 2.80.0, GTK 3.24.41. Arch reports rolling image 20261101.0.620205, GLib 2.90.1, GTK 3.24.52. The Arch package refresh from the preceding validation reported no pending updates. Its diagnostic remains in the local validation archive.
- Both new builds passed their unit suites. Accepted runtime logs contain no ERROR or CRITICAL messages. They contain the expected absent-Wayland warning under Xvfb. Arch also reports that the Glycin image loader cannot use bubblewrap in this container; this occurs before and after.
- Preflight attempts are excluded. The first lacked Docker `--init` and stalled in Xvfb startup. The second sampler encountered an unreadable diagnostic process in `/proc`; the accepted sampler skips unrelated unreadable entries and still requires every expected component.
- At measurement time, the 99 source files in the preceding safety-validation manifest were unchanged. Publication later added documentation; the final product hashes are in [product-source.sha256](../../../memory/product-source.sha256).

## Image identity

| OS | Build | Image ID |
|---|---|---|
| ubuntu | before | `sha256:915939819e70e714e1be9bc29ca398a172f652d5560df4589c7962b88db0027b` |
| ubuntu | after | `sha256:4580a768dc78cf242bb1668177a46391c6540b14b4bf2fecbebe058f278e30d8` |
| arch | before | `sha256:608eaec1162bb38f1347f5f69552978f3d1e6f58d1bbca5e018f6337fc386643` |
| arch | after | `sha256:e8a92fe89bf264888b204e6d61d2a1c56a0ff353ac0fb3d54599845a1e7ffbf4` |

`raw.tar.gz` contains the twelve before/after runs, raw memory readings,
runtime logs, and OS/library/build metadata. Process environment dumps
are omitted. `recipe/` preserves the measurement programs.

## Reproduction

See [the benchmark archive guide](../README.md) for analysis and rerun commands.
The archived numbers describe the recorded images and source hashes.
Current package mirrors or changed source will produce a new experiment.
