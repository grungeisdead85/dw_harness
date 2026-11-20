// sim.cpp: runs detailed_weather_dll.cpp's plan and write against a fake DF on Linux.
// A fake map (blocks, tiletypes, designations, snow and grass events), fake double-buffered
// viewports, a fake "DF draw" with three redraw models, and a fake Lua bake server.
//   mode 0: DF redraws every layer of every cell every frame (the reference)
//   mode 1: DF writes only the layers whose value changed since it last drew that buffer
//   mode 2: as mode 1, plus random whole-cell redraws and random partial-layer writes
// The screen (each frame's current buffers after the hook) must be the same in every mode.
#include <ctime>
#include <cstdio>
#include <vector>
#include <map>
#include <string>
#include <random>
#include "windows.h"
SIZE_T VirtualQuery(LPCVOID, MEMORY_BASIC_INFORMATION*, SIZE_T) { return 0; }
BOOL VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD) { return 1; }
HMODULE GetModuleHandleW(LPCWSTR) { return nullptr; }
BOOL QueryPerformanceCounter(LARGE_INTEGER* t) {
    timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); t->QuadPart = ts.tv_sec * 1000000000LL + ts.tv_nsec; return 1;
}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* f) { f->QuadPart = 1000000000LL; return 1; }
PVOID InterlockedExchangePointer(PVOID volatile* p, PVOID v) { PVOID o = *p; *p = v; return o; }
DWORD GetLastError() { return 0; }
DWORD GetFileAttributesA(LPCSTR) { return INVALID_FILE_ATTRIBUTES; }
extern "C" int _putenv_s(const char*, const char*) { return 0; }

#include "../detailed_weather_dll.cpp"

// ---------------- the fake world ----------------
static int BXN, BYN, ZN, XE, YE;
enum { T_NONE = 0, T_GRASS = 1, T_DIRT = 2, T_RAMP = 3, T_OPEN = 4, T_TRUNK = 5, T_WALL = 6, T_BRANCH = 7 };
struct Tile { uint8_t type = T_WALL; uint8_t snow = 0; uint8_t plant = 0; bool hidden = false, fruit = false, shrub = false, outside = true; uint8_t walls = 0; };
static std::vector<Tile> world;   // [z][y][x]
static Tile& at(int x, int y, int z) { return world[((size_t)z * YE + y) * XE + x]; }
static bool inmap(int x, int y, int z) { return x >= 0 && y >= 0 && z >= 0 && x < XE && y < YE && z < ZN; }
static const int PLANT_EDGING[3] = {0, 1, 12};   // plant index -> edging id (0 none)

// blocks, laid out as the DLL reads them
static const uint32_t OFF_TT = 64, OFF_DES = 576, OFF_EV = 1600;
struct SpatterEv { uint64_t vt; int16_t mat_type; int16_t mat_state; int32_t pad; uint8_t amount[256]; };
struct GrassEv { uint64_t vt; int32_t plant; int32_t pad; uint8_t amount[256]; };
static std::vector<uint8_t*> blocks;
static std::vector<std::vector<void*>> block_events;
static void*** xs_arr;
static int32_t g_xcb, g_ycb, g_zcb;
static const uint64_t VT_SPATTER = 0xAAAA0000ull, VT_GRASS = 0xBBBB0000ull;
static std::vector<SpatterEv*> spatter_evs;   // one per block
static std::vector<GrassEv*> grass_evs[3];

