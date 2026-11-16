# Input-to-screen microbenchmark

Run from a repository checkout:

```sh
bash tests/latency/run.sh ubuntu
bash tests/latency/run.sh arch
INPUT_LANGUAGE=ko bash tests/latency/run.sh arch
```

This builds the current source without sanitizers and measures GTK3 and GTK4
separately. The default Arch base starts from `archlinux:latest` and updates its
packages. To reuse the existing isolated Mozc build dependencies:

```sh
REUSE_BASE=1 bash tests/latency/run.sh arch
```

Docker is required. Set `BUILD_NETWORK=host` if Docker's build network cannot
reach package mirrors. Runtime containers have no network, host mounts, desktop
sockets, or session bus access. Each toolkit receives a fresh home, Nimf/Mozc
profile, D-Bus session, and Xvfb display. Host input settings remain untouched.

## What is measured

An independent X11 driver injects real key events using XTEST. The probe is a
normal `GtkEntry`, with the framework selected through `GTK_IM_MODULE`. GTK4 signals are
observed on its `GtkText` delegate. The driver reads the root framebuffer with
`XGetImage`, cropped to the entry's actual widget allocation with an 8-pixel
inset. The probe reports that rectangle after layout. Focus borders, window
margins, and candidate popups below the entry
are excluded, so popup pixels cannot satisfy the invisible-entry control.

Japanese cycles type `a`, press Enter, then press Backspace:

| Step | Required preedit | Required committed text | Visible change |
|---|---|---|---|
| preedit | `あ` | empty | Kana composition appears |
| commit | empty | `あ` | Composition decoration disappears |
| erase | empty | empty | Committed character disappears |

`INPUT_LANGUAGE=ko` selects Nimf's libhangul engine with its default Dubeolsik
layout. Each Korean cycle types `g`, `k`, `s`, Enter, and Backspace. The required
preedits are `ㅎ`, `하`, and `한`; Enter commits `한`, and Backspace clears it.
Each composition step has its own timing distribution. The standalone driver
accepts `--language ja` or `--language ko`.

The driver requires both the expected semantic state and a changed raster that
remains stable for 25 ms. It records the last raster transition before that
stable period. The extra stability wait is excluded from the latency timestamp.
Intermediate repaints do not count as the final display endpoint. Cursor
blinking and GTK animations are disabled. The pointer is parked outside the
window, and no window manager or compositor runs in the test display.

Two negative controls run after each toolkit benchmark:

- An invisible entry processes `あ` or `ㅎ` without a lasting pixel change in the
  entry region. It must time out at the first key with the final raster unchanged.
- GTK's simple input context displays `a` or `g` instead. It must time out.

The first cycle is recorded separately. For Nimf/Mozc its first key includes lazy
helper and server startup. It is not a cold filesystem-cache measurement. Ten warmup
cycles follow; only the next 100 cycles enter the reported percentiles. A fixed
random seed adds 0–20 ms before each key to vary its phase against GTK's frame
clock. Samples have one key in flight, rather than a saturated typing queue.

## Timing and uncertainty

All timestamps use GLib's monotonic clock on the same host kernel. The start is
immediately before the driver queues the XTEST press and release and flushes the
connection. The probe reports key receipt and preedit/buffer changes over a pipe.

`samples.csv` includes absolute microsecond timestamps and these durations:

| Column | Interval from injection, unless stated otherwise |
|---|---|
| `key_ms` | Probe's key handler is entered |
| `model_ms` | Latest matching preedit/buffer notification |
| `screen_lower_ms` | Start of the frame read preceding the final raster change |
| `screen_upper_ms` | Completion of the first read containing the final raster |
| `interval_ms` | Upper minus lower bound |
| `readback_ms` | Duration of the read that first observed the final raster |

IM filters can consume a key before the widget receives it. Such samples have
`key_received_us=0` and an empty `key_ms`; summaries count only observed key
callbacks. An asynchronous frontend can also forward a key after updating the
text. Widget callback timing is therefore diagnostic, not a comparable framework
latency metric. The acceptance condition always requires a fresh semantic update
after injection and a subsequent stable raster change. Screen and model timing
remain available for consumed keys. The first matching widget callback is kept
when a frontend delivers the same key more than once.

