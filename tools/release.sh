#!/bin/sh
# tools/release.sh VERSION OUTDIR
#
# Builds a release's images from nothing but this tree and the network, as
# .github/workflows/release.yml does for every tag:
#
#   OUTDIR/kestrel-VERSION.iso                  the ISO (BIOS + UEFI)
#   OUTDIR/kestrel-VERSION-usb.img.xz           the small USB stick (IceWM, xterm)
#   OUTDIR/kestrel-VERSION-firefox-usb.img.xz   the Firefox stick: LXDE, Firefox and
#                                               the benchmark entries (Kestrel, Linux)
#   OUTDIR/LICENCE.nvidia                       NVIDIA's firmware licence
#   OUTDIR/SHA256SUMS
#
# All three have nouveau built in (LKPI_NOUVEAU=1) with NVIDIA's signed
# firmware (NVIDIA_FW=, from Ubuntu's linux-firmware-nvidia-graphics).
#
# Runs as root on Ubuntu 24.04 with the packages listed in DEPS below
# (`tools/release.sh --deps` installs them). Sources go to $RELEASE_SRC
# (default build/release-src) and are reused when there:
#
# - noble's source packages of the X libraries, BusyBox, IceWM, xterm and
#   LXPanel (`apt-get source`; deb-src is turned on if it is off);
# - xorgproto 2024.1 from X.org (noble's 2023.2 is too old for XLibre);
# - XLibre at $XLIBRE_REV, the commit ports/xlibre/patches apply to;
# - Mozilla's Firefox $FIREFOX_VERSION;
# - noble's generic kernel (linux-image-unsigned and linux-modules) for the
#   benchmark's Linux entry, and linux-firmware-nvidia-graphics.
set -eu
DEPS="build-essential nasm grub-pc-bin grub-efi-amd64-bin grub-common xorriso mtools
      e2fsprogs python3 zstd xz-utils curl git ca-certificates
      musl-tools linux-libc-dev meson ninja-build pkg-config x11-xkb-utils xkb-data gettext
      autoconf automake libtool bison flex
      dpkg-dev debhelper devscripts libglib2.0-dev"
if [ "${1:-}" = --deps ]; then
    apt-get update -q
    DEBIAN_FRONTEND=noninteractive apt-get install -y -q $DEPS
    exit 0
fi
[ $# -eq 2 ] || { echo "usage: tools/release.sh VERSION OUTDIR   (as root; --deps installs the build packages)"; exit 1; }
VER=$1
mkdir -p "$2"
OUT=$(cd "$2" && pwd)
TOP=$(cd "$(dirname "$0")/.." && pwd)
cd "$TOP"
SRC=${RELEASE_SRC:-$TOP/build/release-src}
XLIBRE_REV=${XLIBRE_REV:-79dee7378eb6581c2fb26d67f86ef594b0aa373e}
FIREFOX_VERSION=${FIREFOX_VERSION:-157.0.1}
XORGPROTO=xorgproto-2024.1
JOBS=$(nproc)
mkdir -p "$SRC"
step() { echo; echo "==== $*"; }

# ---- sources -------------------------------------------------------------
step "deb-src"
if ! apt-get indextargets 2>/dev/null | grep -q '^MetaKey: .*/source/Sources'; then
    for f in /etc/apt/sources.list.d/*.sources; do
        [ -f "$f" ] && sed -i 's/^Types: deb$/Types: deb deb-src/' "$f"
    done
    [ -f /etc/apt/sources.list ] && sed -i 's/^# *deb-src /deb-src /' /etc/apt/sources.list
    apt-get update -q
fi

step "X libraries (noble's source packages, xorgproto from X.org)"
mkdir -p "$SRC/xdeps"
if [ ! -f "$SRC/xdeps/.done" ]; then
    (cd "$SRC/xdeps" && apt-get source -q --download-only xtrans libxau libxdmcp xcb-proto libxcb \
        libx11 libxkbfile libfontenc libxfont pixman zlib libxext libxrender libxfixes libxdamage \
        libxcomposite libxinerama libxrandr libxpm libpng1.6 libice libsm libxt libxmu libxaw \
        ncurses x11-xserver-utils)
    curl -fsSL -o "$SRC/xdeps/xorgproto_2024.1.orig.tar.xz" \
        "https://xorg.freedesktop.org/releases/individual/proto/$XORGPROTO.tar.xz"
    touch "$SRC/xdeps/.done"
fi

orig() {    # PACKAGE DIR: noble's upstream tarball of PACKAGE, unpacked as DIR
    [ -d "$SRC/$2" ] && return 0
    mkdir -p "$SRC/dl"
    (cd "$SRC/dl" && apt-get source -q --download-only "$1")
    t=$(ls "$SRC/dl/$1"_*.orig.tar.* | grep -v '\.asc$' | head -1)
    mkdir -p "$SRC/$2"
    tar -xf "$t" -C "$SRC/$2" --strip-components=1
}
step "BusyBox, IceWM, xterm"
orig busybox busybox
orig icewm icewm
orig xterm xterm

step "LXPanel (source package)"
if [ ! -d "$SRC/lxpanel" ]; then
    mkdir -p "$SRC/lxpanel-pkg"
    (cd "$SRC/lxpanel-pkg" && apt-get source -q lxpanel)
    mv "$(ls -d "$SRC"/lxpanel-pkg/lxpanel-*/ | head -1)" "$SRC/lxpanel"
    DEBIAN_FRONTEND=noninteractive apt-get build-dep -y -q lxpanel