static void build_blocks() {
    xs_arr = (void***)calloc(BXN, sizeof(void*));
    for (int bx = 0; bx < BXN; bx++) {
        void** ys = (void**)calloc(BYN, sizeof(void*));
        xs_arr[bx] = (void**)ys;
        for (int by = 0; by < BYN; by++) {
            void** zs = (void**)calloc(ZN, sizeof(void*));
            ys[by] = zs;
            for (int z = 0; z < ZN; z++) {
                uint8_t* b = (uint8_t*)calloc(1, 2048);
                zs[z] = b;
                blocks.push_back(b);
                SpatterEv* se = (SpatterEv*)calloc(1, sizeof(SpatterEv));
                se->vt = VT_SPATTER; se->mat_type = 6; se->mat_state = 2;
                spatter_evs.push_back(se);
                std::vector<void*>* evs = new std::vector<void*>();
                evs->push_back(se);
                // a decoy: blood (mat_type 0) with amounts, which must not count as snow
                SpatterEv* blood = (SpatterEv*)calloc(1, sizeof(SpatterEv));
                blood->vt = VT_SPATTER; blood->mat_type = 0; blood->mat_state = 1;
                for (int i = 0; i < 256; i++) blood->amount[i] = 250;
                evs->push_back(blood);
                for (int p = 1; p <= 2; p++) {
                    GrassEv* ge = (GrassEv*)calloc(1, sizeof(GrassEv));
                    ge->vt = VT_GRASS; ge->plant = p;
                    evs->push_back(ge);
                    grass_evs[p].push_back(ge);
                }
                if (getenv("HEAVY")) for (int p = 3; p <= 6; p++) {   // (as DF's blocks: more grass events)
                    GrassEv* ge = (GrassEv*)calloc(1, sizeof(GrassEv));
                    ge->vt = VT_GRASS; ge->plant = p;
                    for (int i = 0; i < 256; i++) ge->amount[i] = (uint8_t)(i * p);
                    evs->push_back(ge);
                }
                void** vec = (void**)(b + OFF_EV);
                vec[0] = evs->data();
                vec[1] = evs->data() + evs->size();
            }
        }
    }
}
static int block_no(int bx, int by, int z) { return (bx * BYN + by) * ZN + z; }
static const uint16_t TT_OF[8] = {0, 11, 12, 13, 14, 15, 16, 17};   // fake tiletype numbers
// map tiles -> block bytes and events
static void sync_blocks() {
    for (int z = 0; z < ZN; z++) for (int y = 0; y < YE; y++) for (int x = 0; x < XE; x++) {
        const Tile& t = at(x, y, z);
        int n = block_no(x >> 4, y >> 4, z);
        uint8_t* b = blocks[n];
        int ti = ((x & 15) << 4) | (y & 15);
        ((uint16_t*)(b + OFF_TT))[ti] = TT_OF[t.type];
        uint32_t des = (t.hidden ? 512u : 0u) | (t.outside ? (1u << 16) : 0u);
        ((uint32_t*)(b + OFF_DES))[ti] = des;
        spatter_evs[n]->amount[ti] = t.snow;
        for (int p = 1; p <= 2; p++) grass_evs[p][n]->amount[ti] = (t.plant == p) ? 50 : (t.plant ? 10 : 0);
    }
}

// ---------------- the fake viewports ----------------
struct Bufs { std::vector<int32_t> bg, bg2, spatter; std::vector<uint64_t> flag; std::vector<uint32_t> shadow; std::vector<uint64_t> ramp; };
struct FakeVP {   // the DLL reads: flag 0, dim_x 4, dim_y 8, pointers from 16
    uint32_t flag; int32_t dim_x, dim_y, pad;
    int32_t* bg; int32_t* bg2; int32_t* spatter; uint64_t* floor_flag; uint32_t* shadow; uint64_t* ramp_flag;
};
static const int NSLOT = 4;
static FakeVP vps[NSLOT];
static Bufs* bufs_store[NSLOT][2];
#define bufs_at(s, k) (*bufs_store[s][k])
static int cur_set[NSLOT];
static FakeVP* main_vp_ptr;
static FakeVP* lower_ptrs[10];
static int32_t win_x, win_y, win_z;
static const int32_t BLACK = 9;
static void point(int s) {
    Bufs& b = bufs_at(s, cur_set[s]);
    vps[s].bg = b.bg.data(); vps[s].bg2 = b.bg2.data(); vps[s].spatter = b.spatter.data();
    vps[s].floor_flag = b.flag.data(); vps[s].shadow = b.shadow.data(); vps[s].ramp_flag = b.ramp.data();
}
static void make_viewports(int dx, int dy) {
    for (int s = 0; s < NSLOT; s++) {
        vps[s].flag = 2 | 1; vps[s].dim_x = dx; vps[s].dim_y = dy;
        for (int k = 0; k < 2; k++) {
            bufs_store[s][k] = new Bufs();   // (the old ones kept, as freed memory may be reused)
            Bufs& b = bufs_at(s, k);
            size_t n = (size_t)dx * dy;
            b.bg.assign(n, BLACK); b.bg2.assign(n, 0); b.spatter.assign(n, 0); b.flag.assign(n, 0); b.shadow.assign(n, 0); b.ramp.assign(n, 0);
        }
        cur_set[s] = 0;
        point(s);
    }
    main_vp_ptr = &vps[0];
    for (int s = 1; s < NSLOT; s++) lower_ptrs[s - 1] = &vps[s];
}
static void swap_arrays() { for (int s = 0; s < NSLOT; s++) { cur_set[s] ^= 1; point(s); } }

