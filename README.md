# dw_harness

Offline checks for Detailed Weather's native DLL (grungeisdead's copy). `detailed_weather_dll.cpp` here is the version in play: test 15X (layout 34).

Layout (keep it: the sources find each other by these paths):

    detailed_weather_dll.cpp     the DLL source being checked
    looks.lua                    looks.lua for the paint checks
    build.sh                     builds and runs everything below
    check/
      colsim.cpp                 THE COLOUR (swaps, held slots through a pan), sway, shade, murky, paths, fog over water, pictures checked here,
                                 CRACKS IN PLACE (tiles worked out from the map by frost.lua's rules, and every rule that stops one;
                                 copies painted in the hook the frame they're needed, each checked pixel for pixel against the same
                                 paint job built by hand; the same picture under a new number with padded rows found; a change of
                                 look painted at once; no blanks: the look before's held, then painted; the next look's painted ahead,
                                 a floor's from the next look's own copy once listed, with the cracks on and off; rechecked when due,
                                 the settings or the map's size changing; stopped; kept over the colour's reset; forgotten on a new
                                 world); built with -DPERF -DPERF_FRAMES=200 -DFILL=3 it times a 250 x 100 view, the colour and then
                                 the cracks (the first frames, every block worked out and every copy painted, apart)
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
      fogleaves/                 THE FOG AND THE LEAVES DRAWN HERE (15X): the DLL's fog and falling leaves and petals against a
                                 straight port of the Lua they replace (detailed_weather.lua: find_outdoors, fog_tile; leaves.lua:
                                 find_over, tree_at by DFHack's getPlantAtTile, decide, sheet_tile), cell for cell over a fake map
                                 (a pond open to the sky and one under a bridge, hidden, underground and indoor tiles, missing
                                 blocks of sky, a roof, trees that shed and that don't, whose canopies meet), at several levels,
                                 drifts east and west, both sheets, winds and frames; then the put back over swapped buffers, the
                                 game's own interface and projectile left alone, a tree that stops shedding, out of season, and a
                                 tree felled then its blocks rechecked. `fogleaves perf` times a 250 x 100 view
      crackplace/                CRACKS IN PLACE (15R to 15V): placelib.cpp builds the DLL as a library over a fake DF with a map,
                                 surfaces, heat patterns and blanks; place.py drives it with frost.lua's own NATIVE CRACKS section,
                                 colour.lua's own native_template and the real looks.lua's numbers (cut from the real files, run in
                                 lupa), through views made as native.lua makes them over native.lua's OFF (checked against the
                                 DLL's struct first). Each ramp copy painted in the hook is checked pixel for pixel against looks.lua's
                                 own redraw with the cracks; the floors, the next look ahead (floors from the next look's own copies),
                                 a ramp's new number, a forced temperature, the copies noted by frost.lua. Give it frost.lua,
                                 native.lua, colour.lua and looks.lua, or run build.sh with DW set
      crack/                     DRY CRACKS (15O): crackjob (the crack fields through a whole PaintJob), crackperf (time a
                                 tile), and compare.py: the DLL's crack_in_place against frost.lua's own cracked(), cut from
                                 the real file, bit for bit (needs lupa and Pillow; give it frost.lua and the art folder, or
                                 run build.sh with DW set to a Detailed Weather copy)

Linux, g++: `sh build.sh` from this folder. Or one at a time from `check/`:

    g++ -O2 -std=c++17 -Ishim '-D__declspec(x)=' -o colsim colsim.cpp && ./colsim
