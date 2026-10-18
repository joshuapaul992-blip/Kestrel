#!/bin/sh
# tools/mkfirefoxroot.sh OUT.tar FIREFOX_DIR [DEBCACHE]
# LOCAL_DEBS="build/lxpanel/lxpanel_*+kestrel1_amd64.deb" tools/mkfirefoxroot.sh ...
#
# Build a root file system of Ubuntu 24.04 (noble) runtime packages with
# Firefox in /opt/firefox, as a tarball for
#
#   make usbimg ROOTFS_BASE=OUT.tar ROOTDISK_MB=2400
#
# Runs on an Ubuntu 24.04 host (apt-get download with noble's sources) as
# root (chroot runs the cache tools), with gcc and libglib2.0-dev for
# kestrel-logind. FIREFOX_DIR is an unpacked Linux
# x86_64 Firefox: the directory with firefox-bin, libxul.so and omni.ja
# (Mozilla's tarball, or the conda-forge package's bin/FirefoxApp).
# LOCAL_DEBS lists packages built here, unpacked over the archive's: the
# patched LXPanel from ports/lxpanel/build.sh, without which the panel only
# spans its plugins' width.
#
# What the root holds:
# - glibc, bash, coreutils and the usual tools;
# - GTK 3, GLib, Pango, cairo, fontconfig with the DejaVu fonts (all of
#   them: Sans, Serif and Mono, their Condensed and ExtraLight faces), Mesa (llvmpipe
#   through EGL), the X client libraries and tools (xdpyinfo, xeyes,
#   glxinfo/eglinfo), D-Bus, gtk3-demo, and Xvfb (for /bin/kestrel-bench,
#   the same X server on Kestrel and on Linux);
# - sound: ALSA with its OSS plugin and an asound.conf that sends ALSA
#   programs to Kestrel's /dev/dsp and /dev/mixer, and PulseAudio with
#   module-oss on /dev/dsp (/etc/pulse/kestrel.pa), because Firefox's
#   official builds play only through PulseAudio (or JACK);
# - bash as root's login shell, and halt, reboot, poweroff and shutdown
#   that work without systemd;
# - /usr/local/bin/firefox, which starts PulseAudio if it is not running
#   and Firefox with the sandbox off (Kestrel has no seccomp or namespaces
#   yet), and an IceWM menu for root;
# - the LXDE desktop, which /bin/startx runs when it finds startlxde:
#   Openbox, LXPanel, LXSession, the PCManFM file manager (and desktop),
#   Xarchiver, LXRandR (monitor settings, through xrandr), LXTerminal,
#   LXAppearance and LXTask, with a panel that has Firefox on it;
# - kestrel-logind (ports/logind), which answers the logout dialog's
#   Shutdown and Reboot in systemd-logind's place, and the D-Bus system
#   bus configuration /bin/startx starts it on.
set -e
OUT=$1; FF=$2; CACHE=${3:-$(dirname "$OUT")/debcache}
[ -n "$OUT" ] && [ -f "$FF/firefox-bin" ] || { echo "usage: $0 OUT.tar FIREFOX_DIR [DEBCACHE]"; exit 2; }
SRCDIR=$(cd "$(dirname "$0")/.." && pwd)
ROOT=$(mktemp -d "${TMPDIR:-/tmp}/ffroot.XXXXXX")
trap 'rm -rf "${ROOT:?}"' EXIT

PKGS="bash dash coreutils findutils grep sed gawk procps util-linux xz-utils zstd less
      libgtk-3-0t64 libgtk-3-bin gtk-3-examples libasound2t64 alsa-utils libdbus-1-3 dbus-daemon dbus-x11
      libgl1-mesa-dri libegl1 libegl-mesa0 mesa-utils x11-utils x11-apps x11-xserver-utils
      fonts-dejavu-core fonts-dejavu-extra fontconfig hicolor-icon-theme adwaita-icon-theme librsvg2-common shared-mime-info
      libpci3 libx11-xcb1 libxcomposite1 libxdamage1 libxrandr2 libxcursor1 libxi6 libxtst6 libxss1
      libdbus-glib-1-2 pulseaudio pulseaudio-utils libpulse0 libasound2-plugins xvfb"
# The LXDE desktop (lxde-core without the screen locker), its theme and
# icons, and the archivers Xarchiver drives
LXDE_PKGS="openbox-lxde-session lxpanel pcmanfm xarchiver lxrandr lxterminal lxappearance lxtask
      lxde-icon-theme clearlooks-phenix-theme gtk2-engines gzip bzip2 unzip zip"

