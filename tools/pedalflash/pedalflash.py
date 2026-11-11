#!/usr/bin/env python3
"""
pedalflash — pick your STM32 board, build the project, flash it over USB.

    python3 pedalflash.py                          # interactive wizard
    python3 pedalflash.py boards                   # list known boards
    python3 pedalflash.py setup                    # download toolchain + flash tools
    python3 pedalflash.py doctor                   # what is installed / plugged in
    python3 pedalflash.py build PROJECT [--board B]
    python3 pedalflash.py flash FILE.elf --board B [--method stlink|dfu]
    python3 pedalflash.py go PROJECT --board B     # build + flash
    python3 pedalflash.py drivers                  # USB permissions / drivers

PROJECT is an STM32CubeIDE project folder (the one holding .cproject); the
IDE does not need to be installed.  A folder with a Makefile is built with
`make` instead.

Nothing is "scraped": every download is a fixed official release URL checked
against the SHA-256 recorded below, so a changed or tampered file is refused.
Python 3.9+, standard library only.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
HOME = Path(os.environ.get("PEDALFLASH_HOME", Path.home() / ".pedalflash"))
TOOLS_DIR = HOME / "tools"
DL_DIR = HOME / "downloads"

# --------------------------------------------------------------------------
# Pinned downloads (url, sha256, path of the bin/ directory inside the archive)
# --------------------------------------------------------------------------
_XP = "https://github.com/xpack-dev-tools"
_GCC_V, _OCD_V = "14.2.1-1.1", "0.12.0-7"


def _xpack(repo, name, ver, plat, ext, sha):
    return {"url": f"{_XP}/{repo}/releases/download/v{ver}/xpack-{name}-{ver}-{plat}.{ext}",
            "sha256": sha, "bin": f"xpack-{name}-{ver}/bin"}


TOOLS = {
    "gcc": {
        "version": _GCC_V, "exe": "arm-none-eabi-gcc",
        "linux-x64":    _xpack("arm-none-eabi-gcc-xpack", "arm-none-eabi-gcc", _GCC_V, "linux-x64", "tar.gz",
                               "ed8c7d207a85d00da22b90cf80ab3b0b2c7600509afadf6b7149644e9d4790a6"),
        "linux-arm64":  _xpack("arm-none-eabi-gcc-xpack", "arm-none-eabi-gcc", _GCC_V, "linux-arm64", "tar.gz",
                               "a1ac95c8d9347020d61e387e644a2c1806556b77162958a494d2f5f3d5fe7053"),
        "darwin-x64":   _xpack("arm-none-eabi-gcc-xpack", "arm-none-eabi-gcc", _GCC_V, "darwin-x64", "tar.gz",
                               "b5bf8d5af099fd464d1543e5b8901308fb64116fa7a244426cacf4ff1b882fc7"),
        "darwin-arm64": _xpack("arm-none-eabi-gcc-xpack", "arm-none-eabi-gcc", _GCC_V, "darwin-arm64", "tar.gz",
                               "f52ea3760c53b25d726a7345be60a210736293db85f92daa39d1d22d34e2c995"),
        "win32-x64":    _xpack("arm-none-eabi-gcc-xpack", "arm-none-eabi-gcc", _GCC_V, "win32-x64", "zip",
                               "0b2d496b383ba578182eb57b3f7d35ff510e36eda56257883b902fa07c3bba55"),
    },
    "openocd": {
        "version": _OCD_V, "exe": "openocd",
        "linux-x64":    _xpack("openocd-xpack", "openocd", _OCD_V, "linux-x64", "tar.gz",
                               "94b3790983beaf8ed57e646c0620dd66d705fddae03d290823a6ed3b439468d6"),
        "linux-arm64":  _xpack("openocd-xpack", "openocd", _OCD_V, "linux-arm64", "tar.gz",
                               "db73a3ab91c556ecec2405a7e02d404b11139df6aba1031cad94a7e6766d06cc"),
        "darwin-x64":   _xpack("openocd-xpack", "openocd", _OCD_V, "darwin-x64", "tar.gz",
                               "668ad25350103a4357e11629ec833eae5982e973889ce25bad0c2963e37fa8bf"),
        "darwin-arm64": _xpack("openocd-xpack", "openocd", _OCD_V, "darwin-arm64", "tar.gz",
                               "667342c086984f3e5a55b4e0d5f711add13fb04de040fca493303000e6c19327"),
        "win32-x64":    _xpack("openocd-xpack", "openocd", _OCD_V, "win32-x64", "zip",
                               "6bfd3c97135aafef8affc9af1acf34fd0e2b9ca26044506f6abd7f95b7630052"),
    },
    "dfu-util": {
        # one archive with binaries for several OSes
        "version": "0.11", "exe": "dfu-util",
        **{p: {"url": "https://dfu-util.sourceforge.net/releases/dfu-util-0.11-binaries.tar.xz",
               "sha256": "6450de30a7dcd8d8c1273f43f0b153f054fd24d85f7f38296b1ad8edbd2ddb25",
               "bin": f"dfu-util-0.11-binaries/{sub}"}
           for p, sub in (("linux-x64", "linux-amd64"), ("darwin-x64", "darwin-x86_64"),
                          ("darwin-arm64", "darwin-x86_64"), ("win32-x64", "win64"))},
    },
    "zadig": {   # Windows only: binds the WinUSB driver to ST-LINK / DFU devices
        "version": "2.9", "exe": "zadig-2.9",
        "win32-x64": {"url": "https://github.com/pbatard/libwdi/releases/download/v1.5.1/zadig-2.9.exe",
                      "sha256": "4ecaa95df3da3621486a043aef8b3050b8bafe7c901402871e816229ef82039b",
                      "bin": "."},
    },
}

USB_IDS = {  # VID 0483 (STMicroelectronics)
    "3748": "ST-LINK/V2", "374b": "ST-LINK/V2-1", "374a": "ST-LINK/V2-1", "3752": "ST-LINK/V2-1",
    "374e": "ST-LINK/V3", "374f": "ST-LINK/V3", "3753": "ST-LINK/V3", "3754": "ST-LINK/V3",
    "3757": "ST-LINK/V3PWR", "df11": "STM32 ROM bootloader (USB DFU)",
}


# --------------------------------------------------------------------------
# small helpers
# --------------------------------------------------------------------------
def say(msg=""):
    print(msg, flush=True)


def die(msg, code=1):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(code)


def plat_key():
    m = platform.machine().lower()
    arch = "arm64" if m in ("arm64", "aarch64") else "x64"
    if sys.platform.startswith("linux"):
        return f"linux-{arch}"
    if sys.platform == "darwin":
        return f"darwin-{arch}"
    if sys.platform in ("win32", "cygwin"):
        return "win32-x64"
    return f"{sys.platform}-{arch}"


EXE = ".exe" if plat_key().startswith("win") else ""


def run(cmd, cwd=None, check=True, capture=False):
    say("  $ " + " ".join(str(c) if " " not in str(c) else f'"{c}"' for c in cmd))
    r = subprocess.run([str(c) for c in cmd], cwd=cwd, text=True,
                       stdout=subprocess.PIPE if capture else None,
                       stderr=subprocess.STDOUT if capture else None)
    if check and r.returncode != 0:
        if capture and r.stdout:
            say(r.stdout)
        die(f"command failed ({r.returncode}): {cmd[0]}")
    return r


def ask(prompt, default=None):
    if not sys.stdin.isatty():
        return default
    s = input(f"{prompt}" + (f" [{default}]" if default else "") + ": ").strip()
    return s or default


# --------------------------------------------------------------------------
# tool installation
# --------------------------------------------------------------------------
def tool_bin_dir(name):
    spec = TOOLS[name].get(plat_key())
    if not spec:
        return None
    return TOOLS_DIR / f"{name}-{TOOLS[name]['version']}" / spec["bin"]


def find_tool(name, exe=None):
    """Our pinned copy first, then PATH."""
    exe = exe or TOOLS[name]["exe"]
    d = tool_bin_dir(name)
    if d:
        p = d / (exe + EXE)
        if p.exists():
            return p
    w = shutil.which(exe)
    return Path(w) if w else None


def _sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _download(url, dest, sha):
    if dest.exists() and _sha256(dest) == sha:
        return
    DL_DIR.mkdir(parents=True, exist_ok=True)
    say(f"  downloading {url}")
    tmp = dest.with_suffix(dest.suffix + ".part")
    with urllib.request.urlopen(url, timeout=60) as r, open(tmp, "wb") as f:
        total = int(r.headers.get("Content-Length") or 0)
        got = 0
        while True:
            b = r.read(1 << 16)
            if not b:
                break
            f.write(b)
            got += len(b)
            if total and sys.stdout.isatty():
                print(f"\r    {got >> 20} / {total >> 20} MB", end="", flush=True)
    if total and sys.stdout.isatty():
        print()
    actual = _sha256(tmp)
    if actual != sha:
        tmp.unlink()
        die(f"SHA-256 mismatch for {url}\n  expected {sha}\n  got      {actual}\n"
            "The file is not the one this script was written for — refusing to use it.")
    tmp.replace(dest)


def _safe_extract(archive, dest):
    dest.mkdir(parents=True, exist_ok=True)
    root = dest.resolve()
    if archive.name.endswith(".zip"):
        with zipfile.ZipFile(archive) as z:
            for m in z.namelist():
                if not (root / m).resolve().is_relative_to(root):
                    die(f"unsafe path in archive: {m}")
            z.extractall(dest)
    elif archive.name.endswith(".exe"):
        shutil.copy2(archive, dest / archive.name)
    else:
        with tarfile.open(archive) as t:
            for m in t.getmembers():
                if not (root / m.name).resolve().is_relative_to(root):
                    die(f"unsafe path in archive: {m.name}")
            if hasattr(tarfile, "data_filter"):
                t.extractall(dest, filter="tar")
            else:
                t.extractall(dest)


def install_tool(name):
    spec = TOOLS[name].get(plat_key())
    if not spec:
        say(f"  {name}: no pinned download for {plat_key()} — install it with your package manager")
        return None
    target = TOOLS_DIR / f"{name}-{TOOLS[name]['version']}"
    exe = target / spec["bin"] / (TOOLS[name]["exe"] + EXE)
    if exe.exists():
        say(f"  {name} {TOOLS[name]['version']}: already installed")
        return exe
    archive = DL_DIR / spec["url"].rsplit("/", 1)[1]
    _download(spec["url"], archive, spec["sha256"])
    say(f"  unpacking {archive.name}")
    _safe_extract(archive, target)
    if not EXE and exe.exists():
        exe.chmod(exe.stat().st_mode | 0o111)
    say(f"  {name} {TOOLS[name]['version']} -> {exe.parent}")
    return exe


def cmd_setup(args):
    wanted = args.tools or ["gcc", "openocd", "dfu-util"] + (["zadig"] if EXE else [])
    say(f"Installing into {TOOLS_DIR}  (platform {plat_key()})")
    for t in wanted:
        if t not in TOOLS:
            die(f"unknown tool {t}; choose from {', '.join(TOOLS)}")
        if find_tool(t) and not tool_bin_dir(t) and not args.force:
            say(f"  {t}: found on PATH ({find_tool(t)}), skipping (use --force to install the pinned copy)")
            continue
        install_tool(t)
    if sys.platform.startswith("linux"):
        say("\nLinux: run 'pedalflash drivers' once so ST-LINK/DFU work without sudo.")


def need_tool(name, auto):
    p = find_tool(name)
    if p:
        return p
    if auto or (sys.stdin.isatty() and (ask(f"{name} is not installed. Download the pinned version now? (y/n)", "y") or "").lower().startswith("y")):
        p = install_tool(name)
        if p:
            return p
    die(f"{name} not found — run: pedalflash setup")


# --------------------------------------------------------------------------
# boards
# --------------------------------------------------------------------------
def load_boards():
    with open(HERE / "boards.json", encoding="utf-8") as f:
        return json.load(f)["boards"]


def find_board(q):
    boards = load_boards()
    ql = q.lower()
    exact = [b for b in boards if b["id"] == ql]
    if exact:
        return exact[0]
    hits = [b for b in boards if ql in b["id"] or ql in b["name"].lower() or ql in b["mcu"].lower()]
    if len(hits) == 1:
        return hits[0]
    if not hits:
        die(f"no board matches '{q}'. Run 'pedalflash boards'.")
    die(f"'{q}' matches several boards: " + ", ".join(b["id"] for b in hits))


def cmd_boards(_args):
    for b in load_boards():
        say(f"{b['id']:16} {b['name']}")
        say(f"{'':16} MCU {b['mcu']}, flash via {' / '.join(b['methods'])}")


def pick_board_interactive():
    boards = load_boards()
    for i, b in enumerate(boards, 1):
        say(f"  {i:2}) {b['name']}   [{', '.join(b['methods'])}]")
    s = ask("Which board do you have? (number or name)")
    if not s:
        die("no board chosen")
    if s.isdigit() and 1 <= int(s) <= len(boards):
        return boards[int(s) - 1]
    return find_board(s)


# --------------------------------------------------------------------------
# STM32CubeIDE project → command lines (no IDE needed)
# --------------------------------------------------------------------------
CORES = [  # (MCU prefix, -mcpu, default fpu or None)
    ("STM32F0", "cortex-m0", None), ("STM32G0", "cortex-m0plus", None), ("STM32L0", "cortex-m0plus", None),
    ("STM32C0", "cortex-m0plus", None), ("STM32F1", "cortex-m3", None), ("STM32F2", "cortex-m3", None),
    ("STM32L1", "cortex-m3", None), ("STM32F3", "cortex-m4", "fpv4-sp-d16"), ("STM32F4", "cortex-m4", "fpv4-sp-d16"),
    ("STM32G4", "cortex-m4", "fpv4-sp-d16"), ("STM32L4", "cortex-m4", "fpv4-sp-d16"), ("STM32WB", "cortex-m4", "fpv4-sp-d16"),
    ("STM32F7", "cortex-m7", "fpv5-d16"), ("STM32H7", "cortex-m7", "fpv5-d16"), ("STM32L5", "cortex-m33", "fpv5-sp-d16"),
    ("STM32U5", "cortex-m33", "fpv5-sp-d16"), ("STM32H5", "cortex-m33", "fpv5-sp-d16"),
]
SRC_EXT = {".c": "c", ".cpp": "cpp", ".cc": "cpp", ".cxx": "cpp", ".s": "asm", ".S": "asm"}
OPT = {"o0": "-O0", "o1": "-O1", "o2": "-O2", "o3": "-O3", "os": "-Os", "og": "-Og", "ofast": "-Ofast", "oz": "-Oz"}


def _tail(v):
    """'...option.fpu.value.fpv5-d16' → 'fpv5-d16'"""
    return v.rsplit(".value.", 1)[-1] if v and ".value." in v else v


class CubeProject:
    def __init__(self, root: Path, config: str = "Debug"):
        self.root = root.resolve()
        self.name = self.root.name
        self.links = {}
        self._read_project()
        self._read_cproject(config)

    # .project: linked folders (often absolute Windows paths from the author's PC)
    def _read_project(self):
        p = self.root / ".project"
        if not p.exists():
            return
        t = ET.parse(p).getroot()
        n = t.find("name")
        if n is not None and n.text:
            self.name = n.text.strip()
        for link in t.iter("link"):
            name = (link.findtext("name") or "").strip()
            loc = (link.findtext("location") or link.findtext("locationURI") or "").strip()
            if name and loc:
                self.links[name] = self._resolve_link(loc)

    def _resolve_link(self, loc):
        loc = loc.replace("PARENT-", "").replace("\\", "/")
        m = re.match(r"(\d+)-PROJECT_LOC/(.*)", loc)        # Eclipse PARENT-n-PROJECT_LOC/x
        if m:
            base = self.root
            for _ in range(int(m.group(1))):
                base = base.parent
            return (base / m.group(2)).resolve()
        loc = loc.replace("PROJECT_LOC", str(self.root))
        p = Path(loc)
        if p.exists():
            return p.resolve()
        # absolute path from someone else's machine: find the same trailing
        # components next to this project (…/GuitarPedalSTM32/software/util → ../util)
        parts = [x for x in loc.split("/") if x and not x.endswith(":")]
        for k in range(1, len(parts) + 1):
            suffix = Path(*parts[-k:])
            base = self.root
            for _ in range(6):
                cand = base / suffix
                if cand.exists():
                    return cand.resolve()
                base = base.parent
        return None

    def path(self, s):
        """Resolve a CDT path string to an absolute Path."""
        s = s.strip().strip('"')
        s = s.replace("${ProjName}", self.name)
        m = re.match(r"\$\{workspace_loc:/?([^}]*)\}(.*)", s)
        if m:
            inner = m.group(1).strip("/")
            parts = inner.split("/", 1)
            rel = parts[1] if parts[0] == self.name and len(parts) > 1 else ("" if parts[0] == self.name else inner)
            s = rel + m.group(2)
            return self._project_rel(s)
        if s.startswith("../"):
            return self._project_rel(s[3:])     # CDT builds from <project>/<Config>/
        if os.path.isabs(s):
            return Path(s)
        return self._project_rel(s)

    def _project_rel(self, rel):
        rel = rel.strip("/")
        first, _, rest = rel.partition("/")
        if first in self.links and self.links[first] is not None:
            return (self.links[first] / rest).resolve() if rest else self.links[first]
        return (self.root / rel).resolve()

    def _read_cproject(self, config):
        self.mcu = None
        self.fpu = None
        self.float_abi = None
        self.defs, self.incs, self.cpp_defs, self.cpp_incs, self.asm_defs, self.asm_incs = [], [], [], [], [], []
        self.c_opt = self.cpp_opt = "-O0"
        self.c_std = self.cpp_std = None
        self.debug = "-g3"
        self.ldscript = None
        self.libs, self.libdirs, self.ldflags_extra = [], [], []
        self.sources = []      # [(Path folder, [excluded relative paths])]
        p = self.root / ".cproject"
        if not p.exists():
            raise FileNotFoundError(".cproject")
        root = ET.parse(p).getroot()
        cfgs = list(root.iter("cconfiguration"))
        chosen = None
        for c in cfgs:
            conf = c.find(".//configuration")
            if conf is not None and conf.get("name", "").lower() == config.lower():
                chosen = c
        if chosen is None:
            if not cfgs:
                raise ValueError("no configurations in .cproject")
            chosen = cfgs[0]
        self.config = chosen.find(".//configuration").get("name", config)

        def lst(opt):
            return [v.get("value") for v in opt.iter("listOptionValue") if v.get("value") and v.get("builtIn") != "true"]

        for opt in chosen.iter("option"):
            sc = opt.get("superClass") or ""
            v = opt.get("value")
            if sc.endswith("option.target_mcu"):
                self.mcu = v
            elif sc.endswith("option.fpu"):
                self.fpu = _tail(v)
            elif sc.endswith("option.floatabi"):
                self.float_abi = _tail(v)
        for tool in chosen.iter("tool"):
            ts = tool.get("superClass") or ""
            kind = ("cpp" if ".cpp.compiler" in ts else "c" if ".c.compiler" in ts else
                    "asm" if ".assembler" in ts else "ld" if ".linker" in ts else None)
            if not kind:
                continue
            for opt in tool.iter("option"):
                sc = (opt.get("superClass") or "").lower()
                v = opt.get("value")
                if kind in ("c", "cpp", "asm"):
                    if sc.endswith("definedsymbols"):
                        {"c": self.defs, "cpp": self.cpp_defs, "asm": self.asm_defs}[kind].extend(lst(opt))
                    elif sc.endswith("includepaths"):
                        {"c": self.incs, "cpp": self.cpp_incs, "asm": self.asm_incs}[kind].extend(lst(opt))
                    elif sc.endswith("optimization.level") and v:
                        o = OPT.get(_tail(v).lower(), "-O0")
                        if kind == "c":
                            self.c_opt = o
                        elif kind == "cpp":
                            self.cpp_opt = o
                    elif sc.endswith("languagestandard") and v:
                        std = _tail(v).replace("pp", "++")
                        if kind == "c":
                            self.c_std = std
                        elif kind == "cpp":
                            self.cpp_std = std
                    elif sc.endswith("debuglevel") and v and kind == "c":
                        self.debug = "-" + _tail(v)
                else:
                    if sc.endswith("option.script") and v:
                        self.ldscript = self.path(v)
                    elif sc.endswith(".libraries"):
                        self.libs.extend(lst(opt))
                    elif sc.endswith(".directories"):
                        self.libdirs.extend(lst(opt))
                    elif sc.endswith("otherflags"):
                        self.ldflags_extra.extend(lst(opt))
        for e in chosen.iter("entry"):
            if e.get("kind") == "sourcePath":
                folder = self._project_rel(e.get("name") or "")
                excl = [x for x in (e.get("excluding") or "").split("|") if x]
                self.sources.append((folder, excl))
        if not self.sources:
            self.sources.append((self.root, []))
        if self.ldscript is None:
            lds = sorted(self.root.glob("*FLASH.ld")) or sorted(self.root.glob("*.ld"))
            self.ldscript = lds[0] if lds else None

    def source_files(self):
        out = []
        skip_dirs = {"Debug", "Release", ".settings", "build", ".git"}
        for folder, excl in self.sources:
            if folder is None or not folder.exists():
                say(f"  warning: source folder missing: {folder}")
                continue
            ex = [folder / x for x in excl]
            for dirpath, dirnames, files in os.walk(folder):
                d = Path(dirpath)
                dirnames[:] = [x for x in dirnames if x not in skip_dirs and not x.startswith(".")
                               and not any((d / x) == e for e in ex)]
                for fn in files:
                    f = d / fn
                    if f.suffix in SRC_EXT and not any(f == e for e in ex):
                        out.append(f)
        return sorted(set(out))


class JsonProject:
    """A project described by pedalflash.json — for bare-metal projects that
    have no IDE files.  No make needed (Windows-friendly).  Keys:
      name, board, mcu, sources[], exclude[], include[], defines[], ldscript,
      optimization ("-O2"), c_std, cpp_std, cflags[], ldflags[], libs[]
    Paths are relative to the folder holding pedalflash.json; sources accept
    globs ("../bsp/*.c", "src/**/*.cpp")."""

    def __init__(self, root: Path):
        self.root = root.resolve()
        with open(self.root / "pedalflash.json", encoding="utf-8") as f:
            j = json.load(f)
        self.j = j
        self.name = j.get("name", self.root.name)
        self.board_id = j.get("board")
        self.config = "default"
        self.links = {}
        self.mcu = j.get("mcu")
        self.fpu, self.float_abi = j.get("fpu"), j.get("float_abi")
        self.defs = list(j.get("defines", []))
        self.incs = list(j.get("include", []))
        self.cpp_defs, self.cpp_incs, self.asm_defs, self.asm_incs = [], [], [], []
        self.c_opt = self.cpp_opt = j.get("optimization", "-O2")
        self.c_std, self.cpp_std = j.get("c_std", "gnu11"), j.get("cpp_std", "gnu++17")
        self.debug = j.get("debug", "-g3")
        self.ldscript = self.path(j["ldscript"]) if j.get("ldscript") else None
        self.libs, self.libdirs = list(j.get("libs", [])), []
        self.cflags_extra = list(j.get("cflags", []))
        self.ldflags_extra = list(j.get("ldflags", []))

    def path(self, s):
        return (self.root / s).resolve()

    def source_files(self):
        out = set()
        for pat in self.j.get("sources", []):
            out.update(p.resolve() for p in self.root.glob(pat) if p.suffix in SRC_EXT)
        for pat in self.j.get("exclude", []):
            out.difference_update(p.resolve() for p in self.root.glob(pat))
        return sorted(out)


def cpu_flags(mcu, fpu, float_abi):
    core, dfpu = None, None
    for pre, c, f in CORES:
        if (mcu or "").upper().startswith(pre):
            core, dfpu = c, f
    if not core:
        die(f"don't know the Cortex core of {mcu}")
    fl = [f"-mcpu={core}", "-mthumb"]
    fpu = fpu or dfpu
    abi = float_abi or ("hard" if fpu else "soft")
    if fpu and fpu != "no" and abi != "soft":
        fl.append(f"-mfpu={fpu}")
    fl.append(f"-mfloat-abi={abi}")
    return fl


def build_cube(project_dir: Path, board, config, jobs, auto):
    gcc = need_tool("gcc", auto)
    bindir = gcc.parent
    tool = lambda n: str(bindir / f"arm-none-eabi-{n}{EXE}")
    pj = (JsonProject(project_dir) if (project_dir / "pedalflash.json").exists()
          else CubeProject(project_dir, config))
    mcu = pj.mcu or (board or {}).get("mcu")
    if board and pj.mcu and board["mcu"][:9].upper() != pj.mcu[:9].upper():
        say(f"  warning: project is for {pj.mcu} but board '{board['id']}' is {board['mcu']}")
    cpu = cpu_flags(mcu, pj.fpu, pj.float_abi)
    for name, target in pj.links.items():
        say(f"  linked folder {name} -> {target if target else 'NOT FOUND'}")
    if not pj.ldscript or not Path(pj.ldscript).exists():
        die(f"linker script not found ({pj.ldscript})")

    out = project_dir / "build" / "pedalflash" / pj.config
    out.mkdir(parents=True, exist_ok=True)
    srcs = pj.source_files()
    if not srcs:
        die("no source files found")
    incs = lambda lst: [f"-I{pj.path(i)}" for i in lst]
    defs = lambda lst: [f"-D{d}" for d in lst]
    common = (cpu + [pj.debug, "-ffunction-sections", "-fdata-sections", "-Wall", "-fstack-usage"]
              + getattr(pj, "cflags_extra", []))
    cflags = common + [pj.c_opt] + ([f"-std={pj.c_std}"] if pj.c_std else ["-std=gnu11"]) + defs(pj.defs) + incs(pj.incs)
    cppflags = (common + [pj.cpp_opt] + ([f"-std={pj.cpp_std}"] if pj.cpp_std else ["-std=gnu++17"])
                + ["-fno-exceptions", "-fno-rtti", "-fno-use-cxa-atexit"]
                + defs(pj.cpp_defs or pj.defs) + incs(pj.cpp_incs or pj.incs))
    asmflags = cpu + [pj.debug, "-x", "assembler-with-cpp"] + defs(pj.asm_defs or pj.defs) + incs(pj.asm_incs or pj.incs)

    def obj_for(src):
        try:
            rel = src.relative_to(project_dir.resolve())
        except ValueError:
            rel = Path("_ext") / Path(*[p for p in src.parts if p not in ("/", "\\")][-4:])
        return out / rel.with_suffix(rel.suffix + ".o")

    def up_to_date(src, obj):
        dep = obj.with_suffix(".d")
        if not obj.exists() or not dep.exists():
            return False
        t = obj.stat().st_mtime
        txt = dep.read_text(errors="ignore").replace("\\\n", " ")
        for f in txt.split(":", 1)[-1].split():
            fp = Path(f)
            if fp.exists() and fp.stat().st_mtime > t:
                return False
        return True

    def compile_one(src):
        obj = obj_for(src)
        obj.parent.mkdir(parents=True, exist_ok=True)
        if up_to_date(src, obj):
            return None
        kind = SRC_EXT[src.suffix]
        if kind == "c":
            cmd = [tool("gcc")] + cflags
        elif kind == "cpp":
            cmd = [tool("g++")] + cppflags
        else:
            cmd = [tool("gcc")] + asmflags
        cmd += ["-MMD", "-MP", "-MF", str(obj.with_suffix(".d")), "-c", str(src), "-o", str(obj)]
        r = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return (src, r.returncode, r.stdout, cmd)

    say(f"Building {pj.name} [{pj.config}] for {mcu}: {len(srcs)} source files")
    failed = 0
    with cf.ThreadPoolExecutor(max_workers=jobs) as ex:
        for res in ex.map(compile_one, srcs):
            if res is None:
                continue
            src, rc, txt, cmd = res
            tag = "CC " if rc == 0 else "ERR"
            say(f"  {tag} {src.relative_to(project_dir.resolve()) if src.is_relative_to(project_dir.resolve()) else src}")
            if txt.strip():
                say(txt.rstrip())
            if rc:
                failed += 1
                if failed == 1:
                    say("  failing command: " + " ".join(cmd))
    if failed:
        die(f"{failed} file(s) failed to compile")

    elf = out / f"{pj.name}.elf"
    objs = [str(obj_for(s)) for s in srcs]
    linker = tool("g++") if any(SRC_EXT[s.suffix] == "cpp" for s in srcs) else tool("gcc")
    ld = (cpu + [f"-T{pj.ldscript}", "--specs=nosys.specs", "--specs=nano.specs",
                 f"-Wl,-Map={out / (pj.name + '.map')}", "-Wl,--gc-sections", "-static",
                 "-Wl,--print-memory-usage", "-Wl,--no-warn-rwx-segments"]
          + [f"-L{pj.path(d)}" for d in pj.libdirs] + pj.ldflags_extra
          + ["-o", str(elf)] + objs
          + ["-Wl,--start-group", "-lc", "-lm"] + (["-lstdc++", "-lsupc++"] if linker.endswith("g++" + EXE) else [])
          + [f"-l{l}" for l in pj.libs] + ["-Wl,--end-group"])
    rsp = out / "link.rsp"      # Windows command lines are limited to 32 K
    rsp.write_text("\n".join(f'"{a}"' if " " in a else a for a in ld).replace("\\", "/"))
    say("  LD  " + elf.name)
    run([linker, f"@{rsp}"])
    run([tool("objcopy"), "-O", "binary", elf, elf.with_suffix(".bin")])
    run([tool("objcopy"), "-O", "ihex", elf, elf.with_suffix(".hex")])
    run([tool("size"), elf])
    say(f"OK: {elf}")
    return elf


def git_submodules(project_dir: Path):
    try:
        top = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=project_dir, text=True,
                             capture_output=True)
    except FileNotFoundError:
        return
    if top.returncode:
        return
    st = subprocess.run(["git", "submodule", "status", "--recursive"], cwd=top.stdout.strip(),
                        text=True, capture_output=True).stdout
    if any(line.startswith("-") for line in st.splitlines()):
        say("Fetching git submodules (project dependencies)…")
        run(["git", "submodule", "update", "--init", "--recursive"], cwd=top.stdout.strip())


def find_project(p: Path):
    p = p.resolve()
    if (p / "pedalflash.json").exists() or (p / ".cproject").exists() or (p / "Makefile").exists():
        return p
    hits = sorted({c.parent for pat in ("pedalflash.json", ".cproject") for c in p.rglob(pat)
                   if "build" not in c.parts})
    if len(hits) == 1:
        return hits[0]
    if not hits:
        die(f"no pedalflash.json, STM32CubeIDE project (.cproject) or Makefile under {p}")
    die("several projects found, pick one:\n  " + "\n  ".join(str(h) for h in hits))


def do_build(project, board, config="Debug", jobs=None, auto=False):
    pdir = find_project(Path(project))
    git_submodules(pdir)
    if (pdir / "pedalflash.json").exists() or (pdir / ".cproject").exists():
        return build_cube(pdir, board, config, jobs or os.cpu_count() or 4, auto)
    gcc = need_tool("gcc", auto)
    env_path = str(gcc.parent) + os.pathsep + os.environ.get("PATH", "")
    say(f"Building {pdir} with make")
    r = subprocess.run(["make", f"-j{jobs or os.cpu_count() or 4}"], cwd=pdir, env={**os.environ, "PATH": env_path})
    if r.returncode:
        die("make failed")
    elfs = sorted(pdir.rglob("*.elf"), key=lambda f: f.stat().st_mtime)
    if not elfs:
        die("make finished but no .elf was produced")
    return elfs[-1]


def cmd_build(args):
    board = find_board(args.board) if args.board else None
    do_build(args.project, board, args.config, args.jobs, args.yes)


# --------------------------------------------------------------------------
# flashing
# --------------------------------------------------------------------------
def usb_scan():
    """[(vid, pid, label, accessible)] for ST devices (Linux: sysfs; else via tools)."""
    found = []
    base = Path("/sys/bus/usb/devices")
    if base.exists():
        for d in base.iterdir():
            try:
                vid = (d / "idVendor").read_text().strip()
                pid = (d / "idProduct").read_text().strip()
            except OSError:
                continue
            if vid == "0483" and pid in USB_IDS:
                bus = int((d / "busnum").read_text())
                dev = int((d / "devnum").read_text())
                node = Path(f"/dev/bus/usb/{bus:03d}/{dev:03d}")
                found.append((vid, pid, USB_IDS[pid], os.access(node, os.R_OK | os.W_OK)))
    return found


def flash_stlink(board, image, extra):
    ocd = need_tool("openocd", False)
    scripts = ocd.parent.parent / "openocd" / "scripts"
    cmd = [ocd]
    if scripts.exists():
        cmd += ["-s", scripts]
    cmd += ["-f", "interface/stlink.cfg"]
    for e in extra or []:
        cmd += ["-c", e]
    cmd += ["-f", f"target/{board['openocd_target']}"]
    img = str(image).replace("\\", "/")
    if image.suffix == ".bin":
        cmd += ["-c", f"program {{{img}}} {board.get('flash_base', '0x08000000')} verify reset exit"]
    else:
        cmd += ["-c", f"program {{{img}}} verify reset exit"]
    r = run(cmd, check=False)
    if r.returncode:
        say("\nST-LINK flashing failed. Checklist:\n"
            "  - ST-LINK plugged in? ('pedalflash doctor' lists it)\n"
            "  - target powered (the GuitarML pedal needs its 9 V supply as well)\n"
            "  - Linux: 'pedalflash drivers' for USB permissions; Windows: ST-LINK driver or Zadig/WinUSB\n"
            "  - firmware disabled SWD pins / sleeps: retry with --under-reset")
        sys.exit(r.returncode)


def flash_dfu(board, image):
    dfu = need_tool("dfu-util", False)
    if image.suffix != ".bin":
        b = image.with_suffix(".bin")
        if not b.exists() or b.stat().st_mtime < image.stat().st_mtime:
            gcc = find_tool("gcc")
            if not gcc:
                die("need arm-none-eabi-objcopy to turn the .elf into a .bin (pedalflash setup)")
            run([gcc.parent / f"arm-none-eabi-objcopy{EXE}", "-O", "binary", image, b])
        image = b
    listing = run([dfu, "-l"], check=False, capture=True).stdout or ""
    if "0483:df11" not in listing.lower():
        say("No STM32 DFU device found.")
        say("  " + board.get("dfu_entry", "Put the MCU in its ROM bootloader (BOOT0 high during reset)."))
        if sys.stdin.isatty():
            ask("Press Enter when the board is in DFU mode")
        listing = run([dfu, "-l"], check=False, capture=True).stdout or ""
        if "0483:df11" not in listing.lower():
            say(listing)
            die("still no DFU device (Windows: bind WinUSB with 'pedalflash drivers')")
    run([dfu, "-a", "0", "-d", "0483:df11", "-s", f"{board.get('flash_base', '0x08000000')}:leave", "-D", image])
    say("Flashed. The board leaves DFU and runs the new firmware (some boards need a reset/power cycle).")


def do_flash(board, image: Path, method=None, extra=None, under_reset=False):
    method = method or board["methods"][0]
    if method not in board["methods"]:
        die(f"board '{board['id']}' cannot be flashed with {method}: supports {', '.join(board['methods'])}"
            + (" — " + board["notes"][0] if board.get("notes") else ""))
    for n in board.get("notes", []):
        say(f"  note: {n}")
    if not image.exists():
        die(f"{image} not found")
    say(f"Flashing {image.name} to {board['name']} via {method}")
    if method == "stlink":
        ex = list(extra or [])
        if under_reset:
            ex.append("reset_config srst_only srst_nogate connect_assert_srst")
        flash_stlink(board, image, ex)
    elif method == "dfu":
        flash_dfu(board, image)
    else:
        die(f"unknown method {method}")


def cmd_flash(args):
    do_flash(find_board(args.board), Path(args.file), args.method, args.openocd_extra, args.under_reset)


def project_board(project, given):
    """--board if given, else the "board" named in the project's pedalflash.json."""
    if given:
        return find_board(given)
    pdir = find_project(Path(project))
    pj = pdir / "pedalflash.json"
    if pj.exists():
        bid = json.loads(pj.read_text(encoding="utf-8")).get("board")
        if bid:
            return find_board(bid)
    die("which board? pass --board (see 'pedalflash boards')")


