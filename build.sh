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
g++ $F fogleaves/fogleaves.cpp -o fogleaves/fogleaves && ./fogleaves/fogleaves | tail -1 && ./fogleaves/fogleaves perf | tail -2 | head -1   # the fog and the leaves (15X) against the Lua
cd crack                                                        # DRY CRACKS (15O)
g++ $F -I../shim crackjob.cpp -o crackjob && ./crackjob | tail -1     # crack fields through a whole PaintJob
g++ $F -I../shim crackperf.cpp -o crackperf && ./crackperf | tail -1  # time per tile
g++ $F -I../shim crackcheck.cpp -o crackcheck                        # (run by compare.py)
# bit for bit against frost.lua's own cracked(), from a Detailed Weather copy (needs lupa and Pillow):
#   DW=/path/to/detailed_weather_grungeisdead sh build.sh
[ -n "$DW" ] && python3 compare.py "$DW/scripts_modactive/internal/detailed_weather/frost.lua" "$DW/art" | tail -2 || true
cd ..
cd crackplace                                                   # CRACKS IN PLACE (15R)
g++ -O1 -std=c++17 -I../shim -w '-D__declspec(x)=' -shared -fPIC placelib.cpp -o placelib.so   # the DLL as a library (run by place.py)
# frost.lua's own NATIVE CRACKS, colour.lua's native_template and looks.lua's numbers against it, and native.lua's
# offsets (needs lupa):
#   DW=/path/to/detailed_weather_grungeisdead sh build.sh
[ -n "$DW" ] && python3 place.py "$DW/scripts_modactive/internal/detailed_weather/frost.lua" "$DW/scripts_modactive/internal/detailed_weather/native.lua" "$DW/scripts_modactive/internal/detailed_weather/colour.lua" ../../looks.lua | tail -2 || true
cd ..
g++ $F -DPERF -DPERF_FRAMES=200 -DFILL=3 colsim.cpp -o colsim_perf && ./colsim_perf | tail -2   # a big view timed: the colour, and cracks in place
cd paint
g++ -O2 -std=c++17 -I../shim -w '-D__declspec(x)=' paintcheck.cpp -o paintcheck   # (run by compare.py, which feeds it)
g++ -O2 -std=c++17 -I../shim -w '-D__declspec(x)=' fullcheck.cpp -o fullcheck     # (run by compare_full.py)
# bit for bit against the real looks.lua (needs: pip install lupa)
python3 compare.py | tail -2 || true
python3 compare_full.py | tail -2 || true
