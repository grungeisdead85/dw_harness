# dw_harness

Offline checks for Detailed Weather's native DLL (grungeisdead's copy). `detailed_weather_dll.cpp` here is the version in play: test 15N (layout 25).

Layout (keep it: the sources find each other by these paths):

    detailed_weather_dll.cpp     the DLL source being checked
    looks.lua                    looks.lua for the paint checks
    build.sh                     builds and runs everything below
    check/
      colsim.cpp                 THE COLOUR (swaps, held slots through a pan), sway, shade, murky, paths, fog over water, pictures checked here
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

Linux, g++: `sh build.sh` from this folder. Or one at a time from `check/`:

    g++ -O2 -std=c++17 -Ishim '-D__declspec(x)=' -o colsim colsim.cpp && ./colsim
