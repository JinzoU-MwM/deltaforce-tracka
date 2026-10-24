#!/usr/bin/env python3
"""analyze_rec.py — baca rec*.log dari probe dfrec, hasilkan kandidat offset senjata.

Pemakaian:
    python scripts/analyze_rec.py C:\\Users\\User\\trackA\\rec2.log

Yang dicari (otomatis, tanpa tuning manual):
  AMMO    int yang turun ~1 per shot lalu reset besar (reload)
  SHOTIDX int yang naik ~1 per shot
  FIRE_T  float yang naik terus saat menembak
  RECOIL  float yang naik saat menembak lalu kembali (akumulator/spring)
  RPM     konstanta di 60..1500 yang tidak berubah (kandidat FireRate statis)

Output: ringkasan + tabel kandidat, plus JSON ke stdout bila --json.
"""
import re
import sys
from collections import defaultdict

CHG = re.compile(r"^CHG off=0x([0-9A-Fa-f]+) (int|flt) ([-\d.]+) -> ([-\d.]+)")
SHOT = re.compile(r"^SHOT (\d+) ctrl=\(([-\d.]+),([-\d.]+)\)")
ROT = re.compile(r"^R dt=([\d.]+) ctrl=\(([-\d.]+),([-\d.]+)\) d=\(([-\d.]+),([-\d.]+)\)")


def parse(path):
    ints = defaultdict(list)
    floats = defaultdict(list)
    shots = []
    deltas = []
    with open(path, "r", errors="ignore") as f:
        for line in f:
            m = CHG.match(line.strip())
            if m:
                off = int(m.group(1), 16)
                if m.group(2) == "int":
                    ints[off].append((int(m.group(3)), int(m.group(4))))
                else:
                    floats[off].append((float(m.group(3)), float(m.group(4))))
                continue
            m = SHOT.match(line.strip())
            if m:
                shots.append(int(m.group(1)))
                continue
            m = ROT.match(line.strip())
            if m:
                deltas.append((float(m.group(1)), float(m.group(4)), float(m.group(5))))
    return ints, floats, shots, deltas


def score_int(seq):
    """Turun 1 per langkah = ammo. Naik 1 per langkah = shot index."""
    downs = ups = 0
    big_jump = 0
    for (a, b) in seq:
        d = b - a
        if d == -1:
            downs += 1
        elif d == 1:
            ups += 1
        elif d > 5:
            big_jump += 1
    return downs, ups, big_jump


def score_float(seq):
    up = down = 0
    for (a, b) in seq:
        d = b - a
        if d > 1e-6:
            up += 1
        elif d < -1e-6:
            down += 1
    return up, down


def main():
    if len(sys.argv) < 2:
        print("usage: analyze_rec.py <rec.log>")
        return 2
    path = sys.argv[1]
    ints, floats, shots, deltas = parse(path)
    print(f"file            : {path}")
    print(f"shot edges      : {len(shots)}")
    print(f"rotation sample : {len(deltas)}")
    if deltas:
        dp = [d[1] for d in deltas if abs(d[1]) > 1e-4]
        if dp:
            print(f"delta pitch     : n={len(dp)} mean={sum(dp)/len(dp):+.4f} "
                  f"min={min(dp):+.4f} max={max(dp):+.4f}")
        dy = [d[2] for d in deltas if abs(d[2]) > 1e-4]
        if dy:
            print(f"delta yaw       : n={len(dy)} mean={sum(dy)/len(dy):+.4f}")
    print()

    print("== kandidat AMMO (int turun 1, reset besar saat reload) ==")
    ammo = []
    for off, seq in ints.items():
        downs, ups, big = score_int(seq)
        if downs >= 3 and downs > ups:
            ammo.append((downs, big, off, len(seq)))
    for downs, big, off, n in sorted(ammo, reverse=True)[:6]:
        print(f"  off=0x{off:X} turun1={downs} reload_jump={big} changes={n}")

    print("== kandidat SHOT INDEX (int naik 1) ==")
    idx = []
    for off, seq in ints.items():
        downs, ups, big = score_int(seq)
        if ups >= 3 and ups > downs:
            idx.append((ups, off, len(seq)))
    for ups, off, n in sorted(idx, reverse=True)[:6]:
        print(f"  off=0x{off:X} naik1={ups} changes={n}")

    print("== kandidat FLOAT naik terus (timer/fire time) ==")
    fl = []
    for off, seq in floats.items():
        up, down = score_float(seq)
        if up >= 3:
            fl.append((up, off, len(seq)))
    for up, off, n in sorted(fl, reverse=True)[:6]:
        print(f"  off=0x{off:X} naik={up} changes={n}")

    print("== kandidat RECOIL (float naik lalu turun) ==")
    rc = []
    for off, seq in floats.items():
        up, down = score_float(seq)
        if up >= 2 and down >= 2:
            rc.append((min(up, down), off, up, down))
    for sc, off, up, down in sorted(rc, reverse=True)[:6]:
        print(f"  off=0x{off:X} naik={up} turun={down}")

    if not ints and not floats:
        print("\nTidak ada data CHG. Pastikan probe terpasang dan sudah menembak.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