// ---------------- the fake DF drawing ----------------
static int edging_of(int x, int y, int z) {
    if (!inmap(x, y, z)) return 0;
    const Tile& t = at(x, y, z);
    return (t.type == T_GRASS) ? PLANT_EDGING[t.plant] : 0;
}
struct Cell { int32_t bg = 0, bg2 = 0, spatter = 0; uint64_t flag = 0; uint32_t shadow = 0; uint64_t ramp = 0; };
static const int SDX[4] = {0, -1, 1, 0}, SDY[4] = {1, 0, 0, -1};   // S, W, E, N
static Cell df_cell_of(int x, int y, int z) {
    Cell c;
    if (!inmap(x, y, z)) return c;
    const Tile& t = at(x, y, z);
    if (t.hidden) { c.bg = BLACK; return c; }
    int own = edging_of(x, y, z);
    auto overhangs = [&]() {
        uint64_t f = 0;
        for (int b = 0; b < 4; b++) {
            int e = edging_of(x + SDX[b], y + SDY[b], z);
            if (e && beats(e, own)) f |= (uint64_t)e << (8 * b);
        }
        int e = edging_of(x - 1, y - 1, z);
        if (e && beats(e, own)) f |= (uint64_t)e << 32;   // something in the high bytes
        return f;
    };
    switch (t.type) {
    case T_OPEN: c.flag = overhangs(); break;
    case T_WALL: c.bg = 1200; break;
    case T_RAMP: c.bg = 1100; c.ramp = (uint64_t)t.walls << 8; break;
    case T_TRUNK: c.bg = 1300; c.flag = overhangs(); break;
    case T_GRASS: case T_DIRT: case T_BRANCH: {
        c.bg = t.type == T_GRASS ? 1001 : t.type == T_DIRT ? 1002 : 1003;
        c.flag = overhangs();
        if (t.fruit) c.spatter = 7000;
        else if (t.snow >= 100) {
            if (t.type == T_BRANCH && ((x + y) & 1)) c.spatter = 0;
            else {
                int mask = 0;
                for (int b = 0; b < 4; b++) {
                    int nx = x + SDX[b], ny = y + SDY[b];
                    if (inmap(nx, ny, z) && at(nx, ny, z).snow >= 100) mask |= 1 << b;
                }
                c.spatter = B.pieces_tex[mask][0];
            }
        } else if (t.snow > 0) c.spatter = 120 + t.snow % 12;
        if (inmap(x, y - 1, z) && at(x, y - 1, z).type == T_WALL) c.shadow = 0x10;
        if (t.shrub) c.bg2 = 8000;
        break;
    }
    }
    return c;
}
// DF's own record of what it last drew in each buffer (modes 1 and 2)
static std::vector<Cell> last_drawn[NSLOT][2];
static std::mt19937 rng(12345);
static void df_draw(int mode, bool view_moved) {
    for (int s = 0; s < NSLOT; s++) {
        FakeVP& vp = vps[s];
        int k = cur_set[s];
        std::vector<Cell>& last = last_drawn[s][k];
        size_t n = (size_t)vp.dim_x * vp.dim_y;
        if (last.size() != n) { last.assign(n, Cell()); view_moved = true; }
        for (int sx = 0; sx < vp.dim_x; sx++) for (int sy = 0; sy < vp.dim_y; sy++) {
            int i = sx * vp.dim_y + sy;
            Cell c = df_cell_of(win_x + sx, win_y + sy, win_z - s);
            Cell& l = last[i];
            if (mode == 3) {   // DF compares with what the buffer holds
                if (vp.bg[i] != c.bg) vp.bg[i] = c.bg;
                if (vp.bg2[i] != c.bg2) vp.bg2[i] = c.bg2;
                if (vp.spatter[i] != c.spatter) vp.spatter[i] = c.spatter;
                if (vp.floor_flag[i] != c.flag) vp.floor_flag[i] = c.flag;
                if (vp.shadow[i] != c.shadow) vp.shadow[i] = c.shadow;
                if (vp.ramp_flag[i] != c.ramp) vp.ramp_flag[i] = c.ramp;
                l = c;
                continue;
            }
            bool all = mode == 0 || view_moved || (mode == 2 && rng() % 10 == 0);
            bool partial = mode == 2 && rng() % 7 == 0;
            if (all || c.bg != l.bg || (partial && rng() % 2)) vp.bg[i] = c.bg;
            if (all || c.bg2 != l.bg2 || (partial && rng() % 2)) vp.bg2[i] = c.bg2;
            if (all || c.spatter != l.spatter || (partial && rng() % 2)) vp.spatter[i] = c.spatter;
            if (all || c.flag != l.flag || (partial && rng() % 2)) vp.floor_flag[i] = c.flag;
            if (all || c.shadow != l.shadow || (partial && rng() % 2)) vp.shadow[i] = c.shadow;
            if (all || c.ramp != l.ramp) vp.ramp_flag[i] = c.ramp;
            l = c;
        }
    }
}

