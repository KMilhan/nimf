# Mozc isolation results

Checked on 2026-11-08 using the working tree based on `3dc9294`.
The existing, uncommitted Mozc implementation was tested without changing product code.
Both complete runs returned exit status 1. These are failing integration results.

## Environments

| Component | Ubuntu | Arch |
| --- | --- | --- |
| Distribution | Ubuntu 24.04.5 LTS | archlinux:latest, followed by pacman -Syu |
| Mozc helper/server | 2.28.4715.102, distribution packages | 3.34.6345.2, native upstream build |
| GTK3 / GTK4 | 3.24.41 / 4.14.5 | 3.24.52 / 4.24.1 |
| Qt5 / Qt6 | 5.15.13 / 6.4.2 | 5.15.19 / 6.12.0 |

Arch's helper and server use upstream revision
`18e76511af9371a9a4409b1d13c3e30884b53e9c`, matching AUR emacs-mozc.
The official fcitx5-mozc package does not supply mozc_emacs_helper.
This tests current Arch userspace with that native helper, not fcitx5 integration.

Runtime containers used no network, host mounts, or host display/session sockets.
Each toolkit used a fresh Mozc profile and private session bus under Xvfb.
Build networking used `BUILD_NETWORK=host` because bridge downloads timed out.

## Results

| Check | Ubuntu | Arch |
| --- | --- | --- |
| Full source build | Pass | Pass |
| Mozc unit cases | 11/11 pass | 11/11 pass |
| Real helper preedit and commit | Pass | Pass |
| GTK3, GTK4, Qt5, Qt6 conversion | All pass | All pass |
| Backspace, cancellation, focus transfer | All pass | All pass |
| Recovery after helper termination | All pass | All pass |
| Next-page candidate commit | All pass | All pass |
| Candidate window presence | All pass | All fail |
| Candidate position | GTK4 fails; other three pass | Not testable without a window |
| Mouse selection followed by Enter | All pass | Skipped: no candidate window |
| Nimf termination while a client is present | SIGSEGV in all four toolkit runs | SIGSEGV in all four toolkit runs |

## Confirmed problems

1. **New Mozc candidate field is not recognized.** Arch's helper emits
   `candidate-window`. `nimf-mozc.c:187` only reads `candidates`, so the engine
   hides the candidate window. Japanese conversion and keyboard selection still
   work. This reproduces with all four toolkits.

2. **GTK4 candidate coordinates are not screen coordinates.** On Ubuntu, the app
   origin was `(402, 340)`, but the candidate popup appeared at `(52, 32)`.
   `im-nimf-gtk4.c:482` forwards widget-relative coordinates without converting
   them to the coordinate space used by the candidate service. The captured image
   confirms that the candidate window is detached from the entry.

3. **Nimf crashes during concurrent server/client termination.** Cleanup sends
   SIGTERM to Nimf and the app. Nimf returns 139, while the app returns the expected
   143. Logs reach `nimf_service_ic_finalize` during server destruction.
   `nimf-server.c:315` frees the IC array before destroying services, while
   `nimf-service-ic.c:837` removes an IC from that array. No debugger backtrace or
   sanitizer run was collected. This existing lifecycle path was not changed.

Full `make check` was also attempted. Both distributions fail the existing m17n
language-catalog test with `Key sat not found in c_table`. The focused Mozc suite
does not suppress this result; it runs separately from the unrelated catalog test.

## Evidence and reproduction

```sh
BUILD_NETWORK=host bash tests/mozc/run.sh ubuntu
BUILD_NETWORK=host bash tests/mozc/run.sh arch
```

Final artifacts are in:

- `build/mozc-ubuntu-H2cG0q/`
- `build/mozc-arch-fs5g3r/`

Each directory contains `smoke.log`, `unit.log`, `versions.txt`, `image.json`,
per-toolkit daemon logs, entry state, and screenshots. Raw helper replies are
saved as `helper-protocol.log` alongside those results. Additional m17n logs and
independent Claude review notes are in `build/mozc/`.

ShellCheck and `git diff --check` pass. Independent Claude reviews identified
test synchronization, build-context, cleanup, and timeout issues; those were
addressed. The final host-side timeout returned 124 after two seconds against a
container that ignores SIGTERM. Its hard-kill fallback was separately checked
against a local child that ignores SIGTERM: status 137 after four seconds.
These wrapper checks do not change the GUI scenarios above.

Native Wayland compositor integration, global desktop shortcuts, browser input,
and installation of distributable Nimf packages were not tested.