echo "mkfirefoxroot: resolving packages"
mkdir -p "$CACHE"
pk=$(apt-cache depends --recurse --no-recommends --no-suggests --no-conflicts --no-breaks \
         --no-replaces --no-enhances $PKGS 2>/dev/null | grep -E '^[a-zA-Z0-9]' | sed 's/:any$//' | sort -u)
real=""
for p in $pk; do apt-cache show --no-all-versions "$p" >/dev/null 2>&1 && real="$real $p"; done
# LXDE through apt's own solver against an empty system, which takes the
# first of each "a | b" alternative: apt-cache's recursion above follows
# every alternative, and lxsession's "polkit-1-auth-agent" alone would pull
# in GNOME, KDE and LXQt.
: > "$CACHE/empty.status"
lx=$(apt-get -s -q -o Dir::State::status="$CACHE/empty.status" -o APT::Install-Recommends=false \
         install $LXDE_PKGS 2>/dev/null | awk '/^Inst /{print $2}' | sed 's/:amd64$//')
[ -n "$lx" ] || { echo "mkfirefoxroot: apt could not resolve the LXDE packages"; exit 1; }
real=$(echo $real $lx | tr ' ' '\n' | sort -u)
echo "mkfirefoxroot: downloading $(echo $real | wc -w) packages into $CACHE"
(cd "$CACHE" && miss=""
 for p in $real; do ls "${p}"_*.deb >/dev/null 2>&1 || miss="$miss $p"; done
 # all at once (in parallel), else one by one to find the ones that fail
 [ -z "$miss" ] || apt-get download -q $miss >/dev/null 2>&1 ||
     for p in $miss; do ls "${p}"_*.deb >/dev/null 2>&1 || apt-get download -q "$p" >/dev/null 2>&1 || echo "  skipped $p"; done)

echo "mkfirefoxroot: unpacking"
mkdir -p "$ROOT/usr/bin" "$ROOT/usr/sbin" "$ROOT/usr/lib" "$ROOT/usr/lib64"
for d in bin sbin lib lib64; do ln -s usr/$d "$ROOT/$d"; done     # merged /usr, before any package
for p in $real; do
    for d in "$CACHE/${p}"_*.deb; do
        [ -f "$d" ] || continue
        dpkg-deb --fsys-tarfile "$d" | tar -x --keep-directory-symlink -C "$ROOT" 2>/dev/null || echo "  warning: $(basename "$d")"
    done
done

for d in ${LOCAL_DEBS:-}; do
    echo "mkfirefoxroot: $(basename "$d") over the archive's"
    dpkg-deb --fsys-tarfile "$d" | tar -x --keep-directory-symlink -C "$ROOT"
done
case " ${LOCAL_DEBS:-} " in
*/lxpanel_*) ;;
*) echo "mkfirefoxroot: warning: no LXPanel from ports/lxpanel in LOCAL_DEBS; the panel will be narrow" ;;
esac

echo "mkfirefoxroot: configuring"
[ -e "$ROOT/usr/bin/sh" ] || ln -s dash "$ROOT/usr/bin/sh"            # dpkg would make it
# what update-alternatives would do; mawk rather than gawk, whose build
# is not PIE (Kestrel does not load fixed-address programs yet)
[ -e "$ROOT/usr/bin/awk" ] || ln -s mawk "$ROOT/usr/bin/awk"
cat > "$ROOT/etc/asound.conf" <<'EOF'
# ALSA programs (aplay, speaker-test) play through Kestrel's OSS devices
pcm.!default { type oss device /dev/dsp }
ctl.!default { type oss device /dev/mixer }
EOF
cat > "$ROOT/etc/pulse/kestrel.pa" <<'EOF'
# PulseAudio on Kestrel: one sink, the kernel's OSS device (HD Audio)
load-module module-oss device=/dev/dsp sink_name=kestrel playback=1 record=0 mmap=0
load-module module-native-protocol-unix auth-anonymous=1
load-module module-always-sink
set-default-sink kestrel
EOF
sed -i 's/^; autospawn = yes/autospawn = no/' "$ROOT/etc/pulse/client.conf"
# root logs in to bash (the kernel takes root's shell from /etc/passwd);
# /bin/sh stays dash
printf '%s\n' 'root:x:0:0:root:/root:/bin/bash' 'user:x:1000:1000:Kestrel User:/home/user:/bin/bash' \
    > "$ROOT/etc/passwd"