def cmd_go(args):
    board = project_board(args.project, args.board)
    elf = do_build(args.project, board, args.config, args.jobs, args.yes)
    do_flash(board, elf, args.method, args.openocd_extra, args.under_reset)


# --------------------------------------------------------------------------
# doctor / drivers
# --------------------------------------------------------------------------
UDEV_RULES = """# pedalflash: let normal users talk to ST-LINK probes and the STM32 ROM DFU bootloader
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0666", TAG+="uaccess"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374[abef]", MODE="0666", TAG+="uaccess"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="375[2347]", MODE="0666", TAG+="uaccess"
SUBSYSTEMS=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="df11", MODE="0666", TAG+="uaccess"
"""


def cmd_doctor(_args):
    say(f"pedalflash  python {platform.python_version()}  platform {plat_key()}  home {HOME}")
    for t in ("gcc", "openocd", "dfu-util") + (("zadig",) if EXE else ()):
        p = find_tool(t)
        ver = ""
        if p and t != "zadig":
            try:
                r = subprocess.run([str(p), "--version"], capture_output=True, text=True, timeout=10)
                ver = ((r.stdout or "") + (r.stderr or "")).strip().splitlines()[0]
            except Exception:
                pass
        say(f"  {t:9} {'OK  ' + str(p) if p else 'missing (pedalflash setup)'}  {ver}")
    devs = usb_scan()
    if devs:
        for vid, pid, label, ok in devs:
            say(f"  USB {vid}:{pid} {label}" + ("" if ok else "  <- no permission: run 'pedalflash drivers'"))
    elif Path("/sys/bus/usb/devices").exists():
        say("  USB: no ST-LINK or DFU device plugged in")
    else:
        say("  USB: device scan only on Linux; 'dfu-util -l' / 'openocd' report devices on this OS")


