# Benchmarks

This repository ships a reproducible input-to-screen benchmark. It measures a real X11 key
event from injection until the probe widget shows a stable new raster, in a fresh container
per toolkit. Everything runs from a repository checkout and leaves host input settings
untouched.

## What is measured

Three timings per composition step, in ms:

- `key_ms`: the toolkit key path, from XTEST injection until the toolkit handler returns.
- `model_ms`: the engine path, until the preedit or commit request reaches the client.
- `screen_upper_ms`: an upper bound until the entry region shows a stable new raster. The
  framebuffer is sampled every `POLL_US`, so this term carries that sampling quantum. It is
  not a framebuffer present claim.

A cycle types a composition, commits it, and erases it. Each step keeps its own distribution.
Two negative controls run after every toolkit benchmark, and both must fail to produce pixels,
so a passing row proves the raster gate is real:

- an invisible entry that processes text without a lasting visible change
- a simple input context that renders the raw key instead

## How to run

    BUILD_NETWORK=host REUSE_BASE=1 bash tests/latency/run.sh ubuntu
    BUILD_NETWORK=host REUSE_BASE=1 INPUT_LANGUAGE=ko bash tests/latency/run.sh arch

`IM_FRAMEWORK` selects what is measured: `nimf` (default), `upstream`, `ibus`, or `fcitx5`.
`PROFILE=1` records Nimf spans as well. `CPUSET` pins the container, and it should be set for
any comparison, because placement dominates the result. `tests/latency/README.md` documents
every variable.

Rows are comparable only inside one placement and `POLL_US` setting.

## Fork against upstream Nimf

`IM_FRAMEWORK=upstream` builds `hamonikr/nimf` at `b4a141aec1c833541f163da8b1beb9ab1e0c0615`
(version 1.4.19) in the same image, with the same compiler flags, probe, driver, and language
steps. Korean is the only shared cell, because upstream has no Mozc engine. On Arch both arms
use the same source-built Mozc server binary.

Pinned repeat on the current tree, which includes the GTK cursor origin cache:
`INPUT_LANGUAGE=ko`, `CPUSET=0-3`, `POLL_US=150`, 100 measured keys after 10 warmup cycles,
two runs per arm, pooled.

| OS | Toolkit | fork screen p50 (p95) | upstream screen p50 (p95) | key_ms fork/upstream | model_ms fork/upstream |
|---|---|---:|---:|---|---|
| ubuntu | gtk3 | 0.819 (1.213) | 0.809 (1.123) | 0.150 / 0.145 | 0.318 / 0.285 |
| ubuntu | gtk4 | 0.798 (1.149) | 0.791 (1.158) | 0.127 / 0.125 | 0.309 / 0.298 |
| arch | gtk3 | 0.785 (1.078) | 0.798 (1.142) | 0.140 / 0.139 | 0.295 / 0.289 |
| arch | gtk4 | 0.796 (1.082) | 0.800 (1.145) | 0.123 / 0.121 | 0.381 / 0.378 |

- End-to-end Korean latency is at parity. Every screen gap is at or below the 0.022 ms
  run-to-run spread.
- The one repeatable gap is `model_ms` on Ubuntu GTK3: the fork runs 0.309 and 0.322 ms,
  upstream 0.274 and 0.294 ms. The cause is not isolated.
- Worst first-cycle `model_ms` across all runs: 2.29 ms for the fork, 2.50 ms for upstream.
- Every fork run exited `graceful` on SIGTERM. Every upstream run needed `forced`. This
  lifecycle difference is reported after measurement and is not scored as latency.
- All sixteen toolkit runs passed both negative controls.

`tests/latency/RESULTS.md` keeps the earlier unpinned sweep and the cursor cache study.

## Against other frameworks

Steady state `screen_upper_ms` p50, Korean, GTK4, unpinned:

| OS | Nimf | IBus | Fcitx5 |
|---|---:|---:|---:|
| ubuntu | 1.36 | 2.70 | 1.90 |
| arch | 1.36 | 2.62 | 1.58 |

Japanese GTK3 on Ubuntu gives Nimf 1.49 against IBus 2.45 and Fcitx5 2.00. Nimf loses the
first Japanese key, where the Mozc helper and server start costs about 210 ms, while IBus and
Fcitx5 start their engines with the daemon. Full tables, engine parity notes, and limits are
in `tests/latency/RESULTS.md`.

## Correctness gates

A performance change is kept only when these pass:

| Gate | Command |
|---|---|
| unit and integration tests | `make check`, run by the image build |
| leak and sanitizer lifecycle | `REUSE_BASE=1 bash tests/memory/run.sh ubuntu sanitize` |
| cursor position after a window move | `bash tests/cursor/run.sh <image>` |
| profiler self checks | `bash tests/profile/check.sh` |
| runner argument plumbing | `bash tests/latency/check.sh` |

`tests/cursor/run.sh` is the gate that rejected cached cursor variants which reported a stale
candidate position after a window move.

## Limits

- Placement dominates. The same Nimf Arch GTK3 Korean cell gives 0.80 ms pinned to four
  P-cores, 0.99 ms unpinned, and 2.69 ms on LP cores.
- `screen_upper_ms` is bounded by `POLL_US`, so a delta below the poll quantum is not
  resolvable.
- One host: Intel Core Ultra 7 265U, Linux 7.3.4-1-cachyos, with distro packages from the
  images. Numbers do not transfer across hosts, toolkits, or engine versions.
