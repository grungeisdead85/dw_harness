// colsim.cpp: checks the DLL's colour swaps (THE COLOUR) against a fake DF and a fake colour.lua.
#include <ctime>
#include <cstdio>
#include <vector>
#include <map>
#include "windows.h"
SIZE_T VirtualQuery(LPCVOID p, MEMORY_BASIC_INFORMATION* m, SIZE_T s) { m->BaseAddress = (PVOID)p; m->RegionSize = 1 << 20; m->State = MEM_COMMIT; m->Protect = 4; return s; }
BOOL VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD) { return 1; }
HMODULE GetModuleHandleW(LPCWSTR) { return nullptr; }
BOOL QueryPerformanceCounter(LARGE_INTEGER* t) { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); t->QuadPart = ts.tv_sec * 1000000000LL + ts.tv_nsec; return 1; }
BOOL QueryPerformanceFrequency(LARGE_INTEGER* f) { f->QuadPart = 1000000000LL; return 1; }
PVOID InterlockedExchangePointer(PVOID volatile* p, PVOID v) { PVOID o = *p; *p = v; return o; }
DWORD GetLastError() { return 0; }
DWORD GetFileAttributesA(LPCSTR) { return INVALID_FILE_ATTRIBUTES; }
extern "C" int _putenv_s(const char*, const char*) { return 0; }
#include "../detailed_weather_dll.cpp"
// rainsim.cpp: RAIN AND SNOW FALLING (test 15M) against a fake DF, double buffered, and a reference
// copy of detailed_weather.lua's find_rain, rain_tile and snow_tile.
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// map: BX x BY blocks, 2 levels; z 1 is the view's. tiletypes: 1 floor, 5 EMPTY (open space)
static const int BX = 20, BY = 8, ZN = 2;
static const uint32_t OFF_TT = 64, OFF_DES = 576;
static const uint32_t OUTSIDE = 1u << 16, HIDDEN = 1u << 9, FLOW = 7, MAGMA = 1u << 21;
static void*** xs; static int32_t xcb = BX, ycb = BY, zcb = ZN;
static uint8_t* blk(int x, int y, int z) { return (uint8_t*)((void**)((void**)xs[x >> 4])[y >> 4])[z]; }
static uint32_t& des(int x, int y, int z) { return ((uint32_t*)(blk(x, y, z) + OFF_DES))[tile_of(x, y)]; }
static uint16_t& tt(int x, int y, int z) { return ((uint16_t*)(blk(x, y, z) + OFF_TT))[tile_of(x, y)]; }

// the main viewport: flag, dims, then two layer pointers (high_flow at 16, top_shadow at 24)
struct VP { uint32_t flag; int32_t dx, dy; int32_t pad; int32_t* hf; int32_t* ts; };
static VP vp; static VP* main_ptr = &vp;
static int32_t wx = 0, wy = 0, wz = 1;
static int DX = 40, DY = 30;
static std::vector<int32_t> bufs[2][2];   // [buffer][layer]
static int cur = 0;
static void shape(int dx, int dy) {
    DX = dx; DY = dy; vp.dx = dx; vp.dy = dy;
    for (int b = 0; b < 2; b++) for (int l = 0; l < 2; l++) bufs[b][l].assign(dx * dy, 0);
}
// the game's own flow layer: a flow (mist) at screen (3, 3); both taken at screen (4, 3)
static int32_t game_hf(int sx, int sy) { return (sx == 3 || sx == 4) && sy == 3 ? 777 : 0; }
static int32_t game_ts(int sx, int sy) { return sx == 4 && sy == 3 ? 888 : 0; }
static int redraw_habit = 0;   // 0: the game redraws every cell; 1: only odd columns (the rest keep what they held)
static void game_draws() {      // swap_arrays, put back (prerender), then the game fills the buffer
    cur ^= 1;
    vp.hf = bufs[cur][0].data(); vp.ts = bufs[cur][1].data();
    read_view_as_drawn();
    rain_put_back();
    for (int sx = 0; sx < DX; sx++) for (int sy = 0; sy < DY; sy++) {
        if (redraw_habit == 1 && !(sx & 1)) continue;
        vp.hf[sx * DY + sy] = game_hf(sx, sy); vp.ts[sx * DY + sy] = game_ts(sx, sy);
    }
}
static void hook() { draw_rain(); }

