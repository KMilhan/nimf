# Isolated Mozc checks

Run from the repository root:

```sh
bash tests/mozc/run.sh ubuntu
bash tests/mozc/run.sh arch
```

Docker must be available. The first build downloads development packages and
Mozc. If Docker's bridge cannot reach package mirrors, set `BUILD_NETWORK=host`.
This changes build networking only.

Each run writes logs, package/toolkit versions, image metadata, entry contents,
and screenshots to a new `build/mozc-<distribution>-*` directory. No host package
installation or input-method configuration is required. The runtime container
has no network, host mounts, display sockets, or session bus access. It runs as
an ordinary container user with a private home, runtime directory, D-Bus session,
and Xvfb display. Each toolkit also gets a separate Mozc profile and session bus.
Docker's `--init` reaps children and lets Xvfb's readiness handshake work.

The image build runs the default `make check`, including the fake-helper unit
tests and both Mozc candidate field formats. m17n is disabled by default and is
outside this validation scope. The GUI
test sends actual X11 key events to GTK3, GTK4, Qt5, and Qt6 entry widgets. It
checks Japanese conversion, Backspace, cancellation, focus transfer, and recovery
after terminating the helper. It checks candidate presence, placement relative to
the app before and after moving the window and switching entries. GTK4 uses a
header bar to exercise the native surface decoration offset. The test also checks
mouse selection followed by Enter and a different commit after
PageDown. Screenshots record preedit, candidates, and committed text. Inspect them
for cursor alignment and styling. The placement assertion is specific to this
controlled display, not a general desktop geometry rule. Shutdown failures also
make the run fail. Separate checks use only the system keyboard and exercise shutdown without a
client, after closing a client, and while a client remains connected.
Independent toolkits continue after a failure.

Ubuntu uses its `emacs-mozc-bin` package. Arch starts from `archlinux:latest` and
runs `pacman -Syu`. Its official `fcitx5-mozc` package does not include the Emacs
helper. The Arch test image builds the helper and server from the same upstream
revision used by [AUR emacs-mozc](https://aur.archlinux.org/packages/emacs-mozc).
The revision is pinned in `Dockerfile.arch`; changing the base distribution does
not silently change the helper protocol under test.

These checks cover X11 toolkit input modules. They do not validate native
Wayland compositor integration, global desktop shortcuts, browser-specific
input behavior, or installation of the distributable Nimf packages.
