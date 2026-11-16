#!/usr/bin/env python3
import argparse, glob, os, re, subprocess

ap = argparse.ArgumentParser()
ap.add_argument("--verify", action="store_true")
args = ap.parse_args()

lines = [l.strip() for l in open("makefile.burn_rules", encoding="latin-1") if l.strip() and not l.lstrip().startswith("#")]
obj_bases = sorted(set(re.findall(r"\b([\w-]+)\.o\b", re.sub(r"\\\s*\n", " ", "\n".join(lines)))))

disk = {}
for r, _, fs in os.walk("src"):
    for fn in fs:
        st, ex = os.path.splitext(fn)
        if ex in (".cpp", ".c"):
            disk.setdefault(st, []).append(os.path.join(r, fn).replace("\\", "/"))

mtext = "\n".join(open(p, encoding="utf-8").read() for p in ["meson.build"] + glob.glob("src/**/meson.build", recursive=True))
in_meson = lambda st: bool(re.search(rf"['\"/](?:.*/)?{re.escape(st)}\.(?:cpp|c)['\"]", mtext))

# Demangled C++ / C API prefixes -> Meson module key
API_MAP = {
    "Sek": "m68000", "Zet": "z80", "Z180": "z180", "Z8000": "z8000",
    "M6502": "m6502", "M6800": "m6800", "M6805": "m6805", "M6809": "m6809",
    "HD6309": "hd6309", "Sh2": "sh2", "Sh3": "sh4", "Sh4": "sh4",
    "t11": "t11", "Vez": "nec", "I8039": "i8039", "mcs51": "i8051",
    "pic16c5x": "pic16c5x", "upd7810": "upd7810", "TMS34010": "tms34010",
    "tms32010": "tms32010", "ADSP2100": "adsp2100", "arm": "arm", "Arm7": "arm7",
    "H6280": "h6280", "Konami": "konami", "S2650": "s2650", "TLCS90": "tlcs90",
    "V60": "v60", "EEPROM": "eeprom", "ppi8255": "8255ppi", "pia6821": "6821pia",
    "BurnYM2151": "ym2151", "BurnYM2203": "ym2203", "BurnYM2413": "ym2413",
    "BurnYM2608": "ym2608", "BurnYM2610": "ym2610", "BurnYM2612": "ym2612",
    "BurnYM3526": "ym3526", "BurnYM3812": "ym3812", "YM3812": "ym3812",
    "BurnYMF262": "ymf262", "BurnYMF278B": "ymf278b", "YMZ280B": "ymz280b",
    "AY8910": "ay8910", "SN76496": "sn76496", "MSM5205": "msm5205",
    "MSM5232": "msm5232", "MSM6295": "msm6295", "DAC": "dac",
    "BurnSample": "samples", "Pokey": "pokey", "tms5220": "tms5220",
    "ES5506": "es5506", "SAA1099": "saa1099", "QS1000": "qs1000",
    "BurnWatchdog": "watchdog", "IremGA20": "iremga20", "K053260": "k053260",
    "K051649": "k051649", "K054539": "k054539", "C140": "c140",
    "SegaPCM": "segapcm", "RF5C68": "rf5c68", "QSound": "qsound",
}

bdir = next((d for d in ["build-rls-sdl2", "build-rls-qt", "build"] if os.path.isdir(os.path.join(d, "src"))), None)

def demangled_deps(grp, fn):
    op = os.path.join(bdir or "", "src/burn/drv/libburn_drv.a.p", f"{grp}_{fn}.o")
    if not bdir or not os.path.exists(op):
        return None
    raw = subprocess.check_output(["nm", "-u", op], text=True)
    demangled = subprocess.check_output(["c++filt"], input=raw, text=True)
    deps = set()
    for line in demangled.splitlines():
        m = re.search(r"\b([A-Za-z0-9_]+?)(?:Init|Open|Reset|Run|Write|Read)\b", line)
        if m and m.group(1) in API_MAP:
            deps.add(API_MAP[m.group(1)])
    if "ym2203" in deps:
        deps.add("ay8910")
    return sorted(deps)

if args.verify:
    if not bdir:
        print("No build directory found.")
        raise SystemExit(1)
    issues = checked = 0
    for mb in sorted(glob.glob("src/burn/drv/*/meson.build")):
        grp = os.path.basename(os.path.dirname(mb))
        for m in re.finditer(r"^\s*'(d_[^']+\.(?:cpp|c))':\s*\[([^\]]*)\]", open(mb, encoding="utf-8").read(), re.M):
            fn, listed = m.group(1), set(re.findall(r"'([^']+)'", m.group(2)))
            need = demangled_deps(grp, fn)
            if need is None:
                continue
            checked += 1
            miss = sorted(set(need) - listed)
            if miss:
                issues += 1
                print(f"[MISSING] {mb} -> '{fn}' needs {miss}")
    print(f"Checked {checked} compiled drivers: {issues} missing linker dependencies.")
    raise SystemExit(0)

hmap = {f"{v}{sfx}": v for v in set(API_MAP.values()) for sfx in ("_intf.h", ".h")}
hmap.update({f"burn_{v}.h": v for v in set(API_MAP.values())})

def add_entry(fp, line):
    s = open(fp, encoding="utf-8").read()
    idx = s.find("\n}\n") + 1 if "\n}\n" in s else s.rfind("}\n")
    open(fp, "w", encoding="utf-8").write(s[:idx] + line + s[idx:])

added = 0
for st in obj_bases:
    if in_meson(st):
        continue
    for norm in disk.get(st, []):
        fn, ddir = os.path.basename(norm), os.path.dirname(norm)
        if norm.startswith("src/burn/drv/") and fn.startswith("d_"):
            mb = os.path.join(ddir, "meson.build")
            if os.path.exists(mb) and f"'{fn}'" not in open(mb, encoding="utf-8").read():
                deps = demangled_deps(os.path.basename(ddir), fn)
                if deps is None:
                    deps = sorted({hmap[i] for i in re.findall(r'#include\s+"([^"]+)"', open(norm, encoding="latin-1").read()) if i in hmap})
                    if "ym2203" in deps and "ay8910" not in deps:
                        deps = sorted(deps + ["ay8910"])
                add_entry(mb, f"    '{fn}': [{', '.join(repr(x) for x in deps)}],\n")
                print(f"Added {fn} -> {mb}")
                added += 1
if not added:
    print("Meson is 100% in sync with makefile.burn_rules!")
