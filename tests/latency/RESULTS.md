# Framework comparison: Nimf, IBus, Fcitx5, upstream

Measured 2026-11-16 on one host (Intel Core Ultra 7 265U, Linux 7.3.4-1-cachyos). The framework
tables build this repository on top of `b7e7d59`, before the cursor origin cache described at the
end of this file. Each run directory keeps `source.sha256`, so rows sharing one hash share one
tree. Frameworks come from distro packages selected through `IM_FRAMEWORK`, and `upstream` builds
this fork directly from a pinned revision of `hamonikr/nimf`. Each cell pools two runs of 100
measured keys after ten warmup cycles. The runner builds `nimf-latency-<os>:frameworks-<framework>`
on top of the Nimf test image, so the Nimf images stay free of comparison packages.

## Steady state: screen_upper_ms p50 (p95)

| OS | Language | Toolkit | Nimf | IBus | Fcitx5 |
|---|---|---|---:|---:|---:|
| ubuntu | ko | gtk3 | 1.35 (1.65) | 1.96 (2.41) | 1.40 (2.01) |
| ubuntu | ko | gtk4 | 1.36 (1.95) | 2.70 (3.73) | 1.90 (2.30) |
| ubuntu | ja | gtk3 | 1.49 (2.15) | 2.45 (22.04) | 2.00 (15.30) |
| ubuntu | ja | gtk4 | 1.91 (2.61) | not measured | 2.06 (2.80) |
| arch | ko | gtk3 | 1.36 (1.75) | 1.53 (2.25) | 1.40 (3.13) |
| arch | ko | gtk4 | 1.36 (1.75) | 2.62 (3.32) | 1.58 (2.48) |
| arch | ja | gtk3 | 1.50 (2.11) | 2.06 (2.79) | 2.22 (13.63) |
| arch | ja | gtk4 | 1.85 (2.34) | 3.03 (3.91) | 2.18 (3.34) |

## Steady state: key_ms and model_ms p50

| OS | Language | Toolkit | key_ms Nimf/IBus/Fcitx5 | model_ms Nimf/IBus/Fcitx5 |
|---|---|---|---|---|
| ubuntu | ko | gtk3 | 0.16 / 1.19 / 0.89 | 0.38 / 1.05 / 0.73 |
| ubuntu | ko | gtk4 | 0.14 / 0.15 / 0.14 | 0.37 / 1.40 / 0.77 |
| ubuntu | ja | gtk3 | 0.16 / 1.26 / 0.75 | 0.84 / 1.59 / 1.16 |
| ubuntu | ja | gtk4 | 0.15 / — / 0.14 | 0.94 / — / 1.20 |
| arch | ko | gtk3 | 0.15 / 1.04 / 0.85 | 0.39 / 0.92 / 0.70 |
| arch | ko | gtk4 | 0.13 / 0.14 / 0.14 | 0.48 / 1.34 / 0.81 |
| arch | ja | gtk3 | 0.15 / 1.10 / 0.80 | 0.85 / 1.35 / 1.24 |
| arch | ja | gtk4 | 0.13 / 0.14 / 0.13 | 0.98 / 1.79 / 1.24 |

`key_ms` covers the toolkit key path. IBus GTK3 keeps a slow key path that GTK4 does not show.

## First-input startup (worst first-cycle step, ms)

| OS | Language | Nimf gtk3/gtk4 | IBus gtk3/gtk4 | Fcitx5 gtk3/gtk4 |
|---|---|---|---|---|
| ubuntu | ko | 3.7 / 4.0 | 4.2 / 4.6 | 4.4 / 4.1 |
| ubuntu | ja | 213 / 212 | 4.1-17.1 / — | 26.8 / 8.1 |
| arch | ko | 3.7 / 4.6 | 10.4 / 4.8 | 3.8 / 4.1 |
| arch | ja | 209 / 210 | 6.1 / 6.0 | 18.3 / 12.0 |

The Nimf Japanese startup cost is the Mozc helper and server process start on the first Japanese
key. It is excluded from the steady-state tables. IBus and Fcitx5 start their engines with the daemon.

## Upstream Nimf baseline

`IM_FRAMEWORK=upstream` builds `hamonikr/nimf` at `b4a141aec1c833541f163da8b1beb9ab1e0c0615`
(version 1.4.19), the same revision the 2026-11-08 archive used, with `-g -O2`, GTK4 enabled,
and m17n disabled. Korean is the only shared cell: upstream has no Mozc engine, and Anthy is
not substituted for it. Each row pools two runs of 100 measured keys per step.

### Pinned repeat on the current tree

The fork arm carries the cursor origin cache described below. Both arms ran pinned with
`CPUSET=0-3` and `POLL_US=150`, Korean, two runs per arm, pooled over the five measured steps.

The screen term is sampled more finely than in the unpinned tables above, which use
`POLL_US=500`.

