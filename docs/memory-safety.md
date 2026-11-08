# Memory ownership and validation

Nimf uses GLib ownership facilities with its existing C toolchains. Selecting
C23 alone does not give pointers ownership, bounds checks, or automatic cleanup.
The supported GLib minimum is 2.56. GCC and Clang support the cleanup extensions
used by `g_autoptr` and `g_autofree`.

## Ownership rules

- Initialize owned local resources to `NULL`. Use `g_autoptr` for objects,
  errors, messages, and results. Use `g_autofree` for allocated strings.
- Transfer ownership explicitly with `g_steal_pointer`. Do not put borrowed
  pointers in automatic cleanup variables without first acquiring a reference.
- Allocate arrays with `g_new0(Type, count)` or `g_malloc0_n`. Include the
  terminator in the element count. Avoid hand-written byte-size arithmetic.
- Use `g_clear_object` and `g_clear_pointer` for owned fields. Clearing the
  field before running its destructor makes recursive teardown predictable.
- Each input context owns its selected engine. Lists own their separate engine
  references. Removing a list entry must not invalidate the selected engine.
- Back references to servers, connections, focused contexts, and candidate
  targets are weak. Take a local strong reference before a callback or nested
  main-loop iteration can release the owner.
- Treat signal emission and synchronous protocol waits as reentrancy points.
  Hold every object needed after the call. Do not keep a collection iterator
  across such calls when callbacks can mutate that collection.
- Copy replacement values before freeing old fields when the input may alias
  those fields. Keep a local snapshot when callbacks can replace them again.
- Disconnect callbacks before releasing their receiver. Release object
  relationships in `dispose`; keep it safe to call more than once. Do not drain
  arbitrary queued callbacks while a toolkit context is being destroyed.
- Keep wire values separate from native types. Copy unaligned integers with
  `memcpy`; X11 `Window` is not the wire's 32-bit `CARD32` on 64-bit systems.
- Validate received message lengths and strings before dispatch. A disconnected
  or failed transport cancels pending waits. Shared client socket dispatch uses
  the same mutex as synchronous requests.

Reference-counted messages must always use `nimf_message_unref`, including when
they belong to a result. Never free their struct directly. The message owns its
header by value and releases its body through the supplied destroy callback.

Dynamic `GTypeModule` instances remain registered for the process lifetime.
They are not ordinary temporary module handles. GTK radio groups are borrowed
lists owned by their widgets and must not be freed by settings code.

## Checks

Run from the repository root with Docker available:

```sh
bash tests/memory/run.sh ubuntu sanitize
bash tests/memory/run.sh arch sanitize
bash tests/memory/run.sh ubuntu analyze
bash tests/memory/run.sh arch analyze
```

The default commands refresh their distribution base. Arch uses `archlinux:latest`
and a full package upgrade. `REUSE_BASE=1` reuses an existing
`nimf-mozc-<distribution>:test` image for local iteration. Reused images do not
establish that distribution packages are current.

Builds, source, and test profiles stay inside the images and containers. Runtime
containers have no network, no host display, no host input configuration, and no
Linux capabilities. Results are copied to `build/memory-<distribution>-<mode>-*/`.
The directory includes build logs, runtime logs, package versions, and screenshots.

The sanitizer mode instruments Nimf, its enabled modules, and the toolkit probes
with AddressSanitizer and UndefinedBehaviorSanitizer. LeakSanitizer runs at normal
process exit. The analysis mode runs GCC `-fanalyzer` over enabled C code and
fails on analyzer diagnostics. Both modes reject implicit function declarations,
incompatible pointer types, and invalid return types. `make check` runs in both.

The GitHub Actions memory matrix runs both modes on Ubuntu and Arch. Any failing
matrix entry fails the workflow; logs are uploaded even after failure.

| Area | Regression coverage |
| --- | --- |
| Core messages | Shared reply ownership, replaced bodies, malformed and truncated payloads, unaligned preedit data, oversized sends, cancelled waits |
| Client/server lifetime | Repeated simultaneous contexts, concurrent client threads, owner release inside commit, disconnect during composition, engine removal in both singleton modes |
| Engines | Anthy and Hangul method-list allocation/destruction; Mozc parser, helper protocol, recovery, and conversion |
| GTK3/GTK4/Qt5/Qt6 | Composition, cancellation, focus transfer, candidate clicks/paging/position, helper restart, graceful client/server destruction |
| XIM | Setup and connected/disconnected clients during server shutdown |
| Wayland | A mock compositor without the input-method global, repeated partial initialization/stop, descriptor and heap cleanup |
| Settings | Window creation, keyboard focus navigation, graceful close, schema ownership |

## Exceptions and limits

`tests/memory/lsan.supp` records two leak exceptions: Fontconfig caches and the
GTK3 status-icon constructor call tree. GTK 3.24.41 incorrectly retrieves boxed
color properties in `gtk_status_icon_color_changed`; the observed three 32-byte
allocations originate there. The constructor exception also excludes leaks in
its call tree, so it is a limitation, not proof that this path cannot leak.
There are no blanket GLib, GTK, allocator, or Nimf suppressions. Accessibility
bridge startup is disabled only in the isolated sanitizer environment.
GTK4 uses Cairo with GL/Vulkan disabled to avoid initializing unused Mesa
driver caches in Xvfb. GPU rendering and accessibility are outside this test.

The uninstrumented GTK module-cache utility needs
`verify_asan_link_order=0:detect_leaks=0` while loading instrumented modules.
That exception applies only to cache generation, not unit or runtime tests.

These checks are bug detectors, not a memory-safety proof. They do not cover
every engine input, hostile XIM packet, native Wayland composition, GTK2 runtime,
or arbitrary thread interleaving. Native Wayland composition still needs a
compatible compositor. Third-party libraries are not rebuilt with sanitizers.
Rime is built and analyzed, but disabled in the Mozc runtime profiles to avoid
unrelated first-run dictionary maintenance. Rime composition is not tested here.
m17n is excluded from the default build and this implementation scope.
