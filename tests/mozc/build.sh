#!/bin/bash
set -euo pipefail
ulimit -c 0
cd /src

mkdir -p m4
gtkdocize
autoreconf --force --install
./configure --prefix=/usr --enable-gtk4 --disable-gtk2
make -j8
if ! make check; then
    cat libnimf/test-suite.log modules/engines/nimf-mozc/test-suite.log 2>/dev/null || true
    exit 1
fi
make install
ldconfig
glib-compile-schemas /usr/share/glib-2.0/schemas
# GTK's cache tool loads the instrumented module without an instrumented main.
ASAN_OPTIONS=verify_asan_link_order=0:detect_leaks=0 make update-gtk-im-cache

read -r -a cflags <<< "${CFLAGS:-}"
read -r -a cxxflags <<< "${CXXFLAGS:-}"
read -r -a ldflags <<< "${LDFLAGS:-}"
read -r -a flags <<< "$(pkg-config --cflags --libs gio-unix-2.0)"
"${CC:-cc}" "${cflags[@]}" -Wall -Wextra -Imodules/engines/nimf-mozc tests/mozc/helper-probe.c \
    modules/engines/nimf-mozc/nimf-mozc-helper.c \
    modules/engines/nimf-mozc/nimf-mozc-sexp.c \
    -o /usr/local/bin/nimf-helper-probe "${flags[@]}" "${ldflags[@]}"

for version in 3 4; do
    package=gtk+-3.0
    if [[ $version == 4 ]]; then package=gtk4; fi
    read -r -a flags <<< "$(pkg-config --cflags --libs "$package")"
    "${CC:-cc}" "${cflags[@]}" -Wall -Wextra tests/mozc/gtk-probe.c -o /usr/local/bin/nimf-gtk$version-probe "${flags[@]}" "${ldflags[@]}"
done
for version in 5 6; do
    read -r -a flags <<< "$(pkg-config --cflags --libs Qt${version}Widgets)"
    "${CXX:-c++}" "${cxxflags[@]}" -Wall -Wextra -fPIC tests/mozc/qt-probe.cpp -o /usr/local/bin/nimf-qt$version-probe "${flags[@]}" "${ldflags[@]}"
done
if ! id tester >/dev/null 2>&1; then useradd --create-home tester; fi
install -d -o tester -g tester /results