| OS | Toolkit | fork screen p50 (p95) | upstream screen p50 (p95) | key_ms fork/upstream | model_ms fork/upstream |
|---|---|---:|---:|---|---|
| ubuntu | gtk3 | 0.819 (1.213) | 0.809 (1.123) | 0.150 / 0.145 | 0.318 / 0.285 |
| ubuntu | gtk4 | 0.798 (1.149) | 0.791 (1.158) | 0.127 / 0.125 | 0.309 / 0.298 |
| arch | gtk3 | 0.785 (1.078) | 0.798 (1.142) | 0.140 / 0.139 | 0.295 / 0.289 |
| arch | gtk4 | 0.796 (1.082) | 0.800 (1.145) | 0.123 / 0.121 | 0.381 / 0.378 |

- End-to-end Korean latency stays at parity. Every screen gap is at or below the 0.022 ms
  run-to-run spread.
- The one gap that repeats is `model_ms` on Ubuntu GTK3: the fork runs 0.309 and 0.322 ms,
  upstream 0.274 and 0.294 ms. The cause is not isolated.
- Worst first-cycle `model_ms` across all runs: 2.29 ms for the fork, 2.50 ms for upstream.
- Both arms use the same Mozc server and helper hashes on Arch (`cc3a50db`) and on Ubuntu
  (`bbf20e09`), so these rows compare framework paths, not engines.
- All sixteen toolkit runs passed both negative controls. `shutdown.txt` records `graceful`
  for every fork run and `forced` for every upstream run.

### Earlier unpinned sweep

| OS | Toolkit | fork p50 (p95) | upstream p50 (p95) | key_ms fork/upstream | model_ms fork/upstream |
|---|---|---:|---:|---|---|
| ubuntu | gtk3 | 1.353 (1.653) | 1.391 (1.913) | 0.160 / 0.166 | 0.376 / 0.403 |
| ubuntu | gtk4 | 1.359 (1.948) | 1.386 (1.910) | 0.143 / 0.147 | 0.374 / 0.366 |
| arch | gtk3 | 1.356 (1.751) | 1.360 (1.694) | 0.154 / 0.157 | 0.387 / 0.361 |
| arch | gtk4 | 1.364 (1.749) | 1.369 (1.689) | 0.133 / 0.135 | 0.483 / 0.445 |

- Korean end-to-end latency is at parity. The Ubuntu gap is +0.03 ms (+2 to 3%), inside the
  0.028 ms run-to-run spread. The Arch gap is 0.004 to 0.005 ms.
- Ubuntu GTK3 shows the clearest tail difference: p95 1.653 for the fork against 1.913 upstream.
- All eight upstream runs recorded `forced` in `shutdown.txt` because the upstream daemon did not
  exit on SIGTERM. Every fork run recorded `graceful`. This lifecycle difference is reported after
  measurement and is not counted as a latency result.
- Upstream passed both negative controls in every toolkit and OS image, with the same probe,
  driver, and language steps as the fork rows.

## Engine parity

- ubuntu: Nimf, IBus, and Fcitx5 all use the packaged Mozc server (`mozc-server 2.28.4715.102`,
  `/usr/lib/mozc/mozc_server` sha256 `bbf20e09`) and the same `mozc_emacs_helper` (`a977e28d`).
  IBus 1.5.29-rc2 and Fcitx5 5.1.7. These Japanese rows compare framework paths, not engines.
- arch: Nimf and IBus use the source-built Mozc 3.34.6345 server (`cc3a50db`) from revision
  `18e76511af9371a9a4409b1d13c3e30884b53e9c`. The IBus row adds the source-built `ibus_mozc` engine.
  The Fcitx5 Japanese row installs `fcitx5-mozc 3.35.6354.2`, which overwrites the server binary
  (`762ab9f0`), so that row also moves the Mozc server version. IBus 1.5.34-2 and Fcitx5 5.1.23-1.

## Findings

- Nimf has the lowest p50 in every measured cell. The gap is largest against IBus on GTK4
  (1.36 vs 2.62-2.70 ms for Korean) and on Japanese GTK3 for Ubuntu.
- Korean p50 spreads are narrow for all three frameworks. Japanese GTK3 shows heavy Fcitx5 tails
  (p95 13.6-15.3 ms) driven by commit steps.
- Ubuntu IBus Japanese GTK3 is the least stable cell: three runs gave p50 2.06, 2.45, and 3.75 ms
  with p95 14.4-27.4 ms. A fourth run degraded to 9.46 ms p50 and is excluded from the table.
- Arch IBus 1.5.34 passes the Japanese GTK4 accuracy gate, while Ubuntu IBus 1.5.29 does not,
  which points at the older IBus rather than the harness.

## Notes and limits

