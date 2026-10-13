#!/usr/bin/env python3
"""tools/nvfirmware.py -- put NVIDIA's GPU firmware into a root file system tree

    nvfirmware.py SOURCE ROOT

SOURCE is Ubuntu/Debian's linux-firmware-nvidia-graphics .deb, or a
directory holding linux-firmware's nvidia/ tree (a linux-firmware checkout,
or /lib/firmware of a Linux system). The firmware nouveau loads for the
chips it drives without NVIDIA's GSP (Maxwell 2, Pascal, Volta, Turing) is
copied to ROOT/lib/firmware/nvidia/, symbolic links resolved and .zst
files decompressed, so request_firmware() in the Linux KPI finds the plain
names nouveau asks for. The GSP images of Turing and later (tens of MB
each) are left out.

The files are NVIDIA's, not part of Kestrel: they may be redistributed
unmodified for use on operating systems under an OSI-approved license,
with NVIDIA's license next to them, which this tool copies to
ROOT/lib/firmware/nvidia/LICENCE.nvidia. Nothing here is committed to the
repository; the images get the firmware only when NVIDIA_FW= is given.
"""
import os
import shutil
import subprocess
import sys
import tempfile

FAMILIES = ["gm200", "gm204", "gm206", "gp100", "gp102", "gp104", "gp106", "gp107", "gp108",
            "gv100", "tu102", "tu104", "tu106", "tu10x", "tu116", "tu117"]


def decompress(path):
    if not path.endswith(".zst"):
        with open(path, "rb") as f:
            return f.read()
    try:
        import zstandard
        with open(path, "rb") as f:
            return zstandard.ZstdDecompressor().decompress(f.read(), max_output_size=64 << 20)
    except ImportError:
        return subprocess.run(["zstd", "-dc", path], check=True, capture_output=True).stdout


def license_text(top):
    """LICENCE.nvidia of a linux-firmware tree, or the NVIDIA part of a
    Debian copyright file."""
    for cand in (os.path.join(top, "LICENCE.nvidia"), os.path.join(top, "..", "LICENCE.nvidia")):
        if os.path.isfile(cand):
            with open(cand, encoding="utf-8", errors="replace") as f:
                return f.read()
    for root, _, files in os.walk(top):
        if "copyright" in files and "nvidia" in root:
            with open(os.path.join(root, "copyright"), encoding="utf-8", errors="replace") as f:
                text = f.read()
            i = text.find("License: Redistributable-nvidia\n ")
            if i < 0:
                i = text.rfind("License: Redistributable-nvidia")
            if i >= 0:
                lines = text[i:].splitlines()[1:]
                body = []
                for line in lines:
                    if line and not line.startswith(" "):
                        break
                    line = line[1:] if line.startswith(" ") else line
                    body.append("" if line == "." else line)
                return "\n".join(body).strip() + "\n"
    return None


def find_nvidia_dir(top):
    for cand in (os.path.join(top, "lib", "firmware", "nvidia"), os.path.join(top, "nvidia"), top):
        if os.path.isdir(os.path.join(cand, "gp102")):
            return cand
    return None


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip().splitlines()[2])
    source, root = sys.argv[1], sys.argv[2]
    tmp = None
    try:
        top = source
        if os.path.isfile(source) and source.endswith(".deb"):
            tmp = tempfile.mkdtemp(prefix="nvfw-")
            subprocess.run(["dpkg-deb", "-x", source, tmp], check=True)
            top = tmp
        nv = find_nvidia_dir(top)
        if not nv:
            sys.exit(f"nvfirmware: no nvidia/ firmware tree in {source}")
        lic = license_text(top) or license_text(os.path.dirname(nv))
        if not lic:
            sys.exit(f"nvfirmware: NVIDIA's license is not in {source}; it must travel with the files")

        dst = os.path.join(root, "lib", "firmware", "nvidia")
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        files = size = 0
        for fam in FAMILIES:
            base = os.path.join(nv, fam)
            if not os.path.isdir(base):
                continue
            for dirpath, dirnames, filenames in os.walk(base, followlinks=True):
                dirnames[:] = [d for d in dirnames if d != "gsp"]
                for name in filenames:
                    src = os.path.join(dirpath, name)
                    if not os.path.exists(src):          # a dangling link
                        continue
                    out = os.path.join(dst, os.path.relpath(src, nv))
                    if out.endswith(".zst"):
                        out = out[:-4]
                    os.makedirs(os.path.dirname(out), exist_ok=True)
                    data = decompress(src)
                    with open(out, "wb") as f:
                        f.write(data)
                    files += 1
                    size += len(data)
        with open(os.path.join(dst, "LICENCE.nvidia"), "w", encoding="utf-8") as f:
            f.write(lic)
        print(f"  NVFW    {files} files, {size >> 10} KiB of NVIDIA firmware in {dst}")
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
