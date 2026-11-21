# dw_harness

Offline checks for Detailed Weather's native DLL (grungeisdead's copy). `detailed_weather_dll.cpp` here is the version in play: test 16A, second drop (layout 38).

Layout (keep it: the sources find each other by these paths):

    detailed_weather_dll.cpp     the DLL source being checked
    looks.lua                    looks.lua for the paint checks
    build.sh                     builds and runs everything below
    check/
      colsim.cpp                 THE COLOUR (swaps, held slots through a pan), sway, shade, murky, paths, fog over water, pictures checked here,
                                 PATHS HELD (15Z: a worn tile shows its last painting while a new grass is asked about, with no
                                 blank, and over the frame's painting time, then is painted; let go after P_HOLD_FRAMES; kept when its
                                 block is worked out again, dropped where its wear changed; the paintings' two halves, one found in
                                 the older not painted again; the full list of grass pictures starting over), BY PICTURE (15Z: a
                                 screen copy painted is filed by picture, one not for the screen isn't; a new number of that picture,
                                 padded rows, not answered yet, gets the copy in the same frame and is still asked; another look's
                                 copy isn't used, that look's own is),
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
      offs.cpp                   native.lua's OFF against the DLL's struct (made from native.lua's OFF table)
      look15z/                   THE WAITS TAKEN OUT (15Z): lib15z.cpp builds the DLL's new painting as a library; compare.py
                                 sets it against the real colour.lua (unpremultiplied; clump_offset and leaning, off-centre clumps,
                                 edge pieces, every step, the eight ways; through whole jobs with stale fields) and snow_ramps.lua
                                 (bake_fade, snow_with, snowy_overhang of 1 to 8 pieces), cut from the real files and run in lupa
                                 (Lua 5.3, as DFHack), pixel for pixel; then l_bake_flow, the snow's bakes made here end to end
                                 (painted at once, told to Lua, left be by the colour, kept; a fade to no side; no fill, an
                                 unreadable piece, no blank, the list to tell Lua full and off: asked of Lua; the frame's painting
                                 time used: put off, painted the next frame); l_perf times each painting
      look15z/compare_rustle.py  THE RUSTLE PAINTED HERE (16A): colour.lua's own native_rustle (cut from the real file, run in
                                 lupa) writes each job into memory the DLL paints (lib15z.so), set against colour.lua's own
                                 rustle_frame pixel for pixel: trees and grass, every phase, the three winds, the eight ways;
                                 and the DLL's leaves test against is_leaves, at its threshold too
      lua16a/                    colour_rustle.py (update_sway's trees and grass_wave's grass sent to the DLL, wood not, no
                                 room waits; the canopy listing's SEEN_WOOD), install.py (install.lua over a real scratch
                                 folder: installed, current, updated with the old set aside, a held set-aside name, a newer
                                 copy left alone, a failed write putting the old back, uninstall, once a session at a map
                                 load), stone_tint.py (stone-tint.lua with both Detailed Weathers: give it the file, or run
                                 build.sh with ST set to it)
      wave16a/                   THE GRASS WAVE DRAWN HERE (16A, second drop): wave.py runs colour.lua's own grass_wave twice
                                 over the same frames in lupa, once as drop 1 had it (its own loop over the cells) and once with
                                 the real native_wave and native.lua's colour_view over the DLL's Block (lib_wave.so), where the
                                 DLL draws; every cell of both ground layers must match after every frame: strong wind, a gale
                                 (another kind of rustle), a new set of frames on a diagonal, wind too light to rustle, paused,
                                 the wave off and back. Then the frames handed over filed as their originals, and the DLL's time
                                 on a 200 by 120 view
      gen_offs.py                makes offs.cpp from native.lua's OFF (build.sh does it first when DW is set)
      lua15z/                    native.lua (the snow's start and the bakes told, the shared blanks, hand back; the paths' faster
                                 top-up), colour.lua (native_pixels and native_job's fields, the per-frame shares; build_lean native
                                 and not; the sheet loop) and snow_ramps.lua (the edge art once a world; native_tables(true))
                                 run in lupa with stand-ins. Give each the folder with the three files, or run build.sh with DW set
      shim/windows.h             stands in for Windows
      fogperf/fogperf.cpp        fog over water timing
      paint/                     the DLL's painting against looks.lua (paintcheck, fullcheck; compare.py, compare_full.py need lupa)
      spatter/                   THE SPATTER DRAWN HERE (15Y): spatcells.cpp, which cells the DLL puts in the grass and with what
                                 (fruit on a grass copy thinned by the item spatter fallen, the colour's copy through its original,
                                 blades only where the item layer is free; the snow's, the colour's table copies, what it leaves be,
                                 and spatter off the grass left alone; the same picture painted once; put back); compare.py, the
                                 DLL's painting (spatlib.so) against spatter.lua's own mask_for, bake_spatter, blade_mask and
                                 bake_blades in lupa, pixel for pixel, every share and pattern. Give compare.py spatter.lua, or run
                                 build.sh with DW set
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
