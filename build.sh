#!/bin/sh
# Builds and runs Detailed Weather's native checks on Linux (g++). Run from this folder.
# detailed_weather_dll.cpp here is the DLL source being checked (and looks.lua for the paint checks).
set -e
F="-O2 -std=c++17 -Ishim -w -D__declspec(x)="
cd check
g++ $F colsim.cpp -o colsim && ./colsim | tail -1              # THE COLOUR, held slots, sway, shade, murky, paths, fog over water
g++ $F sim.cpp -o sim && for m in 0 1 2 3; do ./sim $m | tail -1; done   # the snow against four redraw habits
g++ -O2 maths.cpp -o maths && ./maths | tail -1                # the snow maths against snow_ramps.lua's
g++ $F rainsim.cpp -o rainsim && ./rainsim | tail -1           # rain and snow falling against find_rain, rain_tile, snow_tile
g++ $F fogperf/fogperf.cpp -o fogperf/fogperf && ./fogperf/fogperf | tail -1   # fog over water timing
g++ $F slotperf.cpp -o slotperf && ./slotperf | head -1        # held slots rebuild timing
cd paint
g++ -O2 -std=c++17 -I../shim -w '-D__declspec(x)=' paintcheck.cpp -o paintcheck   # (run by compare.py, which feeds it)
g++ -O2 -std=c++17 -I../shim -w '-D__declspec(x)=' fullcheck.cpp -o fullcheck     # (run by compare_full.py)
# bit for bit against the real looks.lua (needs: pip install lupa)
python3 compare.py | tail -2 || true
python3 compare_full.py | tail -2 || true