- `screen_upper_ms` is an upper bound sampled every 500 us. It is not a framebuffer present claim.
- Two sweeps used reversed framework order, so order effects are partly balanced, not removed.
- Ubuntu IBus Japanese GTK4 was not measured because the commit gate fails on IBus 1.5.29.
- First-input startup, not steady state, is where Nimf currently loses to both frameworks in Japanese.
- `versions.txt`, `samples.csv`, screenshots, and `shutdown.txt` (all `graceful`) are kept per run
  under `build/latency-<os>-<rand>/`. Both negative controls passed in every run.
- Every row above runs unpinned. A pinned repeat of the Nimf Arch GTK3 Korean cell on four P-cores
  gave screen p50 0.802 ms and model_ms p50 0.246 ms against 0.992 and 0.291 ms unpinned, and
  LP-core placement gave 2.685 and 1.029 ms (`build/vcpu-arch-*`). Rows are comparable inside one
  placement setting, not across settings.

## Cursor origin cache

The GTK3 and GTK4 client modules cache the root coordinates of the client window and reuse
them while the geometry GDK already tracks stays the same. This removes an
`XTranslateCoordinates` round trip from the key path. `bash tests/cursor/run.sh <image>` is
the correctness gate: it moves a window and requires one cursor report before the move and
one after it.

Both arms were built from the same tree, so they carry the same driver and probe, and the
arm order is reversed between repeats. Arch, Korean, `POLL_US=150`, `CPUSET=0-3`, 100
iterations, 10 warmup, four repeats. Both negative controls passed in all eight runs.

| Toolkit | Step | Base p50 | Cached p50 | Delta | Per-run spread |
|---|---|---:|---:|---:|---:|
| gtk3 | preedit-initial | 0.261 | 0.262 | +0.001 | 0.007 |
| gtk3 | preedit-vowel | 0.247 | 0.249 | +0.002 | 0.015 |
| gtk3 | preedit-final | 0.247 | 0.245 | -0.003 | 0.021 |
| gtk3 | commit | 0.457 | 0.427 | -0.029 | 0.022 |
| gtk3 | erase | 0.406 | 0.375 | -0.032 | 0.017 |
| gtk4 | preedit-initial | 0.418 | 0.402 | -0.016 | 0.007 |
| gtk4 | preedit-vowel | 0.386 | 0.373 | -0.012 | 0.019 |
| gtk4 | preedit-final | 0.386 | 0.376 | -0.010 | 0.017 |
| gtk4 | commit | 0.495 | 0.465 | -0.030 | 0.023 |
| gtk4 | erase | 0.429 | 0.400 | -0.029 | 0.030 |

- `model_ms` p50 in ms. GTK3 gains on commit and erase only; the GTK3 preedit rows stay
  inside the per-run spread and are reported as no change. GTK4 gains in every step.
- `screen_upper_ms` p50 moves -0.041 ms (GTK3) and -0.093 ms (GTK4) on commit and
  -0.016 and -0.012 ms on erase. The GTK4 commit base spread is 0.167 ms, so that cell
  carries the widest uncertainty.
- The profile span `gtk.cursor` falls from 11.5 ms to 0.32 ms over 600 calls on GTK3, and
  from 12.3 ms to 3.5 ms on GTK4. Profiled client P50 per key moves 12 to 31 us on GTK3 and
  2 to 60 us on GTK4, which is the same term.
- The GTK3 cache is exact: GDK reports the client window and toplevel position and size from
  its own cache, so a move or resize invalidates it without a round trip. GTK4 delivers no
  move event to the widget, so that cache expires after 250 ms and a moved window can keep a
  stale candidate position for at most that interval.
- Rejected variants. A cursor cache keyed on the reported area and one-way client signals
  both fail the cursor check because a window move leaves a stale position. A buffered IPC
  reader and a GTK4 preedit cache changed nothing measurable. Starting `mozc_emacs_helper`
  when the engine is created removed 3 ms of the 205 ms first Japanese key, so that term is
  Mozc first conversion work rather than Nimf startup.

    REUSE_BASE=1 CPUSET=0-3 POLL_US=150 INPUT_LANGUAGE=ko bash tests/latency/run.sh arch
    bash tests/cursor/run.sh nimf-latency-arch:test

## Reproduce

    BUILD_NETWORK=host REUSE_BASE=1 ITERATIONS=100 WARMUP=10 RUN_TIMEOUT=1200 \
      IM_FRAMEWORK=ibus INPUT_LANGUAGE=ja bash tests/latency/run.sh ubuntu
    BUILD_NETWORK=host REUSE_BASE=1 ITERATIONS=100 WARMUP=10 RUN_TIMEOUT=1200 \
      IM_FRAMEWORK=fcitx5 INPUT_LANGUAGE=ja bash tests/latency/run.sh arch

    BUILD_NETWORK=host REUSE_BASE=1 ITERATIONS=100 WARMUP=10 RUN_TIMEOUT=1200 \
      IM_FRAMEWORK=upstream INPUT_LANGUAGE=ko bash tests/latency/run.sh arch

Default `IM_FRAMEWORK=nimf` measures Nimf only and needs no comparison packages.