The X server samples pixels sometime between a read's request and reply. Using
the preceding read's **start**, rather than its completion, gives a conservative
lower bound. The summary uses the **upper bound**, with p50/p95/p99/max computed
by nearest rank. It also summarizes key/model timing, bound width, and readback
cost. No polling overhead is subtracted or hidden.

The nominal sleep between reads is 500 microseconds. Scheduling and X11 round
trips widen the actual interval, so this is not a claim of microsecond display
accuracy. Pixel polling and signal instrumentation add observer overhead. Use
the same image, sample budget, polling interval, renderer, and host load when
comparing revisions. Run comparisons serially and repeat whole runs. A small
change inside the observed variation is not a demonstrated improvement.

This endpoint is **pixels in the Xvfb framebuffer**. It excludes physical
keyboard/USB latency, compositor presentation, display scanout, and panel
response. It does not benchmark native Wayland, GTK/Qt applications other than
this fixture, long text, candidate selection, or arbitrary Mozc vocabulary.
GTK4 uses Cairo here. Xvfb timings must not be presented as real desktop latency.

The implementation follows the [XTEST specification](https://xorg.freedesktop.org/archive/X11R7.5/doc/Xext/xtestlib.pdf),
[Xlib image-read semantics](https://xorg.freedesktop.org/archive/current/doc/libX11/libX11/libX11.html#Transferring_Images_between_Client_and_Server),
and [GTK preedit notifications](https://docs.gtk.org/gtk4/signal.Text.preedit-changed.html).

## Artifacts and configuration

Each run writes a fresh `build/latency-<distribution>-*` directory with:

- Build/runtime logs, image identity, OS/toolkit/Mozc/Xvfb versions, and compiler flags.
- Git revision, dirty status, tracked diff, and file hashes including untracked
  source files. The image retains the exact source tree used for the build.
- Per-toolkit CSV samples and percentile summaries. First and warmup cycles
  remain in the CSV with their phase labels but are excluded from summaries.
- Preedit, committed, and empty entry screenshots from the first cycle.
- Logs for both negative controls. A timeout or correctness failure makes the
  overall command fail; failed samples are not silently discarded.
- `shutdown.txt` records `graceful` or `forced`. A daemon that ignores SIGTERM
  is killed after five seconds, after all measurements and controls finish.
  Report forced shutdown separately from latency and input correctness.

Configure the sample budget and polling interval through the runner:

```sh
REUSE_BASE=1 ITERATIONS=200 WARMUP=20 POLL_US=1000 \
  bash tests/latency/run.sh arch
```

`RUN_TIMEOUT` sets the whole-container timeout in seconds, default 600. Large
sample budgets may need a longer timeout. The standalone driver also accepts
`--timeout-ms` per key; see `nimf-latency-driver --help` inside the image.
No machine-independent latency threshold is enforced.
Set `TOOLKIT=gtk3` or `TOOLKIT=gtk4` to run one toolkit; the default runs both.

`CPUSET` restricts the container to the listed CPUs, for example `CPUSET=0-3` or
`CPUSET=2,5`. The default leaves placement to the host scheduler, and `cpuset.txt`
records what a run used. Host core type moves these numbers by more than any
measured code change, so keep pinned and unpinned rows apart and label them.

## Comparing frameworks

```sh
IM_FRAMEWORK=ibus INPUT_LANGUAGE=ja bash tests/latency/run.sh ubuntu
IM_FRAMEWORK=fcitx5 INPUT_LANGUAGE=ko REUSE_BASE=1 bash tests/latency/run.sh arch
```

`IM_FRAMEWORK` selects `nimf`, `ibus`, `fcitx5`, or `upstream` for the whole run. The default
is `nimf`. The runner rejects an unknown `IM_FRAMEWORK` or `INPUT_LANGUAGE`
before it builds anything, then passes the selected framework to the container.
A comparison run builds `nimf-latency-<distro>:frameworks-<framework>` on top of
`nimf-latency-<distro>:test`, so Nimf images stay free of IBus and Fcitx5
packages. `tests/latency/Dockerfile.frameworks.ubuntu` and
`tests/latency/Dockerfile.frameworks.arch` install the framework, its GTK3 and
GTK4 modules, and the language engine. The layer build writes
`frameworks-build.log`. `REUSE_BASE=1` skips only the dependency images, so the
Nimf image and the comparison layer still build. The same probe and driver
binaries serve every framework.

`IM_FRAMEWORK=upstream` measures this fork's direct upstream, `hamonikr/nimf`.
`tests/latency/Dockerfile.upstream` clones the revision pinned in
`UPSTREAM_REVISION` (default `b4a141aec1c833541f163da8b1beb9ab1e0c0615`, the
revision used by the 2026-11-08 archive) and installs its library, server, GTK3
and GTK4 modules, and Hangul engine over `nimf-latency-<distro>:test` with `-g
-O2`. The tag is `nimf-latency-<distro>:upstream` and the build log is
`upstream-build.log`. Upstream has no Mozc engine, so `IM_FRAMEWORK=upstream`
with `INPUT_LANGUAGE=ja` exits before any image build; Anthy is not
substituted. `versions.txt` copies the repository, revision, and flags from
`/opt/upstream-info`. Upstream keeps the `nimf` daemon and socket path, so the
same driver and readiness check apply. Upstream does not exit on SIGTERM, so
its runs record `forced` in `shutdown.txt`.

Arch IBus Japanese builds the IBus Mozc engine from the same source revision as
the Mozc server that Nimf uses, so both frameworks talk to one server binary.
Arch Fcitx5 Japanese installs the packaged Mozc engine instead, which replaces
`/usr/lib/mozc/mozc_server` in that image only. Korean keeps the source-built
server. `versions.txt` records package versions plus server and helper hashes, so
the engine difference stays visible in the evidence. Ubuntu uses packaged Mozc for
all three frameworks.

`PROFILE=1` is Nimf only. Combining it with `ibus`, `fcitx5`, or `upstream` fails before an
image build, and the container session rejects the same combination. The known
Ubuntu IBus 1.5.29 failure of the Japanese GTK4 commit gate needs `TOOLKIT=gtk3`
for that combination. `bash tests/latency/check.sh` verifies runner argument
passing and these rejections without starting a container.

`framework.sh` configures each fresh profile before starting its daemon. Korean
uses Dubeolsik with word commit and automatic reordering disabled. Japanese uses
Mozc's Hiragana mode. IBus keeps its default panel enabled. Its Mozc engine uses
the packaged candidate renderer, with the IBus panel available as a fallback.
Nimf and Fcitx5 also keep their candidate UI enabled. Only one framework is
launched in each session.
Both negative controls run for every framework, language, and toolkit.

Compare matching engine versions and shared-library builds within each OS.
Record package versions and binary hashes; a shared language engine name alone
does not establish identical code. Rotate framework order across repeated runs.
Report first-input startup separately because frameworks initialize engines at
different times. Use injection-to-screen timing as the primary comparison;
neither widget callbacks nor these whole-stack results isolate framework CPU cost.

## Related checks

`bash tests/cursor/run.sh nimf-latency-ubuntu:test` reuses the images built here to prove
that a window move still produces a fresh cursor location. It is the correctness gate for
the cached root coordinates in the GTK client modules, described in tests/cursor/README.md.

## Recorded results

The [2026-11-16 framework comparison](RESULTS.md) records Nimf against packaged IBus and Fcitx5 on Ubuntu and Arch in GTK3 and GTK4, with engine parity notes.

For internal hotspots, use the [optional input-path profiler](../profile/README.md).
It separates IPC waits, toolkit callbacks, engine work, and helper requests.
The existing framebuffer timing bounds remain unchanged.

The [2026-11-08 benchmark archive](../benchmarks/2026-11-08/README.md) includes
Korean and Japanese comparisons, upstream Nimf, O2/O3 measurements, and memory
footprint results with raw data and analysis scripts.
