# Cursor location check

Run it against a built benchmark image:

```sh
bash tests/cursor/run.sh nimf-latency-ubuntu:test
```

A GTK3 and a GTK4 program create an input context, report the same cursor area
twice, move their window through X, and report that area twice again. The Nimf
module must send one cursor report before the move and one after it. The check
counts the reports in the module log and fails when a toolkit sends fewer.

A cached root window origin that ignores a move makes the second report
disappear, so the count drops to 1. The programs leave a one second gap after the
move, so the check rejects a cache that never notices a move and accepts a cache
that expires sooner than that gap. The GTK4 module takes the second route: GTK4
delivers no move event to the widget, so it re-resolves on a timer documented in
modules/clients/gtk/im-nimf-gtk4.c.

The container needs Xvfb, GTK3, and GTK4, which the benchmark images already
have. It runs without network. Artifacts land in build/cursor-<rand>/, with the
per toolkit counts in summary.txt.
