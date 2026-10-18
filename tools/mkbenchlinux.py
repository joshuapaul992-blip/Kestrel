#!/usr/bin/env python3
"""tools/mkbenchlinux.py OUTDIR BUSYBOX DEB...

Linux for the benchmark stick (make usbimg BENCH_LINUX=OUTDIR): Ubuntu
24.04's generic kernel, unmodified, and a small initramfs, so that the same
PC can run the same benchmark (/bin/kestrel-bench) on Linux and on Kestrel,
from the same root file system.

DEB  Ubuntu's linux-image-unsigned-<ver>-generic and linux-modules-<ver>-
     generic packages (`apt-get download`), any order
BUSYBOX  a static BusyBox (Kestrel's own: build/busybox-root/bin/busybox)

OUTDIR gets:
  vmlinuz     the kernel
  initrd.gz   BusyBox, the modules that reach the stick and its report
              volume (USB host controllers, usb-storage, uas, vfat), with
              what they depend on, and tools/benchlinux-init as /init: it
              finds the ext2 volume labelled kestrel-root, mounts it and
              runs /bin/kestrel-bench there as init
"""
import glob
import gzip
import os
import shutil
import stat
import subprocess
import sys
import tempfile

# What the initramfs loads, by module name; those built into the kernel are
# skipped, the rest are loaded with their dependencies first.
WANTED = ["xhci-pci", "xhci-pci-renesas", "ehci-pci", "ohci-pci", "uhci-hcd", "usb-storage", "uas",
          "sd_mod", "vfat", "nls_cp437", "nls_iso8859-1"]


def norm(name):
    return name.replace("-", "_")


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    out, busybox, debs = sys.argv[1], sys.argv[2], sys.argv[3:]
    here = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(out, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for d in debs:
            subprocess.run(["dpkg-deb", "-x", d, tmp], check=True)
        kernels = glob.glob(os.path.join(tmp, "boot", "vmlinuz-*"))
        if len(kernels) != 1:
            sys.exit("mkbenchlinux: need exactly one linux-image package")
        ver = os.path.basename(kernels[0])[len("vmlinuz-"):]
        moddir = os.path.join(tmp, "lib", "modules", ver)
        if not os.path.isdir(moddir):
            moddir = os.path.join(tmp, "usr", "lib", "modules", ver)
        if not os.path.exists(os.path.join(moddir, "modules.builtin")):
            sys.exit(f"mkbenchlinux: no modules for {ver}: add the linux-modules package")
        shutil.copyfile(kernels[0], os.path.join(out, "vmlinuz"))

        builtin = set()
        with open(os.path.join(moddir, "modules.builtin")) as f:
            for line in f:
                builtin.add(norm(os.path.basename(line.strip()).split(".ko")[0]))
        paths = {}                                  # module name -> its file
        for root, _, names in os.walk(moddir):
            for fn in names:
                if ".ko" in fn:
                    paths[norm(fn.split(".ko")[0])] = os.path.join(root, fn)
        mods = os.path.join(tmp, "mods")
        os.makedirs(mods)

        def unpack(name):                           # the plain .ko, from .ko.zst or .ko.xz
            src, dst = paths[name], os.path.join(mods, name + ".ko")
            if not os.path.exists(dst):
                if src.endswith(".zst"):
                    subprocess.run(["zstd", "-q", "-d", "-f", src, "-o", dst], check=True)
                elif src.endswith(".xz"):
                    subprocess.run(f"xz -dc '{src}' > '{dst}'", shell=True, check=True)
                else:
                    shutil.copy(src, dst)
            return dst

        def depends(name):                          # its .modinfo "depends=" list
            body = open(unpack(name), "rb").read()
            i = body.find(b"\0depends=")
            if i < 0:
                return []
            v = body[i + 9:body.index(b"\0", i + 1)].decode()
            return [norm(d) for d in v.split(",") if d]

        order = []

        def add(name):
            if name in order:
                return
            for d in depends(name):
                if d not in builtin:
                    add(d)
            order.append(name)

        for w in WANTED:
            n = norm(w)
            if n in builtin:
                continue
            if n not in paths:
                print(f"mkbenchlinux: {w} is neither a module nor built in, skipped")
                continue
            add(n)

        files = [("init", os.path.join(here, "benchlinux-init"), 0o755), ("bin/busybox", busybox, 0o755)]
        files += [(f"lib/{n}.ko", unpack(n), 0o644) for n in order]
        order_txt = os.path.join(tmp, "modules.order")
        with open(order_txt, "w") as f:
            f.write("\n".join(order) + "\n")
        files.append(("lib/modules.order", order_txt, 0o644))
        write_cpio(os.path.join(out, "initrd.gz"), files)
        print(f"mkbenchlinux: Linux {ver}, initramfs with {len(order)} modules: {' '.join(order)}")


def write_cpio(path, files):
    """newc cpio, gzip-compressed: the directories, /dev/console and `files`."""
    data = bytearray()
    ino = [1]

    def ent(name, mode, body=b"", rdev=(0, 0)):
        hdr = "070701%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X" % (
            ino[0], mode, 0, 0, 1, 0, len(body), 0, 0, rdev[0], rdev[1], len(name) + 1, 0)
        ino[0] += 1
        b = hdr.encode() + name.encode() + b"\0"
        b += b"\0" * (-len(b) % 4) + body + b"\0" * (-len(body) % 4)
        data.extend(b)

    for d in ["bin", "lib", "dev", "proc", "sys", "newroot"]:
        ent(d, stat.S_IFDIR | 0o755)
    ent("dev/console", stat.S_IFCHR | 0o600, rdev=(5, 1))
    for dst, src, mode in files:
        with open(src, "rb") as f:
            ent(dst, stat.S_IFREG | mode, f.read())
    ent("TRAILER!!!", 0)
    with open(path, "wb") as f:
        f.write(gzip.compress(bytes(data)))


if __name__ == "__main__":
    main()
