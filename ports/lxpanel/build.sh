#!/bin/sh
# ports/lxpanel/build.sh LXPANEL_SRC BUILD
#
# Ubuntu 24.04's LXPanel with patches/*.patch, as a Debian package for
# tools/mkfirefoxroot.sh (LOCAL_DEBS=), which unpacks it over the archive's:
#
#   BUILD/lxpanel/lxpanel_<version>+kestrel1_amd64.deb
#
# LXPANEL_SRC  Ubuntu's unpacked source package (`apt-get source lxpanel`:
#              lxpanel-0.10.1 with its debian/ directory); it is copied, not
#              changed
# BUILD        Kestrel's build directory
#
# Host needs: dpkg-dev, debhelper and lxpanel's build dependencies
# (`apt-get build-dep lxpanel`, with deb-src entries in the sources).
#
# The patch: noble's LXPanel 0.10.1 shrinks a full-width panel to its
# plugins' width under GTK 3.24 (0001; lxpanel's git master has the fix).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$1" && pwd)
mkdir -p "$2"
BUILD=$(cd "$2" && pwd)
OUT=$BUILD/lxpanel
WORK=$OUT/src

[ -f "$SRC/debian/changelog" ] || { echo "lxpanel: $SRC is not an unpacked Debian source package"; exit 1; }
rm -rf "$OUT"
mkdir -p "$OUT"
cp -a "$SRC" "$WORK"
for p in "$HERE"/patches/*.patch; do
    cp "$p" "$WORK/debian/patches/kestrel-$(basename "$p")"
    echo "kestrel-$(basename "$p")" >> "$WORK/debian/patches/series"
    (cd "$WORK" && patch -p1 -s < "$p")
done
# a version of its own, newer than the archive's
v=$(dpkg-parsechangelog -l "$WORK/debian/changelog" -S Version)
{
    printf 'lxpanel (%s+kestrel1) noble; urgency=medium\n\n' "$v"
    printf '  * Kestrel: %s\n' $(cd "$HERE/patches" && ls *.patch)
    printf '\n -- Kestrel <kestrel@localhost>  %s\n\n' "$(date -R)"
    cat "$WORK/debian/changelog"
} > "$WORK/debian/changelog.new"
mv "$WORK/debian/changelog.new" "$WORK/debian/changelog"
echo "  DPKG    lxpanel $v+kestrel1"
(cd "$WORK" && dpkg-buildpackage -b -uc -us -j"${MAKEJ:-8}") > "$OUT/build.log" 2>&1 \
    || { tail -30 "$OUT/build.log"; exit 1; }
ls "$OUT"/lxpanel_*+kestrel1_*.deb
