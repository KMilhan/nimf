# Memory safety validation

Validated on 2026-11-08 before publication. The final product-source hashes are
included in [product-source.sha256](product-source.sha256).

| Check | Ubuntu 24.04 | Current Arch |
|---|---|---|
| ASan, UBSan, and scoped leak checks | Pass | Pass |
| GCC static analyzer | Pass | Pass |
| Core memory tests | 7/7 | 7/7 |
| Mozc unit cases | 12/12 | 12/12 |
| GTK3/GTK4/Qt5/Qt6 integration | Pass | Pass |

Runtime checks covered conversion, candidates, helper recovery, graceful server
shutdown, repeated and concurrent input contexts, callback-owner destruction,
preedit toggles, engine unloading, interrupted peers, settings shutdown, and
Wayland partial-start cleanup. Arch used GCC 16.2.1, GTK 3.24.52 / 4.24.1,
and Qt 5.15.19 / 6.12.0. Its package database had no pending updates at validation.

Independent Claude reviews confirmed and resolved a closed-socket dispatch loop,
monitor/getter lifetime issues, and aliased preedit inputs. Focused follow-ups
approved the fixes. A GTK API version concern was rejected against GTK 4.0's
header. Timed-out review attempts were not counted as completed reviews.

The four full local runs were `memory-ubuntu-sanitize-W88lNR`,
`memory-arch-sanitize-w9B7LG`, `memory-ubuntu-analyze-69JOLk`, and
`memory-arch-analyze-3wNRIP`. Their original diagnostics remain in the local
ignored `build/` tree. These are local results, not proof of a GitHub Actions run.

Native Wayland composition, GTK2 runtime, and Rime composition were not exercised.
Fontconfig and GTK3 status-icon allocations have documented leak exceptions.
The legacy m17n engine is excluded from this scope.

Reproduce with the commands in [memory ownership and validation](../../docs/memory-safety.md).