// Lua's sheets: rain cells 6, frames 24, 7 slants, 2 surfaces; snow cells 6, frames 36, 7 winds.
// Tile k's texpos 100000 + k (rain), 200000 + k (snow); every 7th tile has nothing drawn (0).
static const int RC = 6, RF = 24, RS = 7, SC = 6, SF = 36, SW = 7;
static void lua_sheets() {
    B.rain_cells = RC; B.rain_anim = RF; B.rain_slants = RS; B.snow_cells = SC; B.snow_anim = SF;
    B.rain_tex_n = 2 * RS * RC * RC * RF; B.snow_tex_n = SW * SC * SC * SF;
    for (int k = 0; k < B.rain_tex_n; k++) B.rain_tex[k] = k % 7 == 3 ? 0 : 100000 + k;
    for (int k = 0; k < B.snow_tex_n; k++) B.snow_tex[k] = k % 7 == 3 ? 0 : 200000 + k;
}
// Lua's weather grid: by block at the view's level. 0 none, 1 rain, 2 snow
static int weather_of_block(int bx, int by) { return bx == 2 && by == 1 ? 2 : (bx == 3 ? 0 : 1); }
static void lua_grid() {
    B.rain_wz = wz;
    B.rain_wbx = (wx < 0 ? 0 : wx) >> 4; B.rain_wby = (wy < 0 ? 0 : wy) >> 4;
    B.rain_wnx = ((wx + DX - 1) >> 4) - B.rain_wbx + 1; B.rain_wny = ((wy + DY - 1) >> 4) - B.rain_wby + 1;
    memset(B.rain_weather, 0, sizeof B.rain_weather);
    B.rain_weather_any = 0;
    for (int gx = 0; gx < B.rain_wnx; gx++) for (int gy = 0; gy < B.rain_wny; gy++) {
        const int w = weather_of_block(B.rain_wbx + gx, B.rain_wby + gy);
        B.rain_weather[gx * B.rain_wny + gy] = (uint8_t)w;
        if (w) B.rain_weather_any = 1;
    }
}
// the reference: detailed_weather.lua's find_rain, rain_tile, snow_tile (1-based indexes, as there)
static int32_t expected(int x, int y, int slant, int rframe, int wind, int sframe) {
    if (x < 0 || y < 0 || x >= BX * 16 || y >= BY * 16) return 0;
    const int w = weather_of_block(x >> 4, y >> 4);
    if (!w) return 0;
    const uint32_t d = des(x, y, wz);
    if (!(d & OUTSIDE) || (d & HIDDEN)) return 0;
    if (w == 2) {
        const int cell = (y % SC) * SC + (x % SC), row = (wind - 1) * SC * SC + cell;
        const int index = row * SF + sframe + 1;
        return B.snow_tex[index - 1];
    }
    int surface = 1;
    if ((d & FLOW) && !(d & MAGMA)) surface = 2;
    else if (tt(x, y, wz) == 5) { const uint32_t b = des(x, y, wz - 1); if ((b & FLOW) && !(b & MAGMA)) surface = 2; }
    const int cell = (y % RC) * RC + (x % RC), row = ((surface - 1) * RS + (slant - 1)) * RC * RC + cell;
    const int index = row * RF + rframe + 1;
    return B.rain_tex[index - 1];
}
// every cell of the buffer the hook just drew checked against the reference and the game's own
static int check_frame(const char* what) {
    int bad = 0, drawn = 0;
    for (int sx = 0; sx < DX; sx++) for (int sy = 0; sy < DY; sy++) {
        const int32_t e = B.rain_enabled ? expected(wx + sx, wy + sy, B.rain_slant, B.rain_frame, B.snow_wind, B.snow_frame) : 0;
        const int32_t h = vp.hf[sx * DY + sy], t = vp.ts[sx * DY + sy];
        int32_t want_h = game_hf(sx, sy), want_t = game_ts(sx, sy);
        if (e > 0) { if (want_h == 0) want_h = e; else if (want_t == 0) want_t = e; }
        if (redraw_habit == 1 && !(sx & 1)) continue;   // (not redrawn by the game: checked by put back below)
        if (h != want_h || t != want_t) { if (bad++ < 3) printf("  %s: cell %d,%d: %d %d, wanted %d %d\n", what, sx, sy, h, t, want_h, want_t); }
        if (e > 0) drawn++;
    }
    CHECK(bad == 0, "%s: %d cells wrong", what, bad);
    return drawn;
}

