#!/bin/sh
# ports/xorg/build.sh XLIBRE_SRC EVDEV_SRC DEPS BUILD
#
# Build XLibre's Xorg server, with its modesetting driver and GLX, and the
# evdev input driver, for the Firefox image's Ubuntu root (glibc), and
# install them into BUILD/xorg-root laid out like that root:
#
#   usr/bin/Xorg                                  the X server
#   usr/lib/xorg/modules/xlibre-25/...            modesetting, glamor, GLX, evdev
#   usr/lib/x86_64-linux-gnu/xorg/protocol.txt    request names for the log
#   etc/X11/kestrel-xorg.conf                     the layout /bin/startx uses
#
# tools/mkfirefoxroot.sh copies it in when XORG_ROOT names it. /bin/startx
# then runs Xorg on /dev/dri/card0 where there is one, and Xkestrel elsewhere.
#
# XLIBRE_SRC  a checkout of https://github.com/X11Libre/xserver (its commit
#             is built, not the working tree, which ports/xlibre patches)
# EVDEV_SRC   a checkout of https://github.com/X11Libre/xf86-input-evdev
# DEPS        directory with xorgproto_2025.1.orig.tar.gz (noble's xorgproto
#             is older than XLibre needs), from archive.ubuntu.com's pool
# BUILD       Kestrel's build directory (the trees go to BUILD/xorg)
#
# Runs on an Ubuntu 24.04 host with meson, ninja, autoconf, automake,
# libtool, xutils-dev and the -dev packages: libpixman-1-dev libxkbfile-dev
# libxfont-dev libdrm-dev libpciaccess-dev libgbm-dev libepoxy-dev
# libegl-dev libgl1-mesa-dev mesa-common-dev libxshmfence-dev libxcvt-dev
# libbsd-dev libmd-dev libxau-dev libevdev-dev libmtdev-dev.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
XSRC=$(cd "$1" && pwd)
ESRC=$(cd "$2" && pwd)
DEPS=$(cd "$3" && pwd)
mkdir -p "$4"
BUILD=$(cd "$4" && pwd)
OUT=$BUILD/xorg
DEST=$OUT/dest
ROOT=$BUILD/xorg-root
rm -rf "${OUT:?}"
mkdir -p "$OUT/proto-src" "$OUT/xserver-src" "$OUT/evdev-src"

echo "  PROTO   xorgproto"
PROTO=$(ls "$DEPS"/xorgproto_2025*.orig.tar.* | head -1)
tar -xf "$PROTO" -C "$OUT/proto-src" --strip-components=1
meson setup "$OUT/proto" "$OUT/proto-src" --prefix="$OUT/proto-root" >/dev/null
ninja -C "$OUT/proto" install >/dev/null
export PKG_CONFIG_PATH="$OUT/proto-root/share/pkgconfig"

echo "  MESON   xserver"
git -C "$XSRC" archive HEAD | tar -x -C "$OUT/xserver-src"
meson setup "$OUT/xserver" "$OUT/xserver-src" -Dbuildtype=release -Dprefix=/usr \
    -Dlibdir=lib/x86_64-linux-gnu -Dsysconfdir=/etc -Dlocalstatedir=/var \
    -Dxorg=true -Dxephyr=false -Dxnest=false -Dxvfb=false -Dxfbdev=false -Dxmodesetting=false -Dxfake=false \
    -Dglamor=true -Dglx_dri=true -Ddri2=true -Ddri3=true -Dgbm=true -Ddri1=false \
    -Dudev=false -Dudev_kms=false -Dhal=false -Dsystemd_logind=false -Dseatd_libseat=false \
    -Dsystemd_notify=false -Dsuid_wrapper=false -Dpciaccess=true -Dint10=false -Dxselinux=false \
    -Dlinux_apm=false -Dlinux_acpi=false -Ddocs=false -Ddevel-docs=false -Dtests=false \
    -Dlisten_tcp=false -Dmodule_dir=/usr/lib/xorg/modules -Dxkb_dir=/usr/share/X11/xkb \
    -Dxkb_output_dir=/var/lib/xkb -Dlog_dir=/var/log -Dxdmcp=false -Dxdm-auth-1=false >"$OUT/meson.log"
ninja -C "$OUT/xserver" >"$OUT/ninja.log"
DESTDIR=$DEST meson install -C "$OUT/xserver" --no-rebuild >/dev/null

echo "  BUILD   xf86-input-evdev"
git -C "$ESRC" archive HEAD | tar -x -C "$OUT/evdev-src"
(cd "$OUT/evdev-src" &&
 PKG_CONFIG_PATH="$DEST/usr/lib/x86_64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH" &&
 export PKG_CONFIG_PATH &&
 autoreconf -fi >/dev/null 2>&1 &&
 ./configure -q --prefix=/usr CPPFLAGS="-I$DEST/usr/include/xorg" >"$OUT/evdev.log" &&
 make -s >>"$OUT/evdev.log" 2>&1)

rm -rf "${ROOT:?}"
M=usr/lib/xorg/modules
mkdir -p "$ROOT/usr/bin" "$ROOT/$M" "$ROOT/usr/lib/x86_64-linux-gnu/xorg" "$ROOT/etc/X11"
strip -o "$ROOT/usr/bin/Xorg" "$DEST/usr/bin/Xorg"
cp -r "$DEST/$M/." "$ROOT/$M/"
V=$(cd "$ROOT/$M" && ls -d xlibre-* | head -1)
strip -o "$ROOT/$M/$V/input/evdev_drv.so" "$OUT/evdev-src/src/.libs/evdev_drv.so"
find "$ROOT/$M" -name '*.so' -exec strip --strip-unneeded {} +
cp "$DEST/usr/lib/x86_64-linux-gnu/xorg/protocol.txt" "$ROOT/usr/lib/x86_64-linux-gnu/xorg/"
cp "$HERE/kestrel-xorg.conf" "$ROOT/etc/X11/"
touch "$ROOT/.stamp"
echo "xorg: installed into $ROOT"