def cmd_drivers(_args):
    if sys.platform.startswith("linux"):
        dst = Path("/etc/udev/rules.d/49-pedalflash-stm32.rules")
        if os.geteuid() == 0:
            dst.write_text(UDEV_RULES)
            subprocess.run(["udevadm", "control", "--reload-rules"])
            subprocess.run(["udevadm", "trigger"])
            say(f"wrote {dst}; unplug and replug the device")
        else:
            tmp = Path(tempfile.gettempdir()) / "49-pedalflash-stm32.rules"
            tmp.write_text(UDEV_RULES)
            say("Run these once (needs sudo), then replug the device:")
            say(f"  sudo cp {tmp} {dst}")
            say("  sudo udevadm control --reload-rules && sudo udevadm trigger")
    elif sys.platform == "darwin":
        say("macOS needs no drivers for ST-LINK or DFU.")
    else:
        z = install_tool("zadig")
        say("Windows: OpenOCD and dfu-util need the WinUSB driver on the device.\n"
            "  ST-LINK: installing ST's ST-LINK driver also works; otherwise use Zadig.\n"
            "  Zadig: Options > List All Devices, pick 'STM32 BOOTLOADER' (DFU) or 'STM32 STLink',\n"
            "         driver = WinUSB, click Install/Replace Driver.")
        if z and (ask("Open Zadig now? (y/n)", "y") or "").lower().startswith("y"):
            os.startfile(str(z))  # type: ignore[attr-defined]


