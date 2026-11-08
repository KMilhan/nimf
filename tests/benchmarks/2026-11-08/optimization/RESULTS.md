# Nimf -O2 versus -O3

Measured on 2026-11-08. No project source or default build flags were changed.

The comparison did not show a consistent input-to-framebuffer improvement across Ubuntu/Arch and GTK3/GTK4. Arch GTK3 had lower median and tail times with O3 in these runs. Ubuntu and Arch GTK4 generally moved in the opposite direction. The gains are small, trial ranges overlap for the primary preedit medians, and the host was shared. These data do not establish an overall benefit from changing the default optimization level.

## Composition display

All values are milliseconds. Each value is the median of three per-run percentiles; parentheses show their minimum and maximum. Each run contributes 100 measured events per step. Screen times are the Xvfb pixel-observation upper bound, not physical display latency.

| OS | Toolkit | O2 screen p50 | O3 screen p50 | Delta p50 | O2 screen p95 | O3 screen p95 |
|---|---|---:|---:|---:|---:|---:|
| ubuntu | gtk3 | 1.492 (1.476–1.510) | 1.504 (1.475–1.540) | +0.012 | 2.018 (1.723–2.167) | 2.163 (1.954–2.365) |
| ubuntu | gtk4 | 1.493 (1.487–1.494) | 1.501 (1.486–1.534) | +0.008 | 2.239 (1.902–2.268) | 2.346 (1.698–2.380) |
| arch | gtk3 | 1.600 (1.550–1.736) | 1.539 (1.473–1.684) | -0.061 | 2.551 (2.363–3.106) | 2.267 (2.090–2.528) |
| arch | gtk4 | 1.525 (1.508–1.683) | 1.585 (1.511–1.906) | +0.060 | 2.312 (2.209–2.943) | 2.437 (2.430–2.558) |

See [all steps](tables.md) for commit and erase results. The raw samples retain the paired lower/upper observation interval, signal timestamps, first inputs, and warmups.

## Processing before rendering

This interval starts at the probe key handler and ends at the matching preedit/buffer notification. It includes toolkit processing, Nimf IPC, and Mozc work; it is not a pure Nimf CPU timer.

| OS | Toolkit | O2 processing p50 | O3 processing p50 | Delta microseconds |
|---|---|---:|---:|---:|
| ubuntu | gtk3 | 0.573 (0.568–0.592) | 0.581 (0.571–0.597) | +8 |
| ubuntu | gtk4 | 0.599 (0.584–0.600) | 0.604 (0.586–0.627) | +5 |
| arch | gtk3 | 0.689 (0.687–0.715) | 0.676 (0.629–0.719) | -13 |
| arch | gtk4 | 0.841 (0.819–0.879) | 0.862 (0.829–0.863) | +21 |

Per-run median observation-bound widths were 0.725–0.797 ms across the measured steps. The CPU was not pinned. One-minute host load averages sampled at run boundaries ranged from 6.89 to 12.46. Counterbalanced order and repeated runs reduce order bias but do not remove scheduling, thermal, or host-load effects. Small differences should not be treated as a demonstrated universal speedup or regression.

## Code size

These are ELF `.text` bytes from GNU `size -A`. They are machine-code size, not executable file size or runtime RSS/PSS.

| OS | Component | O2 bytes | O3 bytes | Difference |
|---|---|---:|---:|---:|
| ubuntu | `bin/nimf/.libs/nimf` | 2,235 | 2,235 | +0 |
| ubuntu | `libnimf/.libs/libnimf.so` | 37,167 | 38,991 | +1,824 |
| ubuntu | `modules/engines/nimf-mozc/.libs/libnimf-mozc.so` | 8,037 | 9,301 | +1,264 |
| ubuntu | `modules/clients/gtk/.libs/im-nimf-gtk3.so` | 4,889 | 5,081 | +192 |
| ubuntu | `modules/clients/gtk/.libs/libim-nimf-gtk4.so` | 6,509 | 6,973 | +464 |
| ubuntu | selected total | 58,837 | 62,581 | +3,744 (+6.4%) |
| arch | `bin/nimf/.libs/nimf` | 2,507 | 2,507 | +0 |
| arch | `libnimf/.libs/libnimf.so` | 38,294 | 41,830 | +3,536 |
| arch | `modules/engines/nimf-mozc/.libs/libnimf-mozc.so` | 8,355 | 10,659 | +2,304 |
| arch | `modules/clients/gtk/.libs/im-nimf-gtk3.so` | 4,772 | 5,012 | +240 |
| arch | `modules/clients/gtk/.libs/libim-nimf-gtk4.so` | 6,453 | 6,917 | +464 |
| arch | selected total | 60,381 | 66,925 | +6,544 (+10.8%) |