// ---------------- the fake Lua bake server (snow_ramps.lua: serve_native_bakes) ----------------
static std::map<std::string, int32_t> lua_cache;
static int32_t next_bake = 60000;
static int lua_budget_per_frame = 6;
static void lua_serve() {
    int budget = lua_budget_per_frame;
    while (B.req_tail != B.req_head && budget > 0) {
        if (B.res_head - B.res_tail >= (uint32_t)RING) break;
        const BakeRequest& r = B.req[B.req_tail % RING];
        char key[256];
        if (r.kind == 1) snprintf(key, sizeof key, "f%d:%d", r.a, r.c);
        else if (r.kind == 2) snprintf(key, sizeof key, "w%d:%d", r.a, r.b);
        else { std::string k = "e"; for (uint32_t j = 0; j < r.n; j++) k += std::to_string(r.pieces[j]) + ":"; snprintf(key, sizeof key, "%s", k.c_str()); }
        int32_t tex;
        auto it = lua_cache.find(key);
        if (it != lua_cache.end()) tex = it->second;
        else {
            uint64_t h = 1469598103934665603ull;
            for (const char* c = key; *c; c++) { h ^= (uint8_t)*c; h *= 1099511628211ull; }
            tex = 200000 + (int32_t)(h % 800000); lua_cache[key] = tex; budget--; next_bake++;
        }
        BakeResult& o = B.res[B.res_head % RING];
        o.key = r.key; o.texpos = tex;
        B.res_head++;
        B.req_tail++;
    }
}

// ---------------- tables (snow_ramps.lua: native_tables, as detailed_weather_hook_test fills them) ----------------
static const int FULL_SIDES[20] = {15, 15, 15, 15, 15, 0, 9, 6, 8, 1, 2, 4, 10, 12, 3, 5, 11, 13, 14, 7};
static void noop_tidy(void*) {}
static void fill_tables() {
    static const uint8_t TTF[8] = {0, TT_FLOOR | TT_GRASS, TT_FLOOR, TT_RAMP, TT_OPEN, TT_TRUNK | TT_TREE, 0, TT_FLOOR | TT_TREE};
    for (int k = 0; k < 8; k++) B.tt_flags[TT_OF[k]] = TTF[k];
    B.plant_edging[1] = 1; B.plant_edging[2] = 12;
    for (int id : {1, 2, 12}) for (int p = 0; p < 8; p++) { B.edge_art[id][p] = 40000 + id * 8 + p; B.snowy_single[id][p] = 50000 + id * 8 + p; }
    for (int b = 0; b < 256; b++) B.snow_ramp_tex[b] = 5000 + b;
    for (int k = 0; k < 20; k++) {
        B.full_tex[k] = 100 + k; B.full_sides[k] = (uint8_t)FULL_SIDES[k]; B.full_col[k] = (uint8_t)k; B.full_inside[k] = k < 5;
    }
    for (int m = 0; m < 16; m++) {
        int n = 0;
        for (int k = 0; k < 20; k++) if (FULL_SIDES[k] == m) B.pieces_tex[m][n++] = 100 + k;
        B.pieces_n[m] = (uint8_t)n;
    }
    B.page_n = 32; for (int k = 0; k < 32; k++) B.page_tex[k] = 100 + k;
    B.made_n = 0;
    for (int k = 0; k < THIN_N; k++) B.thin_tex[k] = 20000 + k;
    for (int k = 0; k < INSIDE_N; k++) B.inside_tex[k] = 100000 + k;
    B.edge_level = 1; B.snowy_from = 100; B.snow_mat_type = 6; B.snow_mat_state = 2;
    B.hidden_mask = 512; B.outside_mask = 1u << 16; B.black_texpos = BLACK;
    B.off_flag = 0; B.off_dim_x = 4; B.off_dim_y = 8; B.off_background = 16; B.off_bg2 = 24; B.off_spatter = 32;
    B.off_floor_flag = 40; B.off_shadow_flag = 48; B.off_ramp_flag = 56;
    B.off_tiletype = OFF_TT; B.off_designation = OFF_DES; B.off_block_events = OFF_EV;
    B.off_ev_mat_type = 8; B.off_ev_mat_state = 10; B.off_ev_amount = 16; B.off_gev_plant = 8; B.off_gev_amount = 16;
    B.spatter_vtable = VT_SPATTER; B.grass_vtable = VT_GRASS;
    B.main_viewport = (uint64_t)(uintptr_t)&main_vp_ptr; B.lower_viewports = (uint64_t)(uintptr_t)&lower_ptrs[0]; B.lower_count = 10;
    B.window_x = (uint64_t)(uintptr_t)&win_x; B.window_y = (uint64_t)(uintptr_t)&win_y; B.window_z = (uint64_t)(uintptr_t)&win_z;
    g_xcb = BXN; g_ycb = BYN; g_zcb = ZN;
    B.block_index = (uint64_t)(uintptr_t)&xs_arr;
    B.x_count_block = (uint64_t)(uintptr_t)&g_xcb; B.y_count_block = (uint64_t)(uintptr_t)&g_ycb; B.z_count_block = (uint64_t)(uintptr_t)&g_zcb;
    B.tables_ready = 1;
    B.hooked = 1; B.install = 0; B.enabled = 1;
    g_tidy_original = noop_tidy;
    g_us_per_tick = 1e-3;   // (dfhooks_init isn't called here; ticks are ns)
}