# --------------------------------------------------------------------------
# wizard
# --------------------------------------------------------------------------
def wizard():
    say("pedalflash — build and flash an STM32 project\n")
    board = pick_board_interactive()
    say(f"\n{board['name']}")
    for n in board.get("notes", []):
        say(f"  note: {n}")
    default_proj = None
    hint = board.get("project_hint")
    if hint:
        for base in [Path.cwd()] + list(Path.cwd().parents)[:3] + [HERE.parent.parent]:
            if (base / hint).exists():
                default_proj = str(base / hint)
                break
    proj = ask("Project folder (STM32CubeIDE project or Makefile)", default_proj or ".")
    method = board["methods"][0]
    if len(board["methods"]) > 1:
        method = ask(f"Flash method {board['methods']}", method)
    for t in ("gcc", "openocd" if method == "stlink" else "dfu-util"):
        need_tool(t, False)
    elf = do_build(proj, board)
    if (ask("Flash it now? (y/n)", "y") or "y").lower().startswith("y"):
        do_flash(board, elf, method)


def main(argv=None):
    ap = argparse.ArgumentParser(prog="pedalflash", description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("boards", help="list known boards").set_defaults(fn=cmd_boards)
    s = sub.add_parser("setup", help="download pinned toolchain + flash tools")
    s.add_argument("tools", nargs="*", help=f"subset of: {', '.join(TOOLS)}")
    s.add_argument("--force", action="store_true", help="install pinned copies even if found on PATH")
    s.set_defaults(fn=cmd_setup)
    sub.add_parser("doctor", help="show tools and connected USB devices").set_defaults(fn=cmd_doctor)
    sub.add_parser("drivers", help="USB permissions (Linux) / WinUSB via Zadig (Windows)").set_defaults(fn=cmd_drivers)

    def common(p, needs_board):
        p.add_argument("--board", "-b", required=needs_board, help="board id or part of its name")
        p.add_argument("--config", default="Debug", help="CubeIDE build configuration (Debug/Release)")
        p.add_argument("--jobs", "-j", type=int, default=None)
        p.add_argument("--yes", "-y", action="store_true", help="download missing tools without asking")

    def flashopts(p):
        p.add_argument("--method", "-m", choices=["stlink", "dfu"])
        p.add_argument("--under-reset", action="store_true", help="ST-LINK: connect with NRST held low")
        p.add_argument("--openocd-extra", action="append", help="extra OpenOCD command (before the target cfg)")

    b = sub.add_parser("build", help="build a project")
    b.add_argument("project")
    common(b, False)
    b.set_defaults(fn=cmd_build)
    f = sub.add_parser("flash", help="flash an .elf/.bin/.hex")
    f.add_argument("file")
    f.add_argument("--board", "-b", required=True)
    flashopts(f)
    f.set_defaults(fn=cmd_flash)
    g = sub.add_parser("go", help="build + flash")
    g.add_argument("project")
    common(g, False)
    flashopts(g)
    g.set_defaults(fn=cmd_go)

    args = ap.parse_args(argv)
    if not args.cmd:
        return wizard()
    args.fn(args)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
