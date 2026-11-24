"""Sanity checks for the STL files: size, watertightness (every edge shared by 2 faces), positive volume."""
import json, os, sys
import numpy as np
root = os.path.join(os.path.dirname(__file__), "..")
stl = f"{root}/3d-models/STL"
expect = {"bmo_front_shell.stl": (76, 117.5, 13), "bmo_back_shell.stl": (76, 110, 14.8), "bmo_switch_tray.stl": (71.2, 54.2, 5.3),
          "bmo_module_frames.stl": (61.1, 54.4, 3.0), "bmo_key_up.stl": (10.4, 10.4, 4.0)}
bad = 0
for f in sorted(os.listdir(stl)):
    if not f.endswith(".stl"): continue
    d = open(f"{stl}/{f}", "rb").read()
    n = int(np.frombuffer(d[80:84], "<u4")[0])
    rec = np.frombuffer(d[84:], dtype=[("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")], count=n)
    v = rec["v"].reshape(-1, 3)
    _, inv = np.unique(np.round(v, 2), axis=0, return_inverse=True)
    inv = inv.reshape(-1, 3)
    inv = inv[(inv[:, 0] != inv[:, 1]) & (inv[:, 1] != inv[:, 2]) & (inv[:, 0] != inv[:, 2])]   # drop degenerate (zero-area) tris
    e = np.sort(np.concatenate([inv[:, [0, 1]], inv[:, [1, 2]], inv[:, [2, 0]]]), axis=1)
    _, cnt = np.unique(e, axis=0, return_counts=True)
    open_edges = int((cnt != 2).sum())
    t = rec["v"].astype(np.float64)
    vol = float(np.einsum("ij,ij->i", t[:, 0], np.cross(t[:, 1], t[:, 2])).sum() / 6)
    size = v.max(0) - v.min(0)
    ok = open_edges == 0 and vol > 0
    if f in expect: ok = ok and np.allclose(size, expect[f], atol=0.3)
    print(f"{f:26s} tris={n:7d} size={np.round(size,1)} vol={vol:9.1f} open_edges={open_edges} {'OK' if ok else 'FAIL'}")
    bad += (not ok)
fit = json.load(open(f"{root}/tests/fit_report.json"))["fit"]
clash = {k: v for k in ("module_vs_solid_mm3", "module_vs_module_mm3", "part_vs_part_mm3") for v in [fit[k]] if v}
print("fit report:", "no interference" if not clash else clash)
sys.exit(1 if bad or clash else 0)
