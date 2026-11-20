// THE FOG AND THE LEAVES DRAWN HERE (test 15X): the DLL's fog and falling leaves against a straight
// port of the Lua they replace (detailed_weather.lua: find_outdoors, fog_tile, the fog drawing;
// leaves.lua: find_over, tree_at, decide, sheet_tile, apply), cell for cell, over a fake map with water,
// a covered pond, hidden and underground tiles, missing blocks of sky, and trees (DFHack's plants: pos,
// material, tree_info's body by level) that shed and that don't, under a roof and in the open. Then the
// put back over two frames of swapped buffers, and the cells the game or an overlay already used.
// Build: g++ -O2 -std=c++17 -I../shim -w '-D__declspec(x)=' fogleaves.cpp -o fogleaves
#include "../colsim_head.h"   // (the Windows stand-ins, then the DLL itself)
#include <vector>
#include <cstdio>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---- the fake map: BXN x BYN blocks, ZN levels; the ground at z 4 ----
static const int BXN = 16, BYN = 8, ZN = 12;
static const uint32_t OFF_TT = 64, OFF_DES = 576;
static const uint16_t T_FLOOR = 1, T_EMPTY = 2, T_RAMPTOP = 3, T_CANOPY = 4, T_TRUNK = 5, T_TWIG = 6;
static const uint32_t OUTSIDE = 1u << 16, SUBT = 1u << 15, HIDDEN = 1u << 9, FLOW = 7;
static void*** xs; static int32_t xcb = BXN, ycb = BYN, zcb = ZN;
static uint8_t* blk(int x, int y, int z) { return (uint8_t*)((void**)((void**)xs[x >> 4])[y >> 4])[z]; }
static uint16_t& tt_at(int x, int y, int z) { return ((uint16_t*)(blk(x, y, z) + OFF_TT))[tile_of(x, y)]; }
static uint32_t& des_at(int x, int y, int z) { return ((uint32_t*)(blk(x, y, z) + OFF_DES))[tile_of(x, y)]; }

// ---- the fake view: the main viewport only (dims at 4 and 8, interface at 80, projectile at 88) ----
struct VP { uint32_t flag; int32_t dx, dy; int32_t pad; int32_t* L[7]; uint64_t* FF; int32_t* IF; int32_t* PJ; };
static VP vp; static VP* main_ptr = &vp; static VP* lower[2] = {nullptr, nullptr};
static int DX = 40, DY = 30;
static int32_t wx = 0, wy = 0, wz = 5;

// ---- the fake plants (DFHack's df::plant, plant_tree_info, map_block_column at made-up offsets) ----
struct FakeTree { uint16_t** body; int32_t height, dim_x, dim_y; };   // body 0, height 8, dim_x 12, dim_y 16
struct FakePlant { int16_t x, y, z; int16_t pad; int16_t material; int16_t pad2[3]; FakeTree* tree; };   // pos 0, material 8, tree 16
struct FakeColumn { uint8_t pad[40]; FakePlant** first; FakePlant** last; FakePlant** end; };   // plants at 40
static FakeColumn columns[BXN][BYN];
static FakeColumn* col_by_y[BXN][BYN];
static FakeColumn** col_index[BXN];
static FakeColumn*** col_index_ptr = col_index;
static std::vector<FakePlant*> plants_of[BXN][BYN];

// a tree: trunk at its own tile on level 0, then canopy levels 1.. by `shape` (dim 5 x 5)
static FakePlant* add_tree(int px, int py, int pz, int raw, int height, const char* shape) {
    FakePlant* p = new FakePlant{}; p->x = px; p->y = py; p->z = pz; p->material = raw;
    FakeTree* t = new FakeTree{}; t->height = height; t->dim_x = 5; t->dim_y = 5;
    t->body = new uint16_t*[height];
    for (int l = 0; l < height; l++) {
        t->body[l] = new uint16_t[25]();
        for (int i = 0; i < 25; i++) {
            const int xi = i % 5, yi = i / 5;
            const bool on = l == 0 ? (xi == 2 && yi == 2) : shape[(l * 7 + i) % 25] == '#';
            if (on) {
                t->body[l][i] = 0x10 | (uint16_t)l;
                const int x = px - 2 + xi, y = py - 2 + yi, z = pz + l;
                tt_at(x, y, z) = l == 0 ? T_TRUNK : ((i + l) % 3 == 0 ? T_TWIG : T_CANOPY);
            }
        }
    }
    p->tree = t;
    plants_of[(px / 48) * 3][(py / 48) * 3].push_back(p);
    return p;
}

