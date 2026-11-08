# Mozc isolation results

Checked on 2026-11-08 using the working tree based on `3dc9294`.
Both final container runs returned exit status 0 after the fixes below.
The tested product sources and test scripts match the working tree by SHA-256.
No host input settings or packages were changed. No commit was created.

## Environments

| Component | Ubuntu | Arch |
| --- | --- | --- |
| Distribution | Ubuntu 24.04.5 LTS | archlinux:latest, followed by pacman -Syu |
| Mozc helper/server | 2.28.4715.102, distribution packages | 3.34.6345.2, native upstream build |
| GTK3 / GTK4 | 3.24.41 / 4.14.5 | 3.24.52 / 4.24.1 |
| Qt5 / Qt6 | 5.15.13 / 6.4.2 | 5.15.19 / 6.12.0 |

Arch's helper and server use upstream revision
`18e76511af9371a9a4409b1d13c3e30884b53e9c`, pinned in `Dockerfile.arch`.
This tests current Arch userspace with that native helper, not fcitx5 integration.

Runtime containers used no network, host mounts, or host display/session sockets.
Each toolkit used a fresh Mozc profile and private session bus under Xvfb.
Build networking used `BUILD_NETWORK=host` because bridge downloads timed out.
GTK4 was enabled and GTK2 disabled for the integration builds.

## Results

| Check | Ubuntu | Arch |
| --- | --- | --- |
| Full source build and default make check | Pass | Pass |
| Mozc unit cases, including old and new candidate fields | 12/12 pass | 12/12 pass |
| Real helper preedit and commit | Pass | Pass |
| GTK3, GTK4, Qt5, Qt6 conversion | All pass | All pass |
| Backspace, cancellation, focus transfer | All pass | All pass |
| Recovery after helper termination | All pass | All pass |
| Candidate presence, mouse selection, next-page commit | All pass | All pass |
| Candidate position before and after moving to another entry | All pass | All pass |
| Server shutdown with each toolkit still connected | All pass | All pass |
| System-keyboard-only shutdown: no client, client first, server first | All pass | All pass |
| GTK4 header bar at 2x scale: placement, movement, normal client close | Pass | Pass |

Screenshots were inspected for candidate alignment with the entry. In the 2x
GTK4 check, moving the application by `(300, 200)` moved the popup by exactly
`(300, 200)` on both distributions. This additional check rebuilt only the GTK
module over the test image using the same final product source.

## Fixes and scope

- The Mozc protocol adapter reads `candidate-window`, with `candidates` as the
  fallback for older helpers. Candidate UI and protocol tests use this adapter.
- GTK4 tracks its client widget with a weak pointer, including attachment before
  realization. Cursor updates resolve the current native surface and translate
  widget coordinates to logical X11 screen coordinates, including decoration
  offsets and scale. This also fixes the Arch header-bar case where no key
  controller existed when the widget was first attached.
- Server teardown retains the input-context array until services release their
  contexts. The original crash was confirmed in GDB at
  `nimf_service_ic_finalize` calling `g_ptr_array_remove_fast` on the freed array.
- m17n is disabled by default and excluded from this implementation and its
  validation. Default build images, Arch/RPM recipes, and the installer no longer
  require or automatically install it. The legacy source option remains explicit.
  Debian's `nimf-i18n` package and dependencies require `pkg.nimf.m17n`.

Debian metadata checks select only `nimf` and omit m17n build dependencies by
default. The explicit legacy profile selects both binaries. A mocked installer
check downloaded and installed only the core Debian package. These checks did
not install a package on the host or validate the legacy m17n engine.

An early shutdown fixture hit its three-second limit while Rime built its first
set of dictionaries. A separate control exited normally after seven seconds.
The lifecycle fixture now uses only the system keyboard and waits for the
SIGTERM handler before testing teardown.

## Evidence and reproduction

```sh
BUILD_NETWORK=host bash tests/mozc/run.sh ubuntu
BUILD_NETWORK=host bash tests/mozc/run.sh arch
```

Final complete runs:

- `build/mozc-ubuntu-kvDboO/`
- `build/mozc-arch-8p95u1/`

These contain build logs, `smoke.log`, `unit.log`, `versions.txt`, `image.json`,
per-toolkit daemon logs, entry state, and screenshots.

Additional evidence is in `build/mozc-fixes/`: source fingerprints, 2x GTK4
screenshots and logs, Debian metadata and mocked installer checks, and independent
Claude reviews. The final code and focused lifecycle/GTK4 follow-ups received no
confirmed High/Medium findings. ShellCheck and `git diff --check` pass.

Original failing baselines remain in `build/mozc-ubuntu-H2cG0q/` and
`build/mozc-arch-fs5g3r/`. The GDB backtrace and field-name/window-movement controls
are in `build/mozc-diagnosis/`.

Native Wayland compositor integration, global desktop shortcuts, browser input,
other distribution builds, and installation of distributable Nimf packages were
not tested. The default m17n exclusion does not establish that its legacy tests
pass when explicitly enabled.
