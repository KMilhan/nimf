# Forked Nimf, upstream Nimf, IBus, and Fcitx5 input latency

Measured on 2026-11-08 in private Docker/Xvfb sessions on the same host.
See [comparison.md](comparison.md) for the compact P50/P95 tables,
[tables.md](tables.md) for ranges and diagnostic metrics, and `summary.csv` for each trial,
step, and metric. Raw samples, correctness logs, and representative screenshots are in
`raw.tar.gz`. Extract it to recover the original case directory structure.
Two failing Ubuntu IBus GTK4 logs are included as excluded diagnostics.

## Results

All 42 accepted executions finished: 81 toolkit datasets, 16,950 measured keys,
and 162 negative controls. Every accepted sample passed semantic and stable-pixel
checks. Sample counts and timestamp bounds also passed the post-run validator.
ShellCheck, `git diff --check`, and the benchmark/full-source hash checks passed.
Driver and probe builds passed `-Wall -Wextra -Werror`. Representative preedit and commit PNGs
were visually inspected for all four frameworks. All owned runtime containers
were removed after collecting their artifacts.

- Korean composition P50 for the current fork was 1.27–1.33 ms across the four
  OS/toolkit combinations. Fcitx5 was 1.31–1.33 ms. These small differences do
  not establish an overall winner.
- IBus Korean GTK4 P50 was 2.02 ms on Ubuntu and 1.94 ms on Arch; P95 was
  2.69 and 2.66 ms. Its GTK3 results were closer to the other frameworks.
- Upstream Nimf and the fork were broadly similar. Upstream Ubuntu GTK4's
  median run P50 was 0.729 ms, but the individual run P50s spanned
  0.725–1.313 ms. Upstream Arch GTK3 similarly varied from 0.772 to 1.316 ms.
  Model timings were close to the fork. The typical frame-read interval width
  was 0.65–0.71 ms, so these screen-upper-bound fluctuations must not be read
  as a demonstrated memory-safety overhead or universal upstream speedup.
- Warm Japanese composition had lower observed P50 in the current Nimf fork
  than in IBus or Fcitx5 in the valid cells. Arch's Fcitx5 Mozc version differs,
  and the Ubuntu GTK4 IBus cell failed correctness, as detailed below.
- Japanese first-input latency was much higher in Nimf: 207.7–273.4 ms across
  the first keys of all runs. IBus was 3.2–6.4 ms and Fcitx5 7.6–26.5 ms.
  This includes Nimf's lazy helper/server startup while competitors can start
  their engines before the probe. It is a real result for these startup
  policies, not a measurement of equal cold-start work.
- All 12 upstream Nimf toolkit runs required forced daemon shutdown after
  successful measurements and controls. The current fork, IBus, and Fcitx5
  exited normally. See `shutdowns.csv`.

## Method

- Current Nimf worktree, built with `-g -O2`, without sanitizers. Competitors use
  the distribution packages. This compares input stacks, not identical
  compiler optimization flags for every dependency.
- The same driver and GTK probe binaries are used across frameworks within each
  OS. GTK3 and GTK4 use separate fresh HOME, runtime directory, D-Bus, and Xvfb.
  No host display, input settings, home directory, or session bus is exposed.
- GTK4 uses Cairo. GTK cursor blinking and animations are disabled. The primary
  endpoint is XTEST injection to stable pixels in the entry interior, read from
  the root framebuffer. The entry allocation has an 8-pixel inset to exclude
  focus and adjacent popup borders. Candidate selection is not measured.
- One first cycle, ten warmup cycles, then fifty measured cycles. Three serial
  trials rotate framework order. Japanese uses Nimf/IBus/Fcitx5; Korean adds
  upstream Nimf as a fourth entry. Each successive trial starts one position
  later. Three trials do not fully balance all four Korean order positions.
  Each key has seeded 0–20 ms jitter. Poll sleep is 500 us.
- Japanese: `a` -> `あ`, Enter -> committed `あ`, Backspace -> empty.
  Korean Dubeolsik: `g`, `k`, `s` -> `ㅎ`, `하`, `한`, Enter -> committed `한`,
  Backspace -> empty. Word commit and automatic reordering are disabled.
- The summary's Korean composition distribution pools the three composition
  stages equally. Detailed per-stage, commit, and erase results remain in CSV.
  Table P50 and P95 values are the median of the three corresponding per-run
  percentiles, not a percentile over an undisclosed pooled sample set.
- Every accepted toolkit case must pass semantic output and final stable pixel
  checks, plus two negative controls: invisible entry and visible ASCII from the
  wrong input context. Rejected runs are preserved and never silently pooled.
- Host load is recorded at each run in `series.log`. Tests are unpinned on a
  shared host. These measurements do not establish sub-millisecond differences
  outside the observed run variation and polling interval.

## Versions and comparability

| Component | Ubuntu 24.04 | Arch updated during preparation |
|---|---|---|
| IBus | 1.5.29-2 | 1.5.34-2 |
| IBus Hangul | 1.5.5-2build2 | 1.5.5-5 |
| Fcitx5 | 5.1.7-1build3 | 5.1.23-1 |
| Fcitx5 Hangul | 5.1.1-1 | 5.1.11-1 |
| libhangul | 0.1.0+git20230314-2build1 | 0.2.0 |
| Mozc for Nimf and IBus | 2.28.4715.102+dfsg-2.2build7 | 3.34.6345.2 |
| Mozc for Fcitx5 | 2.28.4715.102+dfsg-2.2build7 | 3.35.6354.2-1 |

