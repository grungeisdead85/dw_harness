# dw_harness

Offline checks for Detailed Weather's native DLL (grungeisdead's copy). `detailed_weather_dll.cpp` here is the version in play: test 15U (layout 32).

Layout (keep it: the sources find each other by these paths):

    detailed_weather_dll.cpp     the DLL source being checked
    looks.lua                    looks.lua for the paint checks
    build.sh                     builds and runs everything below
    check/
      colsim.cpp                 THE COLOUR (swaps, held slots through a pan), sway, shade, murky, paths, fog over water, pictures checked here,
                                 CRACKS IN PLACE (tiles worked out from the map by frost.lua's rules: a floor, a dirt ramp, seen down
                                 through open space, cold, and not: mild, under a wall, water, underground, a grass ramp; copies asked
                                 for, placed, the look before's held, the next look's asked for ahead, a ramp held under a new number,
                                 rechecked when due, worked out again when the settings change or the map's size does, stopped, kept
                                 over the colour's reset, forgotten on a new world); built with -DPERF -DPERF_FRAMES=200 -DFILL=3 it
                                 times a 250 x 100 view, the colour and then the cracks (the first frame, every block worked out, apart)
      sim.cpp                    the snow, frame by frame, against four redraw habits (./sim 0 to 3; "perf", "perf nosnow"; env HEAVY=1, UP, W, H)
      maths.cpp                  the snow maths against snow_ramps.lua's
      rainsim.cpp                rain and snow falling against a reference copy of find_rain, rain_tile and snow_tile: pans, a jump, the map's
                                 edge, roofed, hidden, water, magma, open space over water, the game's flow kept, put back with a game redrawing
                                 half the cells, the game's arrays replaced at the same size, turning off, a grid for another level, a zoom, timing
      slotperf.cpp               held slots rebuild timing on panning frames
      offs.cpp                   block offsets
      shim/windows.h             stands in for Windows
      fogperf/fogperf.cpp        fog over water timing
      paint/                     the DLL's painting against looks.lua (paintcheck, fullcheck; compare.py, compare_full.py need lupa)
      crackplace/                CRACKS IN PLACE (15R to 15U): placelib.cpp builds the DLL as a library over a fake DF with a map, and
                                 place.py drives it with frost.lua's own NATIVE CRACKS section (cut from the real file, run in lupa)
                                 through views made as native.lua makes them, over native.lua's OFF (checked against the DLL's struct
                                 first): frost.lua's rules by tiletype handed over, tiles worked out from the map (and those that
                                 mustn't crack), keys, a change of look held over, a trickle, back to a look, a dry look ending, a
                                 forced temperature, the next look ahead. Give it frost.lua and native.lua, or run build.sh with DW set
      crack/                     DRY CRACKS (15O): crackjob (the crack fields through a whole PaintJob), crackperf (time a
                                 tile), and compare.py: the DLL's crack_in_place against frost.lua's own cracked(), cut from
                                 the real file, bit for bit (needs lupa and Pillow; give it frost.lua and the art folder, or
                                 run build.sh with DW set to a Detailed Weather copy)

Linux, g++: `sh build.sh` from this folder. Or one at a time from `check/`:

    g++ -O2 -std=c++17 -Ishim '-D__declspec(x)=' -o colsim colsim.cpp && ./colsim
