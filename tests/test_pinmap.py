"""Checks firmware/src/pins.h against electronics/pinout.csv and ESP32 GPIO rules."""
import csv, os, re, sys
root = os.path.join(os.path.dirname(__file__), "..")
pins = {m.group(1): int(m.group(2)) for m in re.finditer(r"#define\s+PIN_(\w+)\s+(-?\d+)", open(f"{root}/firmware/src/pins.h").read())}
rows = list(csv.DictReader(open(f"{root}/electronics/pinout.csv")))
errs = []
csv_map = {r["signal"]: int(r["esp32_gpio"]) for r in rows}
for sig, gpio in csv_map.items():
    key = {"VBAT_SENSE": "VBAT"}.get(sig, sig)
    key = key.replace("I2S_", "I2S_")
    if key not in pins: errs.append(f"{sig}: missing in pins.h")
    elif pins[key] != gpio: errs.append(f"{sig}: pins.h={pins[key]} csv={gpio}")
used = [g for g in csv_map.values()]
if len(used) != len(set(used)): errs.append("duplicate GPIO in pinout.csv")
flash = {6, 7, 8, 9, 10, 11}
input_only = {34, 35, 36, 39}
for sig, g in csv_map.items():
    if g in flash: errs.append(f"{sig}: GPIO{g} is flash")
    if g in input_only and not sig.startswith("VBAT"): errs.append(f"{sig}: GPIO{g} is input-only/no pull-up")
    if g in (0, 12): errs.append(f"{sig}: strapping pin {g} avoided by design")
    if sig.startswith("BTN") and g in input_only: errs.append(f"{sig}: no internal pull-up on GPIO{g}")
print(f"{len(csv_map)} signals checked")
print("OK" if not errs else "\n".join(errs))
sys.exit(1 if errs else 0)