// ---------------- the scenario ----------------
static void make_world_small() {
    BXN = 4; BYN = 3; ZN = 4; XE = BXN * 16; YE = BYN * 16;
    world.assign((size_t)XE * YE * ZN, Tile());
    std::mt19937 r(7);
    // z 0: rock; z 1: rock; z 2: ground (grass, snow); z 3: a raised plateau on the west, open air elsewhere
    for (int y = 0; y < YE; y++) for (int x = 0; x < XE; x++) {
        Tile& g = at(x, y, 2);
        g.type = T_GRASS; g.plant = (x / 9 + y / 7) % 2 + 1; g.snow = (uint8_t)(100 + r() % 156);
        if ((x * 31 + y * 17) % 23 == 0) g.snow = 255;
        Tile& u = at(x, y, 3);
        if (x < 20) { u.type = T_GRASS; u.plant = 1; u.snow = (uint8_t)(120 + r() % 136); }
        else u.type = T_OPEN;
    }
    // bare patches, dirt, a trunk, ramps, fruit, shrubs, branches, a hidden tile, an indoor tile
    for (int y = 20; y < 26; y++) for (int x = 30; x < 37; x++) at(x, y, 2).snow = 0;
    for (int y = 22; y < 24; y++) for (int x = 32; x < 35; x++) at(x, y, 2).type = T_DIRT;
    at(40, 10, 2).type = T_TRUNK; at(40, 10, 2).snow = 0;
    at(25, 30, 2).type = T_RAMP; at(25, 30, 2).walls = 9; at(26, 30, 2).type = T_RAMP; at(26, 30, 2).walls = 2;
    at(27, 30, 2).type = T_RAMP; at(27, 30, 2).walls = 0; at(27, 30, 2).snow = 50;
    at(45, 15, 2).fruit = true; at(46, 15, 2).fruit = true; at(46, 15, 2).shrub = true;
    at(10, 12, 3).fruit = true;
    for (int x = 50; x < 54; x++) { at(x, 8, 2).type = T_BRANCH; }
    at(52, 9, 2).type = T_BRANCH; at(52, 9, 2).snow = 0;
    at(35, 5, 2).hidden = true;
    at(38, 38, 2).outside = false; at(38, 38, 2).snow = 0;
    for (int x = 28; x < 34; x++) at(x, 40, 2).type = T_WALL;
    at(19, 16, 3).snow = 0;   // a bare tile at the plateau's edge, beside the open air
}