The selected total covers the daemon, libnimf, the Nimf Mozc adapter, and GTK3/GTK4 modules. It excludes other Nimf modules. No O3 runtime memory-footprint comparison was performed.

## First key

Each value summarizes the three first composition inputs with fresh profiles. It includes lazy Mozc helper/server startup. Filesystem caches were not flushed. These samples are excluded from warm percentiles.

| OS | Toolkit | O2 screen upper median (range), ms | O3 screen upper median (range), ms |
|---|---|---:|---:|
| ubuntu | gtk3 | 211.780 (210.819–412.293) | 241.763 (211.773–564.666) |
| ubuntu | gtk4 | 211.369 (210.312–211.507) | 211.132 (210.767–213.303) |
| arch | gtk3 | 209.825 (208.225–233.542) | 210.944 (209.574–410.003) |
| arch | gtk4 | 209.003 (208.406–210.635) | 210.048 (209.178–236.825) |

## Controlled comparison and validation

- O2 used the previously validated current-source latency image. Each O3 image was derived from that exact image, cleaned, and rebuilt with `CFLAGS=-g -O3` and `CXXFLAGS=-g -O3`; `LDFLAGS` stayed empty. No LTO, architecture flags, or fast-math flags were added.
- GTK, Mozc, OS packages, and the benchmark observer were held fixed within each OS. SHA-256 hashes confirm identical Mozc helper/server and latency driver/GTK probe executables. Only Nimf and its modules changed optimization level. The separate test helper programs rebuilt by the existing build script are not used by this latency benchmark.
- Both O3 builds passed 7 core memory tests and 12 Mozc unit tests. O2 unit logs were retained from the validated base images. The accepted runtime series passed all 12 containers, 7,200 measured keys, 48 negative controls, and CSV timestamp-order/count checks.
- Each run used 100 measured cycles after one first-input cycle and 10 warmup cycles. Each cycle contains preedit, commit, and erase. Poll sleep was 500 microseconds. Runtime containers were serial, network-disabled, and isolated from host display/settings.
- Three paired rounds were run per OS: O2/O3, O3/O2, then O2/O3. OS order was also reversed in the middle round. Results are aggregates of per-run quantiles, not a pooled percentile across trials.
- GTK4 used Cairo. These results do not cover native Wayland, physical display scanout, Qt client latency, arbitrary vocabulary, or a saturated typing stream.
- Source hashes still match the original O2 source manifest. Accepted runs used one immutable image identity per OS/variant.
- Preliminary top-level `trial*` data were excluded because provenance inspection overlapped timing startup. The first series was stopped, and the complete accepted series started after inspection finished. No accepted trial was discarded.

## Image identity

| OS | Optimization | Image ID |
|---|---|---|
| arch | o2 | `sha256:33feb6235b6a92dde0d08ae4f0c0702035c72b5dd4072d17e12895552151b3be` |
| arch | o3 | `sha256:5dd128ad7910915605e60570f62a87ca215c9cf7674bcb65f568f3e3d8d97f64` |
| ubuntu | o2 | `sha256:5719ae9a4a34123c24911e8322cd9adbd42d37b3d11aaeb02e5d9d152eaadb1b` |
| ubuntu | o3 | `sha256:0f30f05a2860e72d7a28b730f364fd87ac4d1e0cfcbc4000e437d4afda9f2f6f` |

`raw.tar.gz` contains the accepted raw CSVs, metadata, correctness logs,
and per-run versions under `accepted/trial*-*/`. The four `*-build.json`
files retain binary sizes, control hashes, compiler flags, and unit logs.

## Reproduction

See [the benchmark archive guide](../README.md) for analysis and rerun commands.
The archived numbers describe the recorded images and source hashes.
Current package mirrors or changed source will produce a new experiment.