# halt, reboot, poweroff and shutdown are systemctl here, which needs
# systemd: BusyBox's instead (Kestrel's, which tools/mergeroot.py adds)
for c in halt reboot poweroff; do
    rm -f "$ROOT/usr/sbin/$c"
    printf '#!/bin/sh\n# no systemd on Kestrel: BusyBox %s, straight to the kernel\nexec busybox %s -f "$@"\n' \
        $c $c > "$ROOT/usr/sbin/$c"
done
rm -f "$ROOT/usr/sbin/shutdown"
cat > "$ROOT/usr/sbin/shutdown" <<'EOF'
#!/bin/sh
# shutdown [-r|-H|-P] [TIME]: no systemd on Kestrel, so at once, by BusyBox
case " $* " in
*" -r "*|*" --reboot "*) exec busybox reboot -f ;;
*" -H "*|*" --halt "*)   exec busybox halt -f ;;
esac
exec busybox poweroff -f
EOF
chmod 755 "$ROOT/usr/sbin/halt" "$ROOT/usr/sbin/reboot" "$ROOT/usr/sbin/poweroff" "$ROOT/usr/sbin/shutdown"
python3 -c "import uuid; print(uuid.uuid4().hex)" > "$ROOT/etc/machine-id"
mkdir -p "$ROOT/var/lib/dbus" "$ROOT/proc" "$ROOT/sys" "$ROOT/dev" "$ROOT/tmp" "$ROOT/root"
cp "$ROOT/etc/machine-id" "$ROOT/var/lib/dbus/machine-id"
# Fonts are hinted by FreeType's TrueType bytecode interpreter version 35
# (the classic one; Ubuntu's FreeType defaults to 40), with full hinting:
# slight hinting, Ubuntu's default, would hand TrueType fonts to the
# autohinter and the interpreter would never run. FREETYPE_PROPERTIES is
# read by every program's FreeType; /bin/startx sets it for the desktop,
# and login shells get it from /etc/profile.d.
rm -f "$ROOT"/etc/fonts/conf.d/10-hinting-*.conf
ln -s /usr/share/fontconfig/conf.avail/10-hinting-full.conf "$ROOT/etc/fonts/conf.d/10-hinting-full.conf"
mkdir -p "$ROOT/etc/profile.d"
echo 'export FREETYPE_PROPERTIES="truetype:interpreter-version=35"' > "$ROOT/etc/profile.d/freetype.sh"
echo 'FREETYPE_PROPERTIES="truetype:interpreter-version=35"' >> "$ROOT/etc/environment"
chroot "$ROOT" /usr/bin/fc-cache -f
chroot "$ROOT" /usr/lib/x86_64-linux-gnu/glib-2.0/glib-compile-schemas /usr/share/glib-2.0/schemas
chroot "$ROOT" /usr/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders --update-cache
chroot "$ROOT" /usr/bin/update-mime-database /usr/share/mime
for t in hicolor Adwaita nuoveXT2; do
    [ -d "$ROOT/usr/share/icons/$t" ] && chroot "$ROOT" /usr/bin/gtk-update-icon-cache -f -q /usr/share/icons/$t || true
done

echo "mkfirefoxroot: Firefox from $FF"
mkdir -p "$ROOT/opt"
cp -a "$FF" "$ROOT/opt/firefox"
mkdir -p "$ROOT/opt/firefox/distribution"
cat > "$ROOT/opt/firefox/distribution/policies.json" <<'EOF'
{
  "policies": {
    "SkipTermsOfUse": true,
    "DisableTelemetry": true,
    "DontCheckDefaultBrowser": true,
    "OverrideFirstRunPage": "",
    "OverridePostUpdatePage": "",
    "DisableAppUpdate": true
  }
}
EOF
cat > "$ROOT/opt/firefox/defaults/pref/kestrel.js" <<'EOF'
// Kestrel: let Web Audio start without a click, and skip first-run prompts
pref("media.autoplay.block-webaudio", false);
pref("datareporting.policy.dataSubmissionPolicyBypassNotification", true);
pref("browser.shell.checkDefaultBrowser", false);
pref("browser.aboutwelcome.enabled", false);
EOF
mkdir -p "$ROOT/usr/local/bin"
cat > "$ROOT/usr/local/bin/firefox" <<'EOF'
#!/bin/sh
# Firefox on Kestrel: its sandboxes need seccomp-bpf and namespaces, which
# Kestrel does not have yet, so they are off.
export MOZ_DISABLE_CONTENT_SANDBOX=1 MOZ_DISABLE_GMP_SANDBOX=1 MOZ_DISABLE_RDD_SANDBOX=1 \
       MOZ_DISABLE_SOCKET_PROCESS_SANDBOX=1 MOZ_DISABLE_UTILITY_SANDBOX=1 MOZ_DISABLE_GPU_SANDBOX=1 \
       MOZ_CRASHREPORTER_DISABLE=1
