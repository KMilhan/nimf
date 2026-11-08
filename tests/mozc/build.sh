#!/bin/bash
set -euo pipefail
ulimit -c 0
cd /src

mkdir -p m4
gtkdocize
autoreconf --force --install
./configure --prefix=/usr --enable-gtk4 --disable-gtk2
make -j8
make check
make install
ldconfig
glib-compile-schemas /usr/share/glib-2.0/schemas
make update-gtk-im-cache

read -r -a flags <<< "$(pkg-config --cflags --libs gio-unix-2.0)"
cc -Wall -Wextra -Imodules/engines/nimf-mozc tests/mozc/helper-probe.c \
    modules/engines/nimf-mozc/nimf-mozc-helper.c \
    modules/engines/nimf-mozc/nimf-mozc-sexp.c \
    -o /usr/local/bin/nimf-helper-probe "${flags[@]}"

for version in 3 4; do
    package=gtk+-3.0
    if [[ $version == 4 ]]; then package=gtk4; fi
    read -r -a flags <<< "$(pkg-config --cflags --libs "$package")"
    cc -Wall -Wextra tests/mozc/gtk-probe.c -o /usr/local/bin/nimf-gtk$version-probe "${flags[@]}"
done
for version in 5 6; do
    read -r -a flags <<< "$(pkg-config --cflags --libs Qt${version}Widgets)"
    c++ -Wall -Wextra -fPIC tests/mozc/qt-probe.cpp -o /usr/local/bin/nimf-qt$version-probe "${flags[@]}"
done
useradd --create-home tester
install -d -o tester -g tester /results
