#!/bin/bash
set -euo pipefail
read -r -a flags <<< "$(pkg-config --cflags --libs gio-unix-2.0 x11 xtst)"
cc -g -O2 -Wall -Wextra -Werror /src/tests/latency/driver.c \
    -o /usr/local/bin/nimf-latency-driver "${flags[@]}"
for version in 3 4; do
    package=gtk+-3.0
    if [[ $version == 4 ]]; then package=gtk4; fi
    read -r -a flags <<< "$(pkg-config --cflags --libs "$package")"
    cc -g -O2 -Wall -Wextra -Werror /src/tests/latency/gtk-probe.c \
        -o "/usr/local/bin/nimf-latency-gtk$version" "${flags[@]}"
done