export DISPLAY=${DISPLAY:-:0}
[ "$HOME" = / ] && [ "$(id -u)" = 0 ] && export HOME=/root     # kestrel.exec programs start with HOME=/
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/tmp/runtime-$(id -u)}
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
pactl info >/dev/null 2>&1 ||
    pulseaudio --daemonize=yes -n -F /etc/pulse/kestrel.pa --exit-idle-time=-1 --log-target=syslog 2>/dev/null
exec /opt/firefox/firefox-bin "$@"
EOF
chmod 755 "$ROOT/usr/local/bin/firefox"
mkdir -p "$ROOT/root/.icewm"
cat > "$ROOT/root/.icewm/menu" <<'EOF'
prog "Firefox" - /usr/local/bin/firefox
prog "XTerm" xterm xterm
prog "GTK 3 demo" - gtk3-demo
prog "xeyes" - xeyes
EOF
cp "$ROOT/root/.icewm/menu" "$ROOT/root/.icewm/toolbar"
ln -sfn root/.icewm "$ROOT/.icewm"                  # the desktop started at boot runs with HOME=/

echo "mkfirefoxroot: LXDE"
# What update-alternatives would set up: the terminal and browser that
# LXDE's menus and PCManFM's "Open in terminal" call, and the wallpaper
ln -sf lxterminal "$ROOT/usr/bin/x-terminal-emulator"
ln -sf /usr/local/bin/firefox "$ROOT/usr/bin/x-www-browser"
mkdir -p "$ROOT/etc/alternatives"
ln -sf /usr/share/lxde/wallpapers/lxde_blue.jpg "$ROOT/etc/alternatives/desktop-background"
cat > "$ROOT/usr/share/applications/firefox.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Firefox
GenericName=Web Browser
Comment=Browse the Web
Exec=/usr/local/bin/firefox %u
Icon=/opt/firefox/browser/chrome/icons/default/default128.png
Terminal=false
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
StartupWMClass=firefox
EOF
cat > "$ROOT/usr/share/applications/xterm.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=XTerm
Comment=Standard terminal emulator for the X window system
Exec=xterm
Icon=utilities-terminal
Terminal=false
Categories=System;TerminalEmulator;
EOF
# The session: Openbox, the panel and PCManFM's desktop. No screen saver
# or locker, and no autostart entries for services Kestrel lacks: lxpolkit
# needs polkitd on a system bus, light-locker needs LightDM, and PulseAudio's
# own entry would start it without Kestrel's OSS sink (/usr/local/bin/firefox
# starts that one when it is needed).
printf '%s\n' '@lxpanel --profile LXDE' '@pcmanfm --desktop --profile LXDE' > "$ROOT/etc/xdg/lxsession/LXDE/autostart"
sed -i 's/^sNet\/ThemeName=.*/sNet\/ThemeName=Clearlooks-Phenix/' "$ROOT/etc/xdg/lxsession/LXDE/desktop.conf"
# lxsession starts lxpolkit unless told otherwise, and lxpolkit, finding
# no polkit session, shows an error box (again whenever it is closed). A
# name lxsession does not know makes it start no agent at all.
sed -i 's/^window_manager=.*/&\npolkit\/command=none/' "$ROOT/etc/xdg/lxsession/LXDE/desktop.conf"
# full hinting in GTK's settings too (XSETTINGS override fontconfig), so
# the TrueType interpreter set below is what draws the text
sed -i 's/^iXft\/Antialias=1$/iXft\/Antialias=1\niXft\/Hinting=1\nsXft\/HintStyle=hintfull\nsXft\/RGBA=none/' \
    "$ROOT/etc/xdg/lxsession/LXDE/desktop.conf"
