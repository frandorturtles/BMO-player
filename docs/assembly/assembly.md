# Assembly manual
Tools: soldering iron, 1.5 mm and 2 mm drill (for cleaning holes), M2 screwdriver, flush cutters, hot glue / CA glue, Kapton tape, multimeter.

1. **Bench-test first.** Wire everything loose on a table and run the firmware before closing the case.
2. **Prepare modules.** Remove pin headers (solder wires directly). Set PCM5102A pads (SCK->GND, XSMT high). Set the boost to 5.0 V and measure with a multimeter BEFORE connecting the ESP32.
3. **Keys.** Insert each key from the inside of the front shell: shaft through its hole, flange behind the wall.
4. **Switch tray.** Drop a tact switch in each ring pocket (plunger towards the front), solder one wire to a leg of each switch pair and a common ground wire to the other legs on the back of the tray. Screw the tray on the 4 short bosses with M2 x 6.
5. **Display.** Slide the display into the pocket (glass against the front wall), wires through the left notch; secure with Kapton tape under the tabs.
6. **ESP32.** Place on the ledges with the micro-USB end towards the left opening; add a 1 mm foam pad on top.
7. **Module frames.** Glue the printed frames to the back of the tray (CA glue), then set TP4056, boost, DAC and microSD into them with double-sided tape. Connectors must face their openings.
8. **Power.** Glue the slide switch behind its slot. Wire per wiring.md. Keep LiPo wires short; insulate with Kapton.
9. **Close.** LiPo into the back-shell cradle (tape), check no wire is pinched by the lip, close and drive 4 x M2 x 18 from the back.
10. **First power-up:** switch on, check 5.0 V at ESP32 VIN, then insert a microSD with MP3 files in `/music`.