// ---- THE LUA, ported straight (the reference) ----
static bool open_tt(uint16_t t) { return t == T_EMPTY || t == T_RAMPTOP; }
// detailed_weather.lua: find_outdoors (one tile): false, or the depth
static int lua_outdoors(int x, int y, int z) {
    if (x < 0 || y < 0 || x >= BXN * 16 || y >= BYN * 16) return -1;   // (isValidTilePos)
    uint8_t* b = blk(x, y, z);
    bool submerged = false;
    if (b && (des_at(x, y, z) & FLOW) && z + 1 < ZN && blk(x, y, z + 1)) {
        submerged = (des_at(x, y, z + 1) & FLOW) || !open_tt(tt_at(x, y, z + 1));
    }
    if (submerged) return -1;
    bool ok = !b || (((des_at(x, y, z) & OUTSIDE) || !(des_at(x, y, z) & SUBT)) && !(des_at(x, y, z) & HIDDEN));
    if (!ok) return -1;
    int depth = 0, level = z;
    while (depth < 8) {
        uint8_t* lb = level >= 0 ? blk(x, y, level) : nullptr;
        if (lb) {
            if (!open_tt(tt_at(x, y, level)) || (des_at(x, y, level) & FLOW)) break;
        }
        depth++; level--;
    }
    return depth;
}
static int lmod(int a, int b) { int r = a % b; return r < 0 ? r + b : r; }
static int ldiv(int a, int b) { return (a - lmod(a, b)) / b; }
// fog_tile: 1-based
static int lua_fog_tile(int patch, int frames, int level, int steps, int x, int y) {
    const int frame = lmod(steps, frames), moved = ldiv(steps, frames);
    const int cx = lmod(x - moved, patch), cy = lmod(y, patch);
    const int row = (level - 1) * patch * patch + cy * patch + cx;
    return row * frames + frame + 1;
}
// leaves.lua: tree_at by DFHack's getPlantAtTile: raw (-1 none) and seed
static int lua_tree_at(int x, int y, int z, int* seed) {
    std::vector<FakePlant*>& v = plants_of[(x / 48) * 3][(y / 48) * 3];
    for (FakePlant* p : v) {
        if (p->x == x && p->y == y && p->z == z) { *seed = (p->x * 7919 + p->y * 104729) % 65521; return p->material; }
        if (!p->tree) continue;
        FakeTree& t = *p->tree;
        const int xi = t.dim_x / 2 - p->x % 48 + x % 48, yi = t.dim_y / 2 - p->y % 48 + y % 48, zd = z - p->z;
        if (xi < 0 || xi >= t.dim_x || yi < 0 || yi >= t.dim_y || zd >= t.height || zd < 0) continue;
        if (t.body[zd][xi + yi * t.dim_x] & 0x7F) { *seed = (p->x * 7919 + p->y * 104729) % 65521; return p->material; }
    }
    return -1;
}
static bool sheds[64];
static bool canopy_tt(uint16_t t) { return t == T_CANOPY || t == T_TWIG; }
static bool tree_tt(uint16_t t) { return t == T_CANOPY || t == T_TWIG || t == T_TRUNK; }
// find_over: raw (-1: false) and seed
static int lua_find_over(int x, int y, int z, int* seed) {
    if (x < 0 || y < 0 || x >= BXN * 16 || y >= BYN * 16) return -1;
    if (blk(x, y, z) && (des_at(x, y, z) & (HIDDEN | SUBT))) return -1;
    for (int level = z; level <= z + 6; level++) {
        if (level >= ZN || !blk(x, y, level)) continue;
        const uint16_t t = tt_at(x, y, level);
        if (tree_tt(t)) {
            if (canopy_tt(t)) {
                int s = 0;
                const int raw = lua_tree_at(x, y, level, &s);
                if (raw >= 0 && sheds[raw]) { *seed = s; return raw; }
                if (raw >= 0) return -1;
            }
        } else if (level > z && !open_tt(t)) return -1;
    }
    return -1;
}
// decide: raw (-1 none) and seed
static int lua_decide(int x, int y, int z, int* seed) {
    int raw = lua_find_over(x, y, z, seed);
    if (raw >= 0) return raw;
    int s[4], r[4];
    r[0] = lua_find_over(x - 1, y, z, &s[0]); r[1] = lua_find_over(x + 1, y, z, &s[1]);
    r[2] = lua_find_over(x, y - 1, z, &s[2]); r[3] = lua_find_over(x, y + 1, z, &s[3]);
    for (int k = 0; k < 4; k++) if (r[k] >= 0) { raw = r[k]; *seed = s[k]; break; }
    if (raw < 0) return -1;
    if (blk(x, y, z)) {
        const uint32_t d = des_at(x, y, z);
        if ((d & HIDDEN) || (d & SUBT) || !(d & OUTSIDE)) return -1;
    }
    return raw;
}
static int lua_sheet_tile(int cells, int frames, int winds, int heaviness, int wind, int x, int y, int frame, int seed) {
    x = x + seed % cells; y = y + seed / cells % cells;
    frame = (frame + seed / (cells * cells)) % frames;
    const int cell = (y % cells) * cells + (x % cells);
    const int row = ((heaviness - 1) * winds + (wind - 1)) * cells * cells + cell;
    return row * frames + frame + 1;
}

