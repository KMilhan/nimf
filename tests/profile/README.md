# Input-path profiling

This diagnostic locates elapsed time inside Nimf before attempting a 100 us
input-processing target. It does not change the input algorithm or IPC protocol.
The existing screen benchmark polls every 500 us, so it cannot establish a
100 us display target. Physical keyboard, compositor, scanout, and panel latency
remain outside this test.

See the [initial measurements and observer-cost results](RESULTS.md) for the
first Ubuntu/Arch investigation.

## Run

Use the existing isolated Ubuntu and Arch dependencies:

```sh
REUSE_BASE=1 PROFILE=1 INPUT_LANGUAGE=ko bash tests/latency/run.sh arch
REUSE_BASE=1 PROFILE=1 INPUT_LANGUAGE=ja bash tests/latency/run.sh ubuntu
```

Omit `REUSE_BASE=1` to build fresh dependencies. `PROFILE=1` selects
`--enable-profiling`; normal builds compile out all scopes and the collector.
Profiling images use the separate `nimf-latency-<distro>:profile` tag.
Set `PROFILE_CPU=1` to also record thread CPU time. This costs more than wall
timestamps alone. `ITERATIONS`, `WARMUP`, and `TOOLKIT` retain their existing
latency-runner meanings. Profiling is Nimf only. `PROFILE=1` with another
`IM_FRAMEWORK` fails before the runner builds an image, and the container session
rejects the same combination.

For a custom isolated session, build with `--enable-profiling` and set
`NIMF_PROFILE_DIR` to an existing writable directory before starting Nimf and
its clients. Each thread creates a separate file with mode 0600 and refuses to
overwrite an existing file. `NIMF_PROFILE_CPU=1` enables CPU clocks.
The default buffer holds 65,536 spans per thread, about 3 MiB on x86-64.
`NIMF_PROFILE_CAPACITY` accepts 256 through 1,048,576 spans.

## Outputs

Each toolkit result directory includes:

- `profile/nimf-<pid>-<thread>.csv`: bounded raw spans with completion metadata.
- `profile-keys.csv`: paired client/server durations for each accepted key press.
- `profile-summary.json`: per-phase, step, role, span, and message distributions.
- `profile.md`: measured-key timings and hotspots sorted by total exclusive wall time.
- `profile-trace.json`: a Chrome trace JSON timeline, readable by
  [Perfetto](https://perfetto.dev/docs/getting-started/other-formats).

Recompute with `python3 tests/profile/analyze.py <toolkit-result-directory>`.
The analyzer uses only Python's standard library and supports the Python 3.12
runtime in the Ubuntu test image and the Arch image's Python runtime.

The analyzer pairs every client filter call with exactly one server call inside
its interval. GTK can filter an unhandled key more than once. These calls are
retained and counted, with their durations summed per physical key. The separate
`client_span_us` includes the gaps between those calls. This correspondence assumes the benchmark's serial input and
private daemon. It is not a general causal join across concurrent desktop
clients. Release events, setup, negative controls, first inputs, and warmups
are excluded from the measured-key tables. First/warmup spans remain in the
trace and JSON distributions.

Missing, truncated, overlapping, dropped, or incomplete traces fail analysis.
Buffers do not wrap or flush on the timed path. Normal thread/process exit
writes them; SIGKILL, a crash, or a live unjoined worker can leave incomplete
files. The GTK probe now exits on SIGTERM, with a two-second driver fallback.
No key values, input strings, surrounding text, or object pointers are recorded.

## Read the profile

| Span | Measured work |
|---|---|
| `client.filter` | `nimf_im_filter_event`, including its lock and synchronous request |
| `client.lock` | Acquisition of the client transport lock |
| `client.request` | Send and wait for a client request |
| `client.dispatch` | Callback delivery to the toolkit and reply acknowledgement |
| `client.state` | Copying a received preedit string and attributes into `NimfIM` |
| `client.signal` | `NimfIM` signal emission, including toolkit and application handlers |
| `gtk.preedit` | GTK module conversion of the preedit to a Pango attribute list |
| `gtk.cursor` | GTK module conversion of the cursor area to root coordinates |
| `server.filter` | Shortcut dispatch and language-engine filtering |
| `engine.filter` | Language-engine call, including its nested callbacks |
| `server.emit` | Synchronous preedit/commit/surrounding callback round trip |
| `ipc.send`, `ipc.receive`, `ipc.wait` | Transport, validation, and nested main-context dispatch |
| `hangul.process` | The `hangul_ic_process` call only |
| `mozc.key`, `mozc.start` | Helper key operation and lazy startup check |
| `mozc.request`, `mozc.read`, `mozc.parse` | Helper request, reply wait/read, and S-expression parsing |

`detail` identifies message type for transport/request/dispatch spans, and
press (0) or release (1) for filter spans. Message numbers are defined in
`libnimf/nimf-message-private.h`: for example, filter/reply are 5/6,
preedit-start/reply 19/20, preedit-changed/reply 23/24, and commit/reply 25/26.
No message contents are captured.

Inclusive wall time contains nested calls. Exclusive wall time subtracts only
instrumented direct children, so it includes uninstrumented work and waits.
CPU time is time used by the current thread, including kernel execution, not
time spent by the helper or server processes. Wall time minus CPU time includes
blocking and scheduler delays; it does not identify their cause by itself.
The clocks use [`CLOCK_MONOTONIC` and `CLOCK_THREAD_CPUTIME_ID`](https://man7.org/linux/man-pages/man3/clock_gettime.3.html).
Nanosecond units do not imply nanosecond accuracy. The sample join retains the
existing observer's microsecond injection timestamps.

Client and server intervals overlap. Do not add their inclusive times.
`outside_server_us` is client duration minus server duration; it includes client
work and transport/scheduling outside the server filter, not pure socket cost.
`client.signal` self time is toolkit work, such as GTK layout. In the latency
fixture it also includes the probe's own preedit and text handlers. Nested
cursor-location requests and the `gtk.*` spans are its instrumented children.
Similarly, `engine.filter` includes callback waits; use its children and CPU
columns before calling it an engine-computation hotspot.

## Validate overhead

```sh
bash tests/profile/check.sh
# Use the fixture binary directory printed by check.sh:
python3 tests/profile/overhead.py build/profile-check-XXXXXX
```

The calibration repeats 100,000 minimal scopes per mode in five alternating
rounds. It reports nanoseconds per scope for compiled-out, runtime-disabled,
wall-only, and wall-plus-CPU modes. Initialization and exit-time file output are
outside the timed loop. This isolates recorder overhead, not its whole-system
cache/scheduling impact. Also compare the same instrumented image with
`PROFILE=0` and `PROFILE=1` in repeated serial runtime trials before interpreting
small differences. Do not subtract calibration numbers from measured latency.

For that runtime comparison, use a retained profiling image directly with
`docker create -e PROFILE=0 ...` and `docker create -e PROFILE=1 ...`.
The normal runner rebuilds the image when its build options change.