static uint64_t screen_hash() {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
    for (int s = 0; s < NSLOT; s++) {
        size_t n = (size_t)vps[s].dim_x * vps[s].dim_y;
        for (size_t i = 0; i < n; i++) {
            mix((uint32_t)vps[s].bg[i]); mix((uint32_t)vps[s].bg2[i]); mix((uint32_t)vps[s].spatter[i]);
            mix(vps[s].floor_flag[i]); mix(vps[s].shadow[i]);
        }
    }
    return h;
}
static void dump_screen(const char* path) {
    FILE* f = fopen(path, "w");
    for (int s = 0; s < NSLOT; s++) {
        size_t n = (size_t)vps[s].dim_x * vps[s].dim_y;
        for (size_t i = 0; i < n; i++)
            fprintf(f, "%d %zu %d %d %d %llx %x\n", s, i, vps[s].bg[i], vps[s].bg2[i], vps[s].spatter[i],
                    (unsigned long long)vps[s].floor_flag[i], vps[s].shadow[i]);
    }
    fclose(f);
}
// every cell of the screen equal to DF's own drawing (after the snow is turned off)
static int cells_not_df() {
    int bad = 0;
    for (int s = 0; s < NSLOT; s++)
        for (int sx = 0; sx < vps[s].dim_x; sx++) for (int sy = 0; sy < vps[s].dim_y; sy++) {
            int i = sx * vps[s].dim_y + sy;
            Cell c = df_cell_of(win_x + sx, win_y + sy, win_z - s);
            if (vps[s].bg[i] != c.bg || vps[s].bg2[i] != c.bg2 || vps[s].spatter[i] != c.spatter ||
                vps[s].floor_flag[i] != c.flag || vps[s].shadow[i] != c.shadow) bad++;
        }
    return bad;
}

static int run_perf(bool dump, bool nosnow);
static FILE* g_dump = nullptr;
static void dump_records(int frame) {
    if (!g_dump) return;
    for (uint32_t n = 0; n < g_rec_n; n++) {
        const Rec& r = g_recs[n];
        fprintf(g_dump, "%d %d %d %d %d %d j%d s%d f%d k%d fl%d a%d l%d c%d v%d,%d b%d e%d d%d,%d,%d,%d\n", frame, r.x, r.y, r.index,
                r.slot, r.kind, r.join, r.snowy, r.full, r.keep, r.flags, r.amount, r.level, r.combo, r.vthin, r.vinside,
                r.block, r.own_edge, r.diag[0], r.diag[1], r.diag[2], r.diag[3]);
    }
}
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "perf") return run_perf(argc > 2 && std::string(argv[2]) == "dump", argc > 2 && std::string(argv[2]) == "nosnow");
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    bool leftover = argc > 2 && std::string(argv[2]) == "leftover";
    if (argc > 2 && std::string(argv[2]) == "dump") g_dump = fopen("recs_small.txt", "w");
    make_world_small();
    build_blocks();
    sync_blocks();
    make_viewports(40, 30);
    fill_tables();
    win_x = 5; win_y = 6; win_z = 3;
    g_log = stdout;
    for (int frame = 0; frame < 60; frame++) {
        bool moved = false;
        // events in the world
        if (frame == 15) { at(12, 20, 2).snow = 0; at(41, 20, 2).snow = 0; sync_blocks(); }    // melts
        if (frame == 18) { at(44, 24, 2).fruit = true; sync_blocks(); }                       // fruit falls
        if (frame == 21) { at(30, 21, 2).snow = 180; sync_blocks(); }                         // snow on the bare patch
        if (frame == 25) { win_x = 6; moved = true; }                                         // a pan
        if (frame == 30) { win_z = 2; moved = true; }                                         // down a level
        if (frame == 33) { win_z = 3; moved = true; }                                         // back up
        if (frame == 45) B.enabled = 0;                                                       // snow turned off
        // (leftover: the snow drawn by "the Lua" for 8 frames, then handed over with nothing kept)
        int draw_mode = (leftover && frame < 8) ? 0 : mode;
        if (leftover && frame == 8) for (int s2 = 0; s2 < SLOTS; s2++) for (int k = 0; k < 2; k++) g_banks[s2][k].n = 0;
        swap_arrays();
        dfhooks_prerender();
        dump_records(frame);
        df_draw(draw_mode, moved);
        lua_serve();
        if (frame == 38) { make_viewports(46, 34); for (int s2 = 0; s2 < NSLOT; s2++) for (int k = 0; k < 2; k++) last_drawn[s2][k].clear(); }
        else tidy_hook(nullptr);   // (on a zoom DF skips the renderer's calls that frame)
        printf("frame %2d hash %016llx records %lld waiting %lld restored %lld bakes %lld/%lld\n", frame,
               (unsigned long long)screen_hash(), (long long)B.records_last, (long long)B.cells_waiting,
               (long long)B.cells_restored, (long long)B.bakes_done, (long long)B.bakes_asked);
        if (frame == 44 || frame == 32 || frame == 35) { char p[64]; snprintf(p, sizeof p, "screen_mode%d_f%d.txt", mode, frame); dump_screen(p); }
    }
    printf("after off: cells not the game's own drawing: %d\n", cells_not_df());
    printf("waiting cells seen in all: %lld\n", (long long)B.cells_waiting);
    if (g_dump) fclose(g_dump);
    printf("counts: ramps %lld floors %lld trunks %lld edges %lld; overflow %u; memory %u KB\n",
           (long long)B.ramps_written, (long long)B.floors_written, (long long)B.trunks_written,
           (long long)B.edges_written, B.plan_overflow, memory_kb());
    return 0;
}