# The logout dialog offers Shutdown and Reboot only when systemd-logind on
# the system bus says they are allowed: kestrel-logind takes its name there
# and calls reboot(2), on a system bus of its own configuration (no
# messagebus user, no services started on demand), both started by startx.
mkdir -p "$ROOT/usr/libexec" "$ROOT/etc/dbus-1"
gcc -O2 -o "$ROOT/usr/libexec/kestrel-logind" "$SRCDIR/ports/logind/kestrel-logind.c" \
    $(pkg-config --cflags --libs gio-2.0)
cp "$SRCDIR/ports/logind/system-bus.conf" "$ROOT/etc/dbus-1/kestrel-system.conf"
for f in lxpolkit light-locker pulseaudio at-spi-dbus-bus xdg-user-dirs; do rm -f "$ROOT/etc/xdg/autostart/$f.desktop"; done
# The logout dialog offers Lock Screen when it finds lxlock, which would
# hand the screen to light-locker, and that needs LightDM.
rm -f "$ROOT/usr/bin/lxlock"
rm -f "$ROOT/usr/share/applications/python3.12.desktop"   # non-PIE: Kestrel cannot load it yet
# xrdb, which lxsession runs for every settings change, passes resources
# through cpp when it finds /usr/bin/cpp; GCC's cpp is not PIE either, so
# it fails each time. Without it xrdb reads them as they are.
rm -f "$ROOT/usr/bin/cpp"
sed -i 's|^wallpaper=.*|wallpaper=/usr/share/lxde/wallpapers/lxde_blue.jpg|' "$ROOT/etc/xdg/pcmanfm/LXDE/pcmanfm.conf"
sed -i 's|^terminal=.*|terminal=lxterminal -e %s|' "$ROOT/etc/xdg/libfm/libfm.conf"
# The panel along the bottom of the first monitor: menu, Firefox, PCManFM
# and LXTerminal, the task list, CPU, tray, clock and logout. Kestrel has
# no screen locker, so LXDE's lock button is left out.
cat > "$ROOT/etc/xdg/lxpanel/LXDE/panels/panel" <<'EOF'
# lxpanel <profile> config file. Manually editing is not recommended.
# Use preference dialog in lxpanel to adjust config when you can.

Global {
    edge=bottom
    align=left
    margin=0
    widthtype=percent
    width=100
    height=26
    transparent=0
    tintcolor=#000000
    alpha=0
    setdocktype=1
    setpartialstrut=1
    autohide=0
    heightwhenhidden=0
    usefontcolor=1
    fontcolor=#ffffff
    background=1
    backgroundfile=/usr/share/lxpanel/images/background.png
    monitor=0
}

Plugin {
    type=space
    Config {
        Size=2
    }
}

Plugin {
    type=menu
    Config {
        image=/usr/share/lxde/images/lxde-icon.png
        system {
        }
        separator {
        }
        item {
            command=run
        }
        separator {
        }
        item {
            image=gnome-logout
            command=logout
        }
    }
}

Plugin {
    type=launchbar
    Config {
        Button {
            id=firefox.desktop
        }
        Button {
            id=pcmanfm.desktop
        }
        Button {
            id=lxterminal.desktop
        }
    }
}

Plugin {
    type=space
    Config {
        Size=4
    }
}

Plugin {
    type=wincmd
    Config {
        Button1=iconify
        Button2=shade
    }
}

Plugin {
    type=space
    Config {
        Size=4
    }
}

Plugin {
    type=pager
}

Plugin {
    type=space
    Config {
        Size=4
    }
}

Plugin {
    type=taskbar
    expand=1
    Config {
        tooltips=1
        IconsOnly=0
        AcceptSkipPager=1
        ShowIconified=1
        ShowMapped=1
        ShowAllDesks=0
        UseMouseWheel=1
        UseUrgencyHint=1
        FlatButton=0
        MaxTaskWidth=150
        spacing=1
    }
}

Plugin {
    type=cpu
}

Plugin {
    type=tray
}

Plugin {
    type=dclock
    Config {
        ClockFmt=%R
        TooltipFmt=%A %x
        BoldFont=0
    }
}

Plugin {
    type=launchbar
    Config {
        Button {
            id=lxde-logout.desktop
        }
    }
}
EOF

echo "mkfirefoxroot: writing $OUT"
tar -C "$ROOT" --numeric-owner -cf "$OUT" .
echo "mkfirefoxroot: $(du -sh "$ROOT" | cut -f1) root in $OUT"