Ubuntu uses the same packaged Mozc source version and server for all three
frameworks. Arch's IBus client was built from the existing Nimf helper/server
source revision `18e76511af9371a9a4409b1d13c3e30884b53e9c` using the cached Bazel
release build. Arch's official Fcitx5 Mozc package uses a newer server. Its
separate image preserves that version without replacing the Nimf/IBus server.
Arch Japanese results therefore compare these specific stacks with a version
confound; they cannot establish an intrinsic framework ranking. The two Arch
images otherwise have identical installed package versions, driver/probe hashes,
and libhangul binary. Korean comparisons use the same libhangul build within an OS.

Full package versions and hashes are recorded in `*-packages.txt`; each trial
also retains image identity and runtime `versions.txt`. Source manifests identify the measured checkout. Measurements preceded
publication; see the parent README for the resulting source commits.
No host input configuration change was made.

## Upstream Nimf baseline

The baseline is this fork's direct upstream,
[`hamonikr/nimf`](https://github.com/hamonikr/nimf), at master on the measurement date,
`b4a141aec1c833541f163da8b1beb9ab1e0c0615` (version 1.4.19). Its identity was
verified with `git ls-remote` and the source was exported with `git archive`.
It is not the earlier historical nimf-i18n repository. At measurement time,
the fork's HEAD added a Hangul delimiter fix. Its working tree contained the
Mozc and memory-safety changes subsequently published in this series.

`recipe/Dockerfile.upstream` builds the untouched upstream source with `-g -O2`
on each OS's comparison image. GTK3 and GTK4 IM modules, core library, server,
and Hangul engine are replaced by the upstream build. GTK4's GIO module cache
is regenerated as an installation step. m17n is disabled at build time and
unrelated engines are disabled at runtime, matching the requested scope.
Upstream has GTK3/GTK4 and Hangul support but no Mozc engine, so its Japanese
Mozc cell is unsupported. Its Anthy engine is not substituted for Mozc.

The baseline was requested after the first four accepted cases. The serial
runner was stopped between cases while the upstream images were built. Those
completed Nimf/Fcitx5 cases were retained, and no measured interval overlapped
these builds. The initial IBus case was later superseded when its default panel
was enabled following independent review. Its excluded data remains in the local experiment archive.

Upstream input and both negative controls passed in preflight on both toolkits
and OS images. However, the upstream daemon did not exit on SIGTERM after the
tests. The harness now records `shutdown.txt` and kills the owned daemon after a
five-second grace period. This post-measurement lifecycle failure is reported
separately; it is not counted as an input latency or hidden as a clean shutdown.

## Correctness failure excluded from timing

Ubuntu IBus 1.5.29 + Mozc + GTK4 fails the first Enter/commit step. Both the
committed text and stale preedit remain `あ`. The log reports:

```text
ibus_input_context_post_process_key_event: Type 'h' is not supported.
Timeout at commit: key=65293 preedit='あ' text='あ' pixel_change=1 final_pixel_change=1
```

This reproduced in `preflight5-ubuntu-ibus-ja` and the serial
`preflight6-ubuntu-ibus-ja`. The default package behavior is retained and the
GTK4 Japanese cell is excluded rather than weakening semantic validation or
patching a competing framework. GTK3 Japanese and both Korean toolkit paths
remain eligible. Arch's newer IBus passed this preflight.

The observed failure matches the missing hide-preedit case in
[IBus 1.5.29's post-processing code](https://github.com/ibus/ibus/blob/1.5.29/src/ibusinputcontext.c).
[IBus 1.5.30](https://github.com/ibus/ibus/blob/1.5.30/src/ibusinputcontext.c)
handles that event. A separate application's
[documented compatibility fix](https://github.com/ghostty-org/ghostty/pull/5324)
also describes the missing preedit-end behavior in 1.5.29. This comparison does
not modify IBus or infer that every application exhibits the same symptom.

## Instrumentation checks

Independent Claude review accepted the timing and negative controls but found
that disabling only IBus's panel made its configuration lighter. The final
configuration keeps the default panel enabled for all IBus cases. Mozc retains
its packaged renderer selection; Ubuntu's older version does not support the
newer `mozc_renderer` protobuf field, so no such field is written. The native
IBus panel is available alongside the other frameworks' enabled candidate UIs.
The panel and bounded-shutdown changes received focused independent follow-up
reviews. The timing review and both focused follow-ups completed with no unresolved
confirmed findings. Raw reviewer session metadata is not published.

Some IBus and Fcitx5 GTK3 filters consume keys before the widget's key callback.
`key_received_us=0` and an empty `key_ms` mean unobserved, not zero latency.
Widget key timing is diagnostic only; injection-to-model and injection-to-screen
remain defined. Forwarded duplicate widget events retain the first timestamp.

Initial preflights found candidate-window borders in the old whole-window crop.
The final probe reports the entry interior. The invisible-entry control now
requires a first-key timeout with unchanged final pixels; transient popup
movement does not satisfy the stable endpoint. Both failed diagnostics and the
subsequent successful controls are preserved. Earlier Nimf-only numbers used a
larger crop and should not be directly merged with this dataset.


## Reproduction

See [the benchmark archive guide](../README.md) for analysis and rerun commands.
The archived numbers describe the recorded images and source hashes.
Current package mirrors or changed source will produce a new experiment.