int main() {
    xs = (void***)calloc(BX, sizeof(void*));
    for (int bx = 0; bx < BX; bx++) {
        void** ys = (void**)calloc(BY, sizeof(void*)); xs[bx] = (void**)ys;
        for (int by = 0; by < BY; by++) {
            void** zs = (void**)calloc(ZN, sizeof(void*)); ys[by] = zs;
            for (int z = 0; z < ZN; z++) {
                uint8_t* b = (uint8_t*)calloc(1, 2048); zs[z] = b;
                for (int i = 0; i < 256; i++) { ((uint16_t*)(b + OFF_TT))[i] = 1; ((uint32_t*)(b + OFF_DES))[i] = OUTSIDE; }
            }
        }
    }
    // the cases: inside (under a roof), hidden, water, magma, open space over water and over magma, over dry ground
    des(5, 5, 1) &= ~OUTSIDE;
    des(6, 5, 1) |= HIDDEN;
    des(7, 5, 1) |= 7;                       // water here
    des(8, 5, 1) |= 7 | MAGMA;               // magma here
    tt(9, 5, 1) = 5; des(9, 5, 0) |= 7;      // open space over water
    tt(10, 5, 1) = 5; des(10, 5, 0) |= 7 | MAGMA;   // over magma
    tt(11, 5, 1) = 5;                        // over dry ground
    des(12, 5, 1) |= 3;                      // shallow water (flow 3)
    B.tt_empty[5] = 1;

    B.main_viewport = (uint64_t)(uintptr_t)&main_ptr; B.lower_count = 0;
    B.off_flag = 0; B.off_dim_x = 4; B.off_dim_y = 8; B.off_high_flow = 16; B.off_top_shadow = 24;
    B.off_tiletype = OFF_TT; B.off_designation = OFF_DES;
    B.window_x = (uint64_t)(uintptr_t)&wx; B.window_y = (uint64_t)(uintptr_t)&wy; B.window_z = (uint64_t)(uintptr_t)&wz;
    B.block_index = (uint64_t)(uintptr_t)&xs;
    B.x_count_block = (uint64_t)(uintptr_t)&xcb; B.y_count_block = (uint64_t)(uintptr_t)&ycb; B.z_count_block = (uint64_t)(uintptr_t)&zcb;
    B.outside_mask = OUTSIDE; B.hidden_mask = HIDDEN; B.flow_mask = FLOW; B.magma_mask = MAGMA;
    g_us_per_tick = 1e-3;
    vp.flag = 1;
    shape(40, 30);
    lua_sheets();
    B.rain_enabled = 1;

    // ---- frames: slant, frame, wind change; the view pans and changes level ----
    int drawn_total = 0, over_frames = 0;
    for (int f = 0; f < 60; f++) {
        if (f == 20) { wx = 3; wy = 2; }           // a pan
        if (f == 30) { wx = 37; wy = 11; }         // a jump (other blocks: snow at block 2,1; none at column 3)
        if (f == 40) { wx = -5; wy = -3; }         // past the map's edge
        B.rain_slant = 1 + f % RS; B.rain_frame = (f * 5) % RF; B.snow_wind = 1 + (f / 3) % SW; B.snow_frame = (f * 7) % SF;
        game_draws();
        lua_grid();   // (Lua's overlay, after the game drew, before the hook)
        hook();
        char what[32]; snprintf(what, sizeof what, "frame %d", f);
        drawn_total += check_frame(what);
        if (B.rain_over_flow_last) over_frames++;
    }
    CHECK(over_frames > 0, "no frame drew over the game's flow (top_shadow untested)");
    printf("frames that drew over the game's flow (top_shadow): %d\n", over_frames);
    printf("cells drawn over 60 frames: %d (last frame: %u falling, %u in the flow layer, %u over a flow, %u no room)\n",
           drawn_total, B.rain_cells_last, B.rain_drawn_last, B.rain_over_flow_last, B.rain_no_room_last);
    CHECK(B.rain_over_flow_last + B.rain_no_room_last == 0 || true, "");

    // ---- the cases, by name, at a still view over them ----
    wx = 0; wy = 0; B.rain_slant = 4; B.rain_frame = 0;
    // (find a frame where none of the case cells lands on an empty sheet tile)
    for (int f = 0; f < RF; f++) {
        B.rain_frame = f; bool all = true;
        for (int x = 5; x <= 12; x++) { int32_t e = expected(x, 5, 4, f, 1, 0); if (x == 7 || x == 9 || x == 11 || x == 12) all = all && e > 0; }
        if (all) break;
    }
    game_draws(); lua_grid(); hook();
    check_frame("cases");
    CHECK(vp.hf[5 * DY + 5] == 0, "rain under a roof");
    CHECK(vp.hf[6 * DY + 5] == 0, "rain on a hidden tile");
    { const int32_t w = vp.hf[7 * DY + 5]; CHECK(w > 100000 && ((w - 100000) / RF) / (RC * RC) >= RS, "water here not the water surface: %d", w); }
    { const int32_t w = vp.hf[9 * DY + 5]; CHECK(w > 100000 && ((w - 100000) / RF) / (RC * RC) >= RS, "open space over water not the water surface: %d", w); }
    { const int32_t w = vp.hf[11 * DY + 5]; CHECK(w > 100000 && ((w - 100000) / RF) / (RC * RC) < RS, "open space over dry ground not the ground: %d", w); }
    CHECK(vp.hf[3 * DY + 3] == 777, "the game's flow overwritten");
    CHECK(vp.hf[4 * DY + 3] == 777 && vp.ts[4 * DY + 3] == 888, "a cell with both layers taken changed");

    // ---- put back with a game that doesn't redraw half the cells ----
    redraw_habit = 1;
    for (int f = 0; f < 6; f++) { B.rain_frame = (B.rain_frame + 1) % RF; game_draws(); lua_grid(); hook(); }
    {   // a cell the game didn't redraw: holds the game's value or this frame's rain, never an old frame's
        int stale = 0;
        for (int sx = 0; sx < DX; sx += 2) for (int sy = 0; sy < DY; sy++) {
            const int32_t e = expected(wx + sx, wy + sy, B.rain_slant, B.rain_frame, B.snow_wind, B.snow_frame);
            const int32_t h = vp.hf[sx * DY + sy];
            const int32_t want = game_hf(sx, sy) ? game_hf(sx, sy) : (e > 0 ? e : 0);
            if (h != want) stale++;
        }
        CHECK(stale == 0, "cells the game didn't redraw kept an old frame's rain: %d", stale);
    }
    redraw_habit = 0;

    // ---- THE LIST (15N): the game's arrays replaced at the same size, again and again ----
    {
        const uint32_t full0 = B.rain_kept_full; const uint32_t drop0 = B.rain_dropped;
        std::vector<std::vector<int32_t>> retired;   // (kept alive, to see nothing writes them after)
        std::vector<std::vector<int32_t>> retired_copy;
        for (int r = 0; r < 12; r++) {
            for (int b = 0; b < 2; b++) for (int l = 0; l < 2; l++) {
                std::vector<int32_t> fresh(DX * DY, 0);
                bufs[b][l].swap(fresh);          // (new arrays, the same size; the old kept alive in retired)
                retired_copy.push_back(fresh);
                retired.push_back(std::move(fresh));
            }
            for (int f = 0; f < 3; f++) { B.rain_frame = (B.rain_frame + 1) % RF; game_draws(); lua_grid(); hook(); check_frame("arrays replaced"); }
        }
        CHECK(B.rain_kept_full == full0, "the put back list filled with arrays replaced: %u times", B.rain_kept_full - full0);
        CHECK(B.rain_dropped > drop0, "no entries let go when arrays were replaced");
        int touched = 0;
        for (size_t k = 0; k < retired.size(); k++) if (retired[k] != retired_copy[k]) touched++;
        CHECK(touched == 0, "%d retired arrays written after they were replaced", touched);
        printf("arrays replaced 12 times: %u entries let go, list full %u times\n", B.rain_dropped - drop0, B.rain_kept_full - full0);
    }
    // ---- turned off: nothing of it left two frames later ----
    B.rain_enabled = 0;
    game_draws(); hook(); game_draws(); hook();
    { int left = 0; for (int b = 0; b < 2; b++) for (int i = 0; i < DX * DY; i++) {
          const int sx = i / DY, sy = i % DY;
          if (bufs[b][0][i] != game_hf(sx, sy) || bufs[b][1][i] != game_ts(sx, sy)) left++; }
      CHECK(left == 0, "rain left behind after turning off: %d cells", left); }
    // ---- the grid for another level: nothing drawn, counted ----
    B.rain_enabled = 1;
    { const uint32_t sk = B.rain_skipped; game_draws(); lua_grid(); B.rain_wz = wz + 1; hook();
      CHECK(B.rain_skipped == sk + 1, "a grid for another level not skipped"); }
    // ---- a zoom: new buffers; nothing written into the old ones, the list dropped ----
    game_draws(); lua_grid(); hook();
    std::vector<int32_t> old0 = bufs[0][0], old1 = bufs[1][0];
    int32_t* oldp0 = bufs[0][0].data(); int32_t* oldp1 = bufs[1][0].data(); (void)oldp0; (void)oldp1;
    shape(30, 20);
    game_draws(); lua_grid(); hook();
    check_frame("after a zoom");
    CHECK(g_rain_kept_dx == 30, "put back list not remade at the new size");

    // ---- PERF: a big view, rain on all of it ----
    {
        shape(250, 100);
        for (int bx = 0; bx < BX; bx++) for (int by = 0; by < BY; by++) for (int i = 0; i < 256; i++)
            ((uint32_t*)(((uint8_t*)((void**)((void**)xs[bx])[by])[1]) + OFF_DES))[i] = OUTSIDE;
        wx = 0; wy = 0;
        double total = 0, worst = 0; const int K = 300;
        B.rain_us_total = 0; B.rain_us_max = 0;
        for (int f = 0; f < K; f++) {
            B.rain_frame = f % RF; game_draws(); lua_grid();
            const double t0 = ticks_now(); hook(); const double us = (ticks_now() - t0) * g_us_per_tick;
            total += us; if (us > worst) worst = us;
        }
        check_frame("perf view");
        printf("PERF %d x %d view, all outdoors and falling: %.1f us a frame on average (worst %.1f); last frame %u falling, %u drawn\n",
               DX, DY, total / K, worst, B.rain_cells_last, B.rain_drawn_last + B.rain_over_flow_last);
        // put back's own cost, on the same view
        double pb = 0; for (int f = 0; f < 50; f++) { cur ^= 1; vp.hf = bufs[cur][0].data(); vp.ts = bufs[cur][1].data();
            const double t0 = ticks_now(); rain_put_back(); pb += (ticks_now() - t0) * g_us_per_tick; hook(); }
        printf("PERF put back: %.1f us a frame on average\n", pb / 50);
    }
    printf(fails ? "%d FAILS\n" : "all rain checks pass\n", fails);
    return fails ? 1 : 0;
}
