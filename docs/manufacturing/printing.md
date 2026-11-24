# Printing
| File | Orientation (already modelled) | Material | Notes |
|---|---|---|---|
| bmo_front_shell.stl | face down | PLA/PETG mint | 0.2 mm layers, 3 perimeters, no supports needed (display tabs are small 1 mm overhangs) |
| bmo_back_shell.stl | back face down | same | text is engraved 0.6 mm |
| bmo_switch_tray.stl | plate down, rings up | any | |
| bmo_module_frames.stl | flat | any | 4 frames, glue to tray |
| bmo_key_*.stl | top face down | yellow (cross), blue, green, red | print 0.12 mm layers for the caps |
Tolerance: holes were designed with 0.25 mm clearance per side; the STLs are meshes at 0.40 mm (shells) / 0.25 mm (small parts); STL files kept under 20 MB so they fit the GitHub web upload limit (25 MB per file) voxel resolution. For finer meshes edit `H_SHELL`/`H_SMALL` in bmo_case.py (file size grows quickly).