int main(int argc, char** argv) {
    const bool perf = argc > 1;
    { LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_us_per_tick = 1e6 / (double)f.QuadPart; }   // (as dfhooks_init)
    if (perf) { DX = 250; DY = 100; }
    // ---- the map ----
    xs = (void***)calloc(BXN, sizeof(void*));
    for (int bx = 0; bx < BXN; bx++) {
        void** ys = (void**)calloc(BYN, sizeof(void*)); xs[bx] = (void**)ys;
        for (int by = 0; by < BYN; by++) {
            void** zs = (void**)calloc(ZN, sizeof(void*)); ys[by] = zs;
            for (int z = 0; z < ZN; z++) {
                if (z >= 10 && bx == 0) continue;   // (open sky with no map block)
                uint8_t* b = (uint8_t*)calloc(1, 4096); zs[z] = b;
                for (int i = 0; i < 256; i++) {
                    ((uint16_t*)(b + OFF_TT))[i] = z <= 4 ? T_FLOOR : T_EMPTY;
                    ((uint32_t*)(b + OFF_DES))[i] = z < 4 ? SUBT : OUTSIDE;
                }
            }
        }
    }
    for (int x = 10; x < 15; x++) for (int y = 10; y < 15; y++) des_at(x, y, 4) |= 4;        // a pond, open to the sky
    for (int x = 10; x < 13; x++) des_at(x, 12, 5) |= 3;                                     // water a level up on it: under water
    des_at(16, 16, 4) |= 5; tt_at(16, 16, 5) = T_FLOOR;                                     // a pond under a bridge
    tt_at(17, 16, 5) = T_RAMPTOP;                                                           // open: a ramp's top
    for (int x = 30; x < 34; x++) des_at(x, 5, 4) |= HIDDEN;                                 // hidden
    for (int x = 40; x < 46; x++) for (int y = 2; y < 5; y++) des_at(x, y, 4) = SUBT;        // underground, not outside
    for (int x = 0; x < 8; x++) for (int y = 20; y < 23; y++) tt_at(x, y, 6) = T_FLOOR;     // a roof over open ground
    for (int x = 15; x < 18; x++) for (int y = 17; y < 25; y++) des_at(x, y, 5) = 0;          // indoors above ground, by a tree
    // trees (raw 3 sheds, raw 7 doesn't, raw 5 sheds): one under the roof, two whose canopies meet
    sheds[3] = sheds[5] = true;
    add_tree(20, 20, 5, 3, 5, "#.##.#.###.##.#.##.##.#.#");
    add_tree(24, 21, 5, 7, 5, "##.#.##..###.#.##.#.##.##");
    add_tree(4, 21, 5, 5, 4, "#.#.###.#.##.###.#.##.#.#");
    add_tree(33, 9, 4, 5, 6, "###.##.#.#.###.#.##.###.#");   // (rooted at the ground: its canopy from the view's level)
    for (int bx = 0; bx < BXN; bx++) for (int by = 0; by < BYN; by++) {
        std::vector<FakePlant*>& v = plants_of[bx][by];
        columns[bx][by].first = v.data(); columns[bx][by].last = v.data() + v.size(); columns[bx][by].end = columns[bx][by].last;
        col_by_y[bx][by] = &columns[bx][by];
    }
    for (int bx = 0; bx < BXN; bx++) col_index[bx] = col_by_y[bx];
    // ---- the block, as native.lua fills it ----
    const int N = DX * DY;
    vp.flag = 1; vp.dx = DX; vp.dy = DY;
    int32_t* ifa = (int32_t*)calloc(N, 4); int32_t* ifb = (int32_t*)calloc(N, 4);
    int32_t* pja = (int32_t*)calloc(N, 4); int32_t* pjb = (int32_t*)calloc(N, 4);
    vp.IF = ifa; vp.PJ = pja;
    B.off_flag = 0; B.off_dim_x = 4; B.off_dim_y = 8; B.off_interface = 80; B.off_projectile = 88;
    B.off_tiletype = OFF_TT; B.off_designation = OFF_DES;
    B.main_viewport = (uint64_t)(uintptr_t)&main_ptr; B.lower_viewports = (uint64_t)(uintptr_t)&lower[0]; B.lower_count = 0;
    B.block_index = (uint64_t)(uintptr_t)&xs;
    B.x_count_block = (uint64_t)(uintptr_t)&xcb; B.y_count_block = (uint64_t)(uintptr_t)&ycb; B.z_count_block = (uint64_t)(uintptr_t)&zcb;
    B.outside_mask = OUTSIDE; B.subterranean_mask = SUBT; B.hidden_mask = HIDDEN; B.flow_mask = FLOW;
    B.wl_tt[T_EMPTY] = 4; B.wl_tt[T_RAMPTOP] = 4; B.wl_tt[T_CANOPY] = 3; B.wl_tt[T_TWIG] = 3; B.wl_tt[T_TRUNK] = 2;
    B.have_drawn = 1;
    // the fog sheet: patch 8, frames 32, 6 densities (art/fog_sheet.lua), texpos 100000 + index
    const int patch = 8, frames = 32;
    B.wfog_patch = patch; B.wfog_frames = frames;
    for (int i = 0; i < 6 * patch * patch * frames; i++) B.wfog_tex[i] = 100000 + i;
    B.wfog_tex_n = 6 * patch * patch * frames;
    // the leaf sheets: cells 4, frames 36, winds 5, heaviness 3; a quarter of the tiles empty (0)
    const int lc = 4, lf = 36, lw = 5, lh = 3, ln = lh * lw * lc * lc * lf;
    B.leaf_cells = lc; B.leaf_frames = lf; B.leaf_winds = lw; B.leaf_heavies = lh;
    for (int s = 0; s < 2; s++) { for (int i = 0; i < ln; i++) B.leaf_tex[s][i] = (i % 4 == 1) ? 0 : 200000 + s * 20000 + i; B.leaf_tex_n[s] = ln; }
    B.column_index = (uint64_t)(uintptr_t)&col_index_ptr;
    B.off_col_plants = 40; B.off_plant_pos = 0; B.off_plant_material = 8; B.off_plant_tree = 16;
    B.off_tree_body = 0; B.off_tree_height = 8; B.off_tree_dim_x = 12; B.off_tree_dim_y = 16;
    B.leaf_sheds[3] = B.leaf_sheds[5] = 1;
    B.leaf_recheck = 3600;

    auto frame_at = [&](int x0, int y0, int z) { B.drawn_x = x0; B.drawn_y = y0; B.drawn_z = z; };
    // ---- THE FOG: every cell against the Lua, at levels with depths 0 to 8, drifting east and west ----
    int fog_cells = 0;
    const int steps_sets[3][9] = {{0, 37, 74, 111, 148, 185, 222, 259, 296}, {-5, 30, 70, 100, 140, 180, 220, 250, 290},
                                  {1000, 800, 650, 560, 480, 420, 370, 330, 300}};
    struct View { int x0, y0, z, level, set; };
    const View views[] = {{0, 0, 5, 6, 0}, {5, 3, 4, 1, 1}, {-3, 90, 9, 3, 2}, {2, 6, 11, 4, 1}, {200, 100, 6, 2, 0}};
    for (const View& v : views) {
        frame_at(v.x0, v.y0, v.z);
        for (int d = 0; d < 9; d++) B.wfog_steps[d] = steps_sets[v.set][d];
        B.wfog_level = v.level; B.wfog_on = 1;
        memset(vp.IF, 0, N * 4);
        draw_weather_fog();
        int bad = 0, firstx = -1, firsty = -1, want0 = 0, got0 = 0;
        for (int sx = 0; sx < DX; sx++) for (int sy = 0; sy < DY; sy++) {
            const int x = v.x0 + sx, y = v.y0 + sy;
            const int depth = lua_outdoors(x, y, v.z);
            int want = 0;
            if (depth >= 0) { want = 100000 + lua_fog_tile(patch, frames, v.level, steps_sets[v.set][depth], x, y + depth * 3) - 1; fog_cells++; }
            const int got = vp.IF[sx * DY + sy];
            if (got != want) { if (!bad) { firstx = x; firsty = y; want0 = want; got0 = got; } bad++; }
        }
        CHECK(bad == 0, "fog at level %d: %d cells differ (first %d,%d: want %d, got %d)", v.z, bad, firstx, firsty, want0, got0);
        // (put back: next frame the other buffer, then this one again, given the game's own back)
        g_wfog_kept.last = vp.IF;
        kept_put_back(g_wfog_kept, B.off_interface, B.wfog_put_back, B.wfog_dropped);   // (the other buffer drawn: kept)
        vp.IF = vp.IF == ifa ? ifb : ifa;
        kept_put_back(g_wfog_kept, B.off_interface, B.wfog_put_back, B.wfog_dropped);
        vp.IF = vp.IF == ifa ? ifb : ifa;
        kept_put_back(g_wfog_kept, B.off_interface, B.wfog_put_back, B.wfog_dropped);
        int left = 0;
        for (int i = 0; i < N; i++) left += vp.IF[i] != 0;
        CHECK(left == 0 && g_wfog_kept.n == 0, "fog not all put back at level %d: %d left, %d kept", v.z, left, g_wfog_kept.n);
    }
    CHECK(fog_cells > 1000, "too few fog cells looked at: %d", fog_cells);
    // the game's or another overlay's own in the interface: left as it is, and not put back over
    frame_at(0, 0, 5); B.wfog_level = 6;
    memset(vp.IF, 0, N * 4); vp.IF[7] = 42;
    draw_weather_fog();
    CHECK(vp.IF[7] == 42 && B.wfog_taken_last >= 1, "fog over another's interface: %d", vp.IF[7]);
    kept_put_back(g_wfog_kept, B.off_interface, B.wfog_put_back, B.wfog_dropped);
    vp.IF = vp.IF == ifa ? ifb : ifa; kept_put_back(g_wfog_kept, B.off_interface, B.wfog_put_back, B.wfog_dropped);
    vp.IF = vp.IF == ifa ? ifb : ifa; kept_put_back(g_wfog_kept, B.off_interface, B.wfog_put_back, B.wfog_dropped);
    CHECK(vp.IF[7] == 42, "another's interface put back over: %d", vp.IF[7]);
    // no fog: nothing drawn
    memset(vp.IF, 0, N * 4); B.wfog_level = 0; draw_weather_fog();
    { int any = 0; for (int i = 0; i < N; i++) any += vp.IF[i] != 0; CHECK(any == 0, "fog drawn at density 0: %d", any); }
    B.wfog_on = 0;

    // ---- THE LEAVES: every cell against the Lua, both sheets, the heavinesses, winds and frames ----
    int leaf_cells = 0, leaf_drawn = 0;
    struct LView { int x0, y0, z, frame, wind; };
    const LView lviews[] = {{0, 0, 5, 0, 3}, {10, 5, 5, 17, 1}, {14, 12, 6, 35, 5}, {-4, 15, 5, 8, 2}, {20, 2, 4, 3, 4}, {0, 0, 9, 11, 3}};
    B.leaf_spec[3] = 1 | (3 << 8);   // raw 3: sheet 1, many
    B.leaf_spec[5] = 2 | (1 << 8);   // raw 5: sheet 2, a few
    B.leaf_on = 1; B.leaf_any = 1;
    for (const LView& v : lviews) {
        frame_at(v.x0, v.y0, v.z);
        B.leaf_frame = v.frame; B.leaf_wind = v.wind;
        memset(vp.PJ, 0, N * 4);
        plan_leaves(); draw_leaves();
        int bad = 0, firstx = -1, firsty = -1, want0 = 0, got0 = 0;
        for (int sx = 0; sx < DX; sx++) for (int sy = 0; sy < DY; sy++) {
            const int x = v.x0 + sx, y = v.y0 + sy;
            int want = 0, seed = 0;
            const int raw = (x >= 0 && y >= 0 && x < BXN * 16 && y < BYN * 16) ? lua_decide(x, y, v.z, &seed) : -1;
            if (raw >= 0) {
                leaf_cells++;
                const int sheet = raw == 3 ? 0 : 1, heavy = raw == 3 ? 3 : 1;
                const int index = lua_sheet_tile(lc, lf, lw, heavy, v.wind, x, y, v.frame, seed);
                want = B.leaf_tex[sheet][index - 1];
                if (want) leaf_drawn++;
            }
            const int got = vp.PJ[sx * DY + sy];
            if (got != want) { if (!bad) { firstx = x; firsty = y; want0 = want; got0 = got; } bad++; }
        }
        CHECK(bad == 0, "leaves at level %d (%d,%d): %d cells differ (first %d,%d: want %d, got %d)", v.z, v.x0, v.y0, bad, firstx, firsty, want0, got0);
        g_leaf_kept.last = vp.PJ;
        kept_put_back(g_leaf_kept, B.off_projectile, B.leaf_put_back, B.leaf_dropped);
        vp.PJ = vp.PJ == pja ? pjb : pja; kept_put_back(g_leaf_kept, B.off_projectile, B.leaf_put_back, B.leaf_dropped);
        vp.PJ = vp.PJ == pja ? pjb : pja; kept_put_back(g_leaf_kept, B.off_projectile, B.leaf_put_back, B.leaf_dropped);
        int left = 0;
        for (int i = 0; i < N; i++) left += vp.PJ[i] != 0;
        CHECK(left == 0, "leaves not all put back: %d left", left);
    }
    CHECK(leaf_cells > 40 && leaf_drawn > 20, "too few leaf cells looked at: %d (%d drawn)", leaf_cells, leaf_drawn);
    // a tree no longer shedding (its spec 0): nothing on it; nothing at all out of season (leaf_any 0)
    frame_at(0, 0, 5); B.leaf_frame = 0; B.leaf_wind = 3;
    B.leaf_spec[3] = 0; memset(vp.PJ, 0, N * 4); plan_leaves(); draw_leaves();
    { int on3 = 0; for (int sx = 15; sx < 26; sx++) for (int sy = 15; sy < 26; sy++) on3 += vp.PJ[sx * DY + sy] != 0;
      CHECK(on3 == 0, "leaves drawn for a tree that stopped shedding: %d", on3); }
    B.leaf_spec[3] = 1 | (3 << 8);
    for (int k = 0; k < 3; k++) { vp.PJ = vp.PJ == pja ? pjb : pja; kept_put_back(g_leaf_kept, B.off_projectile, B.leaf_put_back, B.leaf_dropped); }
    memset(pja, 0, N * 4); memset(pjb, 0, N * 4);
    B.leaf_any = 0; plan_leaves(); draw_leaves();
    { int any = 0; for (int i = 0; i < N; i++) any += vp.PJ[i] != 0; CHECK(any == 0, "leaves drawn out of season: %d", any); }
    // the game's own projectile left as it is
    B.leaf_any = 1; frame_at(0, 0, 5); memset(vp.PJ, 0, N * 4);
    vp.PJ[20 * DY + 20] = 77; plan_leaves(); draw_leaves();
    CHECK(vp.PJ[20 * DY + 20] == 77, "a leaf over the game's projectile: %d", vp.PJ[20 * DY + 20]);
    // a tree felled: worked out again once its blocks are rechecked (leaf_recheck frames)
    B.leaf_recheck = 2;
    for (int l = 1; l < 5; l++) for (int i = 0; i < 25; i++) {
        if (plants_of[0][0][0]->tree->body[l][i]) tt_at(20 - 2 + i % 5, 20 - 2 + i / 5, 5 + l) = T_EMPTY;
    }
    plants_of[0][0].erase(plants_of[0][0].begin());
    columns[0][0].first = plants_of[0][0].data(); columns[0][0].last = plants_of[0][0].data() + plants_of[0][0].size();
    for (int k = 0; k < 40; k++) { vp.PJ = vp.PJ == pja ? pjb : pja; kept_put_back(g_leaf_kept, B.off_projectile, B.leaf_put_back, B.leaf_dropped); plan_leaves(); draw_leaves(); }
    { int bad = 0; for (int sx = 0; sx < DX; sx++) for (int sy = 0; sy < DY; sy++) {
        int seed = 0; const int raw = lua_decide(sx, sy, 5, &seed);
        int want = 0;
        if (raw >= 0) { const int sheet = raw == 3 ? 0 : 1, heavy = raw == 3 ? 3 : 1; want = B.leaf_tex[sheet][lua_sheet_tile(lc, lf, lw, heavy, 3, sx, sy, 0, seed) - 1]; }
        const int got = vp.PJ[sx * DY + sy];
        if (got != want && got != 77) bad++;
      }
      CHECK(bad == 0, "after a tree was felled and its blocks rechecked: %d cells differ", bad); }
    B.leaf_recheck = 3600;
    printf("fog: %d outdoor cells checked; leaves: %d cells under a shedding tree checked (%d drawn); %lld blocks worked out, %u maps kept\n",
           fog_cells, leaf_cells, leaf_drawn, (long long)B.leaf_blocks_worked, B.leaf_maps_kept);
    if (perf) {   // (a 250 x 100 view, thick fog and leaves falling, timed)
        double fog_us = 0, leaf_us = 0;
        frame_at(0, 0, 5); B.wfog_on = 1; B.wfog_level = 6;
        for (int k = 0; k < 200; k++) {
            for (int d = 0; d < 9; d++) B.wfog_steps[d] = k + d * 37;
            const double t0 = ticks_now(); draw_weather_fog(); fog_us += (ticks_now() - t0) * g_us_per_tick;
            vp.IF = vp.IF == ifa ? ifb : ifa; kept_put_back(g_wfog_kept, B.off_interface, B.wfog_put_back, B.wfog_dropped);
            B.leaf_frame = k % lf;
            const double t1 = ticks_now(); plan_leaves(); draw_leaves(); leaf_us += (ticks_now() - t1) * g_us_per_tick;
            vp.PJ = vp.PJ == pja ? pjb : pja; kept_put_back(g_leaf_kept, B.off_projectile, B.leaf_put_back, B.leaf_dropped);
        }
        printf("PERF fog: %.1f us a frame for %u outdoor cells; leaves: %.1f us a frame (%u cells under a shedding tree)\n",
               fog_us / 200, B.wfog_cells_last, leaf_us / 200, B.leaf_cells_last);
    }
    printf(fails ? "%d FAILED\n" : "fog and leaves: all checks pass\n", fails);
    return fails != 0;
}