fi

step "XLibre $XLIBRE_REV"
if [ ! -d "$SRC/xserver/.git" ]; then
    git clone -q --filter=blob:none https://github.com/X11Libre/xserver "$SRC/xserver"
    git -C "$SRC/xserver" checkout -q "$XLIBRE_REV"
fi

step "Firefox $FIREFOX_VERSION"
if [ ! -x "$SRC/firefox/firefox-bin" ]; then
    curl -fsSL -o "$SRC/firefox.tar.xz" \
        "https://archive.mozilla.org/pub/firefox/releases/$FIREFOX_VERSION/linux-x86_64/en-US/firefox-$FIREFOX_VERSION.tar.xz"
    rm -rf "$SRC/firefox"
    tar -xf "$SRC/firefox.tar.xz" -C "$SRC"
    rm -f "$SRC/firefox.tar.xz"
fi

step "Linux for the benchmark, NVIDIA firmware"
mkdir -p "$SRC/debs"
if [ ! -f "$SRC/debs/.done" ]; then
    kv=$(apt-cache depends linux-image-generic | sed -n 's/.*Depends: linux-image-\([0-9][^ ]*-generic\).*/\1/p' | head -1)
    [ -n "$kv" ] || { echo "release: cannot find noble's generic kernel version"; exit 1; }
    (cd "$SRC/debs" && apt-get download -q "linux-image-unsigned-$kv" "linux-modules-$kv" linux-firmware-nvidia-graphics)
    touch "$SRC/debs/.done"
fi
FW=$(ls "$SRC"/debs/linux-firmware-nvidia-graphics_*.deb | head -1)

# ---- ports ---------------------------------------------------------------
step "make busybox";  [ -f build/busybox-root/.stamp ] || make busybox BUSYBOX_SRC="$SRC/busybox"
step "make xlibre";   [ -f build/xlibre-root/.stamp ]  || MAKEJ=$JOBS make xlibre XLIBRE_SRC="$SRC/xserver" XLIBRE_DEPS="$SRC/xdeps"
step "make icewm";    [ -f build/icewm-root/.stamp ]   || make icewm ICEWM_SRC="$SRC/icewm"
step "make xterm";    [ -f build/xterm-root/.stamp ]   || make xterm XTERM_SRC="$SRC/xterm"
step "make lxpanel"
ls build/lxpanel/lxpanel_*+kestrel1_amd64.deb >/dev/null 2>&1 || make lxpanel LXPANEL_SRC="$SRC/lxpanel"

step "Linux for the benchmark (tools/mkbenchlinux.py)"
[ -f "$SRC/benchlinux/initrd.gz" ] ||
    python3 tools/mkbenchlinux.py "$SRC/benchlinux" build/busybox-root/bin/busybox \
        "$SRC"/debs/linux-image-unsigned-*.deb "$SRC"/debs/linux-modules-*.deb

step "Firefox root (tools/mkfirefoxroot.sh)"
[ -f "$SRC/ffroot.tar" ] ||
    LOCAL_DEBS="$(ls build/lxpanel/lxpanel_*+kestrel1_amd64.deb build/lxpanel/lxpanel-data_*+kestrel1_all.deb)" \
    sh tools/mkfirefoxroot.sh "$SRC/ffroot.tar" "$SRC/firefox" "$SRC/debcache"

# ---- images --------------------------------------------------------------
K="LKPI_NOUVEAU=1 NVIDIA_FW=$FW"
step "kernel"
make -j"$JOBS" $K
step "ISO"
make iso $K
cp build/kestrel.iso "$OUT/kestrel-$VER.iso"
step "USB stick"
rm -f build/rootdisk.ext2
make usbimg $K
xz -T0 -6 -c build/kestrel-usb.img > "$OUT/kestrel-$VER-usb.img.xz"
step "Firefox USB stick"
rm -f build/rootdisk.ext2
make usbimg $K ROOTFS_BASE="$SRC/ffroot.tar" ROOTDISK_MB=2400 BENCH_LINUX="$SRC/benchlinux"
xz -T0 -6 -c build/kestrel-usb.img > "$OUT/kestrel-$VER-firefox-usb.img.xz"
rm -f build/rootdisk.ext2 build/kestrel-usb.img

lic=$(find build -path '*/lib/firmware/nvidia/LICENCE.nvidia' | head -1)
[ -n "$lic" ] && cp "$lic" "$OUT/LICENCE.nvidia"
(cd "$OUT" && sha256sum kestrel-"$VER"* > SHA256SUMS && cat SHA256SUMS)
src=$(sed -n 's/^#define KESTREL_VERSION *"\(.*\)"/\1/p' include/kernel/bootinfo.h)
[ "$src" = "$VER" ] || echo "release: warning: the source says $src, the release is $VER"