// ---------------- timing: a 160 x 90 view of snowy grass over 9 levels ----------------
static int run_perf(bool dump, bool nosnow) {
    // (UP: levels the view sits above the ground, 2 by default; W, H: the view's size, 160 x 90)
    const int up = getenv("UP") ? atoi(getenv("UP")) : 2;
    const int vw = getenv("W") ? atoi(getenv("W")) : 160, vh = getenv("H") ? atoi(getenv("H")) : 90;
    BXN = 14; BYN = 9; ZN = 10 + up; XE = BXN * 16; YE = BYN * 16;
    world.assign((size_t)XE * YE * ZN, Tile());
    std::mt19937 r(11);
    for (int y = 0; y < YE; y++) for (int x = 0; x < XE; x++) {
        Tile& g = at(x, y, 9);
        g.type = T_GRASS; g.plant = (x / 9 + y / 7) % 2 + 1; g.snow = (uint8_t)(100 + r() % 156);
        if ((x * 31 + y * 17) % 23 == 0) g.snow = 255;
        if ((x / 13 + y / 11) % 5 == 0) g.snow = (uint8_t)(r() % 90);
        if (x % 17 == 3 && y % 13 == 5) g.type = T_TRUNK;
        if (nosnow) g.snow = 0;
        for (int z = 10; z < ZN; z++) at(x, y, z).type = T_OPEN;
    }
    build_blocks();
    sync_blocks();
    // NSLOT viewports are made; the view sits 2 levels above the ground (the lower ones show it)
    make_viewports(vw, vh);
    fill_tables();
    win_x = 20; win_y = 20; win_z = 9 + up;
    double plan_total = 0, write_total = 0, plan_worst = 0, write_worst = 0;
    int frames = dump ? 30 : 200;
    if (dump) g_dump = fopen("recs_perf.txt", "w");
    for (int frame = 0; frame < frames; frame++) {
        if (frame % 50 == 25) { win_x += 1; }
        swap_arrays();
        double p0 = B.plan_us_total, w0 = B.apply_us_total;
        dfhooks_prerender();
        dump_records(frame);
        df_draw(1, frame % 50 == 25);
        lua_serve();
        tidy_hook(nullptr);
        if (frame >= 20) {
            double p = B.plan_us_total - p0, w = B.apply_us_total - w0;
            plan_total += p; write_total += w;
            if (p > plan_worst) plan_worst = p;
            if (w > write_worst) write_worst = w;
        }
    }
    if (g_dump) { fclose(g_dump); g_dump = nullptr; }
    int n = frames - 20;
    printf("perf: kept %u, redone %u in the last plan\n", B.blocks_kept_last, B.blocks_redone_last);
    printf("perf: %lld columns, %lld records (%lld snowy floors), %u blocks read a frame\n",
           (long long)B.columns_last, (long long)B.records_last, (long long)B.snowy_last, B.blocks_last);
    printf("perf: plan %.1f us a frame (worst %.1f), write %.1f us (worst %.1f), put back %.0f cells a frame; memory %u KB\n",
           plan_total / n, plan_worst, write_total / n, write_worst, (double)B.cells_restored / B.applies, memory_kb());
    return 0;
}
