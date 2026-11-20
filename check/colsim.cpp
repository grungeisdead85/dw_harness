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

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// map: 2 x 2 blocks, 3 levels. z 2 is the surface: tiles with x >= 16 are underground (subterranean).
// a tree tile at (3, 3, 2); water underground at (20, 4, 2).
static const uint32_t OFF_TT = 64, OFF_DES = 576;
static void*** xs; static int32_t xcb = 2, ycb = 2, zcb = 3;
static uint8_t* blk(int x, int y, int z) { return (uint8_t*)((void**)((void**)xs[x >> 4])[y >> 4])[z]; }
static const uint16_t TT_PLAIN = 1, TT_TREE_TYPE = 2;
static const uint32_t SUBT = 1u << 15;

// viewports: dims 8 x 6, seven layers at offsets 16.. (as the fake layout)
struct VP { uint32_t flag; int32_t dx, dy; int32_t pad; int32_t* L[7]; uint64_t* FF; int32_t* IF; };   // (FF at 72: floor_flag; IF at 80: interface)
static VP vps[3]; static VP* main_ptr = &vps[0]; static VP* lower[2] = {&vps[1], &vps[2]};
static int32_t wx = 0, wy = 0, wz = 2;
#ifndef PERF
static const int DX = 8, DY = 6, N = DX * DY;
#else
static const int DX = 250, DY = 100, N = DX * DY;   // (PERF: a big view, timing only)
#endif

// fake colour.lua: originals 1000.., copies by look: look A 20000 + t, look B 30000 + t. table copies 9000.. -> 1000..
static int look = 0;   // 0: A, 1: B
static int32_t copy_of(int32_t t) { return (look ? 30000 : 20000) + t; }
static void publish(int32_t t) {   // an original's copy in the look shown, and the copy's own entry
    int32_t c = copy_of(t);
    g_link[t] = c; g_cflag[t] |= CF_COPY_FOR;
    g_link[c] = t; g_cflag[c] = CF_SCREEN;
}
static int answered = 0;
static void lua_answers() {   // what colour.lua does with the questions, a frame later
    while (B.col_req_tail != B.col_req_head) {
        ColQuestion q = B.col_req[B.col_req_tail % COL_RING];
        B.col_req_tail++;
        int32_t t = q.texpos;
        answered++;
        if (t >= 9000 && t < 9100) { g_link[t] = t - 8000; g_cflag[t] |= CF_TABLE; continue; }
        if (t == 777) { g_cflag[t] |= CF_LEAVE; continue; }
        if (q.where & (CW_UNDER | CW_PLAIN)) { if (t == 1007) { g_cflag[t] |= CF_OTHER; publish(t); } continue; }
        publish(t);
    }
}
static void frame() {   // the hook's part of a frame
    read_view_as_drawn();
    if (B.col_reset != B.col_reset_done) {
        memset(g_link, 0, sizeof g_link); memset(g_cflag, 0, sizeof g_cflag); free_held();
        B.col_req_head = B.col_req_tail = B.col_seen_head = B.col_seen_tail = 0; B.col_reset_done = B.col_reset;
    }
    swap_colour();
}
static int32_t& cell(int d, int l, int sx, int sy) { return vps[d].L[l][sx * DY + sy]; }

int main() {
    xs = (void***)calloc(2, sizeof(void*));
    for (int bx = 0; bx < 2; bx++) {
        void** ys = (void**)calloc(2, sizeof(void*)); xs[bx] = (void**)ys;
        for (int by = 0; by < 2; by++) {
            void** zs = (void**)calloc(3, sizeof(void*)); ys[by] = zs;
            for (int z = 0; z < 3; z++) {
                uint8_t* b = (uint8_t*)calloc(1, 4096); zs[z] = b;   // (4096: room for the temperatures at 2048, CRACKS IN PLACE)
                for (int i = 0; i < 256; i++) {
                    ((uint16_t*)(b + OFF_TT))[i] = TT_PLAIN;
                    if (bx == 1 || z < 2) ((uint32_t*)(b + OFF_DES))[i] = SUBT;
                }
            }
        }
    }
    ((uint16_t*)(blk(3, 3, 2) + OFF_TT))[tile_of(3, 3)] = TT_TREE_TYPE;
    ((uint32_t*)(blk(20, 4, 2) + OFF_DES))[tile_of(20, 4)] |= 1;   // water, underground
    B.tt_flags[TT_TREE_TYPE] = TT_TREE;
    for (int d = 0; d < 3; d++) {
        vps[d].flag = 1; vps[d].dx = DX; vps[d].dy = DY;
        for (int l = 0; l < 7; l++) vps[d].L[l] = (int32_t*)calloc(N, 4);
    }
    B.off_flag = 0; B.off_dim_x = 4; B.off_dim_y = 8;
    B.off_background = 16; B.off_bg2 = 24; B.off_spatter = 32; B.off_building_one = 40;
    B.off_building_two = 48; B.off_item = 56; B.off_vehicle = 64;
    B.off_tiletype = OFF_TT; B.off_designation = OFF_DES;
    B.main_viewport = (uint64_t)(uintptr_t)&main_ptr; B.lower_viewports = (uint64_t)(uintptr_t)&lower[0]; B.lower_count = 2;
    B.window_x = (uint64_t)(uintptr_t)&wx; B.window_y = (uint64_t)(uintptr_t)&wy; B.window_z = (uint64_t)(uintptr_t)&wz;
    B.block_index = (uint64_t)(uintptr_t)&xs;
    B.x_count_block = (uint64_t)(uintptr_t)&xcb; B.y_count_block = (uint64_t)(uintptr_t)&ycb; B.z_count_block = (uint64_t)(uintptr_t)&zcb;
    B.black_texpos = 3; B.subterranean_mask = SUBT; B.flow_mask = 7;
    B.col_depths = 3; B.col_reset = 1; B.col_enabled = 1; B.hooked = 1;
    g_us_per_tick = 1e-3;

#ifdef PERF
    // a full view: the ground layer all table copies, a third of the other layers filled with originals
    // (40 kinds), on all three levels; answered, then timed over 200 frames
    for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) for (int i = 0; i < N; i++)
        vps[d].L[l][i] = l == 0 ? 9000 + i % 50 : (i % FILL == 0 ? 1000 + (i * 7 + l) % 40 : 0);
    xcb = 16; ycb = 8;
    { void*** big = (void***)calloc(16, sizeof(void*));
      for (int bx = 0; bx < 16; bx++) { void** ys = (void**)calloc(8, sizeof(void*)); big[bx] = (void**)ys;
        for (int by = 0; by < 8; by++) ys[by] = ((void**)xs[bx & 1])[by & 1]; }
      xs = big; }
    frame(); lua_answers(); frame(); lua_answers();
    B.col_frames = 0; B.col_us_total = 0; B.col_us_max = 0;
    for (int k = 0; k < PERF_FRAMES; k++) {
        // DF redraws: copies go back to originals in every other cell
        for (int d = 0; d < 3; d++) for (int l = 1; l < 7; l++) for (int i = 0; i < N; i += 6)
            if (vps[d].L[l][i] > 20000) vps[d].L[l][i] -= 20000;
        frame();
    }
    printf("PERF %d cells x 7 layers x 3 levels: %.1f us a frame (worst %.1f), %u with something, %u swapped the last\n",
           N, B.col_us_total / B.col_frames, B.col_us_max, B.col_cells_last, B.col_swapped_last);
    // CRACKS IN PLACE (15U): every tile on the view's level may crack (every block's tiles worked out from the
    // map on the first frame, timed apart), all its copies made; timed after
    {
        B.crack_tt[TT_PLAIN] = CRACK_FLOOR; B.subterranean_mask = 0; B.crack_band_c = 3; B.crack_cool_from = 10;
        B.crack_warm_from = 20; B.crack_recheck = 600; B.crack_tables++; B.off_temperature = 0;   // (no temperature read: a forced -10 C, a band that cracks)
        B.crack_forced_on = 1; B.crack_forced_c = -10;
        B.crack_on = 1; B.crack_gen = 1; B.crack_gen_prev = 0xffffffffu; B.crack_depth = 2;
        for (int d = 0; d < 3; d++) for (int i = 0; i < N; i++) vps[d].L[0][i] = 9000 + i % 50;   // (as DF draws it: the colour put some back)
        for (int k = 0; k < 8; k++) {
            place_cracks();
            if (k == 0) printf("PERF cracks: the first frame, every block worked out: %.1f us, %u blocks\n", B.crack_us_max, B.crack_worked_last);
            while (B.crack_q_tail != B.crack_q_head) {
                const CrackAsk q = B.crack_q[B.crack_q_tail % CRACK_ASK_RING]; B.crack_q_tail++;
                CrackAsk& a = B.crack_a[B.crack_a_head % CRACK_ASK_RING];
                a = q; a.tex = 60000 + (q.base - 9000) * 16 + q.piece; B.crack_a_head++;
            }
        }
        B.crack_frames = 0; B.crack_us_total = 0; B.crack_us_max = 0;
        for (int k = 0; k < PERF_FRAMES; k++) {
            for (int d = 0; d < 3; d++) for (int i = 0; i < N; i += 3) vps[d].L[0][i] = 9000 + i % 50;   // (DF redraws a third)
            place_cracks();
        }
        printf("PERF cracks in place (view's level): %.1f us a frame (worst %.1f), %u cells, %u given their copy, %u waiting\n",
               B.crack_us_total / B.crack_frames, B.crack_us_max, B.crack_cells_last, B.crack_written_last, B.crack_waiting_last);
    }
    return 0;
#endif
    // ---- frame 1: everything unknown: asked, game's colours meanwhile ----
    cell(0, 5, 1, 1) = 1001;        // an item above ground
    cell(0, 0, 1, 2) = 1002;        // plain ground layer, not a tree: never swapped
    cell(0, 0, 3, 3) = 1003;        // ground layer on a tree tile: swapped
    cell(0, 1, 2, 2) = 9004;        // a table copy above ground: left
    cell(0, 0, 0, 0) = 3;           // the game's black: never looked at
    cell(0, 0, 1, 4) = 1007;        // in the tables, not ground (OTHER): swapped in the ground layer
    frame();
    CHECK(cell(0, 5, 1, 1) == 1001, "unknown item swapped before an answer");
    CHECK(B.col_req_head == 5, "questions asked %u, expected 5", B.col_req_head);
    lua_answers();
    // ---- frame 2: answered ----
    frame();
    CHECK(cell(0, 5, 1, 1) == 21001, "item not swapped: %d", cell(0, 5, 1, 1));
    CHECK(cell(0, 0, 1, 2) == 1002, "plain ground swapped: %d", cell(0, 0, 1, 2));
    CHECK(cell(0, 0, 3, 3) == 21003, "tree ground not swapped: %d", cell(0, 0, 3, 3));
    CHECK(cell(0, 1, 2, 2) == 9004, "table copy above ground changed: %d", cell(0, 1, 2, 2));
    CHECK(cell(0, 0, 1, 4) == 21007, "OTHER in the ground layer not swapped: %d", cell(0, 0, 1, 4));
    // ---- frame 3: DF didn't redraw: the copies stay; nothing more asked ----
    uint32_t asked = B.col_req_head;
    frame();
    CHECK(cell(0, 5, 1, 1) == 21001 && B.col_req_head == asked, "copies not kept as they are");
    // ---- RAMPS IN THE LOOK: a ramp in the ground layer is asked about with its class, and swapped ----
    {
        static const uint16_t TT_RAMP_SOIL = 5;
        B.tt_flags[TT_RAMP_SOIL] = TT_RAMP | TT_SOIL;
        ((uint16_t*)(blk(2, 4, 2) + OFF_TT))[tile_of(2, 4)] = TT_RAMP_SOIL;
        cell(0, 0, 2, 4) = 1040;   // the ramp's picture, ground layer
        cell(0, 5, 2, 4) = 1041;   // an item on the ramp
        uint32_t from = B.col_req_head;
        frame();
        uint8_t w_ramp = 0, w_item = 0;
        for (uint32_t k = from; k != B.col_req_head; k++) {
            const ColQuestion& q = B.col_req[k % COL_RING];
            if (q.texpos == 1040) w_ramp = q.where;
            if (q.texpos == 1041) w_item = q.where;
        }
        CHECK(w_ramp == (CW_RAMP | CW_RSOIL), "ramp asked with where %d", w_ramp);
        CHECK(w_item == 0, "item on a ramp asked with ramp bits: %d", w_item);
        lua_answers(); frame();
        CHECK(cell(0, 0, 2, 4) == 21040, "ramp not swapped: %d", cell(0, 0, 2, 4));
        CHECK(cell(0, 5, 2, 4) == 21041, "item on the ramp not swapped: %d", cell(0, 5, 2, 4));
        // past the levels colour looks at, ramps alone: depth 1 as if outside colour's depths
        B.col_depths = 1;
        ((uint16_t*)(blk(2, 4, 1) + OFF_TT))[tile_of(2, 4)] = TT_RAMP_SOIL;
        ((uint32_t*)(blk(2, 4, 1) + OFF_DES))[tile_of(2, 4)] &= ~SUBT;
        ((uint32_t*)(blk(3, 4, 1) + OFF_DES))[tile_of(3, 4)] &= ~SUBT;
        cell(1, 0, 2, 4) = 1042;   // a ramp a level below
        cell(1, 5, 3, 4) = 1043;   // an item a level below: not colour's level now
        frame(); lua_answers(); frame();
        CHECK(cell(1, 0, 2, 4) == 21042, "ramp past colour's levels not swapped: %d", cell(1, 0, 2, 4));
        CHECK(cell(1, 5, 3, 4) == 1043, "item past colour's levels swapped: %d", cell(1, 5, 3, 4));
        B.col_depths = 3;
        ((uint16_t*)(blk(2, 4, 2) + OFF_TT))[tile_of(2, 4)] = TT_PLAIN;
        ((uint16_t*)(blk(2, 4, 1) + OFF_TT))[tile_of(2, 4)] = TT_PLAIN;
        ((uint32_t*)(blk(2, 4, 1) + OFF_DES))[tile_of(2, 4)] |= SUBT;
        ((uint32_t*)(blk(3, 4, 1) + OFF_DES))[tile_of(3, 4)] |= SUBT;
        cell(0, 0, 2, 4) = 0; cell(0, 5, 2, 4) = 0; cell(1, 0, 2, 4) = 0; cell(1, 5, 3, 4) = 0;
        printf("ramps: swapped through the colour\n");
    }
    // ---- PATHS: worn grass planned from the blocks' grass events, painted, swapped in; an edge worn off ----
    {
        static const uint16_t TT_GRASS_FLOOR = 6, TT_SOIL_FLOOR = 7;
        static const uint32_t OUTSIDE = 1u << 17;
        static const uint64_t VT_GRASS = 0x5151;
        B.paths_kind[TT_GRASS_FLOOR] = 1; B.paths_kind[TT_SOIL_FLOOR] = 2;
        B.outside_mask = OUTSIDE; B.off_block_events = 1600; B.grass_vtable = VT_GRASS; B.off_gev_amount = 16;
        B.off_floor_flag = 72;
        static uint64_t ffbuf[N];
        vps[0].FF = ffbuf;   // (the main viewport's floor_flag, at offset 72)
        ffbuf[6 * DY + 3] = 0xff00ull | 0x0a;   // the soil tile: an edge from the grass to its west (byte 2), and another
        static uint8_t ev[16 + 256];
        memset(ev, 0, sizeof ev);
        *(uint64_t*)ev = VT_GRASS;
        static void* evlist[1] = {ev};
        uint8_t* b0 = blk(0, 0, 2);
        *(void**)(b0 + 1600) = &evlist[0]; *(void**)(b0 + 1608) = &evlist[1];
        for (int i = 0; i < 256; i++) {
            ((uint16_t*)(b0 + OFF_TT))[i] = TT_GRASS_FLOOR;
            ((uint32_t*)(b0 + OFF_DES))[i] |= OUTSIDE;
            ev[16 + i] = 100;
        }
        for (int x = 1; x <= 5; x++) ev[16 + tile_of(x, 3)] = 20;   // a worn path along y = 3
        ((uint16_t*)(b0 + OFF_TT))[tile_of(6, 3)] = TT_SOIL_FLOOR;  // running onto bare soil
        B.paths_enabled = 1; B.paths_depths = 0;
        read_view_as_drawn();
        plan_paths();
        const PBlock* pb = p_block_kept(0, 0, 2);
        CHECK(pb && pb->n == 5, "paths: worn tiles planned %d", pb ? pb->n : -1);
        // the viewport's floor_flag buffer, and the soil and grass pictures
        for (int d = 0; d < 3; d++) memset(vps[d].L[0], 0, N * 4);
        static uint32_t soil[1024], grassp[1024], blanks[8][1024];
        for (int k = 0; k < 1024; k++) { soil[k] = 0xff000000u | (40u << 16) | (80u << 8) | 120u; grassp[k] = 0xff000000u | (30u << 16) | (160u << 8) | 50u; }
        B.paths_soil_addr = (uint64_t)(uintptr_t)soil;
        B.murky_blank_head = B.murky_blank_tail = 0;
        for (int k = 0; k < 8; k++) { B.murky_blank_tex[k] = 62000 + k; B.murky_blank_addr[k] = (uint64_t)(uintptr_t)blanks[k]; B.murky_blank_head++; }
        for (int x = 0; x < 8; x++) cell(0, 0, x, 3) = 1050;      // the grass, as the game drew it
        cell(0, 0, 6, 3) = 1051;                                   // the soil
        // (a floor_flag layer for the main viewport: the fake viewport keeps it in a side table)
        draw_paths();
        CHECK(B.paths_q_head == 1 && B.paths_q[0] == 1050, "paths: asked %u (%d)", B.paths_q_head, B.paths_q[0]);
        CHECK(cell(0, 0, 3, 3) == 1050, "paths: drawn before the answer: %d", cell(0, 0, 3, 3));
        B.paths_q_tail = 1;
        B.paths_a[0].texpos = 1050; B.paths_a[0].addr = (uint64_t)(uintptr_t)grassp; B.paths_a_head = 1;
        draw_paths();
        const int32_t t3 = cell(0, 0, 3, 3);
        CHECK(t3 >= 62000 && t3 < 62008, "paths: worn tile not swapped: %d", t3);
        CHECK(cell(0, 0, 0, 3) == 1050 && cell(0, 0, 7, 3) == 1050, "paths: unworn grass swapped");
        // the middle of a track: bare soil
        const uint32_t mid = blanks[t3 - 62000][15 * 32 + 15];
        CHECK(mid == soil[0], "paths: track middle %08x, soil %08x", mid, soil[0]);
        CHECK(B.paths_cells_last == 5, "paths: cells drawn %u", B.paths_cells_last);
        const int64_t baked = B.paths_baked;
        for (int x = 0; x < 8; x++) cell(0, 0, x, 3) = 1050;
        draw_paths();
        CHECK(B.paths_baked == baked && B.paths_cells_last == 5, "paths: painted again: %lld", (long long)B.paths_baked);
        CHECK(ffbuf[6 * DY + 3] == 0x0a, "paths: edge onto the soil not worn off: %llx", (unsigned long long)ffbuf[6 * DY + 3]);
        printf("paths: %d worn tiles, %lld paintings\n", pb->n, (long long)B.paths_baked);
        B.paths_enabled = 0; B.off_floor_flag = 0; vps[0].FF = nullptr;
        for (int i = 0; i < 256; i++) { ((uint16_t*)(b0 + OFF_TT))[i] = TT_PLAIN; ((uint32_t*)(b0 + OFF_DES))[i] &= ~OUTSIDE; }
        *(void**)(b0 + 1600) = nullptr; *(void**)(b0 + 1608) = nullptr;
        for (int d = 0; d < 3; d++) memset(vps[d].L[0], 0, N * 4);
    }
    // ---- underground: copies put back, table copies put back in the ground layer and wet flows ----
    wx = 16;   // the view moves onto the underground half
    for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
    cell(0, 5, 1, 1) = 21001;        // a copy left underground: back to its original
    cell(0, 0, 2, 2) = 9005;        // a table copy, ground layer: put back
    cell(0, 3, 4, 4) = 9006;        // a table copy, building layer, water there: put back
    cell(0, 3, 4, 3) = 9008;        // the same, no water: left
    cell(0, 5, 2, 1) = 1001;        // an original with a copy: not swapped underground
    frame(); lua_answers(); frame();
    CHECK(cell(0, 5, 1, 1) == 1001, "copy underground not put back: %d", cell(0, 5, 1, 1));
    CHECK(cell(0, 0, 2, 2) == 1005, "table copy underground not put back: %d", cell(0, 0, 2, 2));
    CHECK(cell(0, 3, 4, 4) == 1006, "wet flow not put back: %d", cell(0, 3, 4, 4));
    CHECK(cell(0, 3, 4, 3) == 9008, "dry building layer put back: %d", cell(0, 3, 4, 3));
    CHECK(cell(0, 5, 2, 1) == 1001, "swapped underground: %d", cell(0, 5, 2, 1));
    // ---- a level below: depth 1 is z 1, underground everywhere ----
    wx = 0;
    for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
    cell(1, 0, 1, 1) = 9010;
    frame(); lua_answers(); frame();
    CHECK(cell(1, 0, 1, 1) == 1010, "table copy a level below underground not put back: %d", cell(1, 0, 1, 1));
    // ---- HELD: a new number where a copy was: last frame's copy kept, up to HOLD_FRAMES ----
    cell(0, 1, 5, 5) = 1020; frame(); lua_answers(); frame();
    CHECK(cell(0, 1, 5, 5) == 21020, "first copy: %d", cell(0, 1, 5, 5));
    cell(0, 1, 5, 5) = 1021; frame();   // remade under a new number: held on 21020 until answered
    CHECK(cell(0, 1, 5, 5) == 21020, "not held: %d", cell(0, 1, 5, 5));
    lua_answers(); cell(0, 1, 5, 5) = 1021; frame();
    CHECK(cell(0, 1, 5, 5) == 21021, "answer not used: %d", cell(0, 1, 5, 5));
    // never answered: held 3 frames, then the game's own
    cell(0, 1, 5, 5) = 1666;
    for (int k = 0; k < 3; k++) { frame(); CHECK(cell(0, 1, 5, 5) == 21021, "hold %d: %d", k, cell(0, 1, 5, 5)); cell(0, 1, 5, 5) = 1666; }
    frame();
    CHECK(cell(0, 1, 5, 5) == 1666, "held past HOLD_FRAMES: %d", cell(0, 1, 5, 5));
    B.col_req_tail = B.col_req_head;   // (1666 left unanswered)
    // ---- THE HELD SLOTS (15L): a pan keeps each tile's held copy; a tile new to the view borrows nothing ----
    for (int l = 0; l < 7; l++) memset(vps[0].L[l], 0, N * 4);
    cell(0, 1, 5, 4) = 1030;        // tile (5, 4)
    cell(0, 1, 0, 4) = 1033;        // tile (0, 4): leaves the view on the pan; its slot is the one tile (8, 4) takes
    frame(); lua_answers(); frame();
    CHECK(cell(0, 1, 5, 4) == 21030 && cell(0, 1, 0, 4) == 21033, "pan setup: %d %d", cell(0, 1, 5, 4), cell(0, 1, 0, 4));
    { const int64_t held0 = B.col_held, moved0 = B.col_held_moved, new0 = B.col_wait_new, nohold0 = B.col_wait_nohold;
      wx = 1;   // pan a tile east: tile (5, 4) is now screen 4; tile (8, 4) comes in at screen 7
      for (int l = 0; l < 7; l++) memset(vps[0].L[l], 0, N * 4);
      cell(0, 1, 4, 4) = 1031;      // tile (5, 4) drawn under a new number, not yet answered
      cell(0, 1, 7, 4) = 1034;      // tile (8, 4), new to the view, not yet answered
      cell(0, 1, 2, 4) = 1035;      // tile (3, 4): in view last frame with nothing held there
      frame();
      CHECK(cell(0, 1, 4, 4) == 21030, "held copy lost on the pan: %d", cell(0, 1, 4, 4));
      CHECK(cell(0, 1, 7, 4) == 1034, "a tile new to the view took another tile's held copy: %d", cell(0, 1, 7, 4));
      CHECK(cell(0, 1, 2, 4) == 1035, "held a copy where none was: %d", cell(0, 1, 2, 4));
      CHECK(B.col_held - held0 == 1 && B.col_held_moved - moved0 == 1, "held counts: %lld %lld",
            (long long)(B.col_held - held0), (long long)(B.col_held_moved - moved0));
      CHECK(B.col_wait_new - new0 == 1 && B.col_wait_nohold - nohold0 == 1, "wait counts: new %lld, no hold %lld",
            (long long)(B.col_wait_new - new0), (long long)(B.col_wait_nohold - nohold0));
      // pan back: the hold keeps running from the same entry, then answers take over
      lua_answers();
      wx = 0;
      for (int l = 0; l < 7; l++) memset(vps[0].L[l], 0, N * 4);
      cell(0, 1, 5, 4) = 1031; cell(0, 1, 8 - 1, 4) = 0;
      frame();
      CHECK(cell(0, 1, 5, 4) == 21031, "answer not used after the pan back: %d", cell(0, 1, 5, 4));
      // a resize empties the lot: nothing held across it
      B.col_req_tail = B.col_req_head;
    }
    // ---- the look changes: Lua points every original at the new look's copy ----
    cell(0, 5, 1, 1) = 21001;   // still showing look A's copy
    look = 1;
    for (int32_t t = 1000; t < 1100; t++) if (g_cflag[t] & CF_COPY_FOR) publish(t);
    frame();
    CHECK(cell(0, 5, 1, 1) == 31001, "old look's copy not moved to the new look's: %d", cell(0, 5, 1, 1));
    // ---- SEEN: listed once a period ----
    uint32_t seen = B.col_seen_head; frame();
    CHECK(B.col_seen_head == seen, "listed twice in one period");
    B.col_period++; frame();
    CHECK(B.col_seen_head > seen, "not listed in a new period");
    // ---- an original whose copy was taken away (Lua cleared it): its copy goes back ----
    g_cflag[1001] &= ~(CF_COPY_FOR | CF_ASKED); frame();
    CHECK(cell(0, 5, 1, 1) == 1001, "copy with no copy-for left: %d", cell(0, 5, 1, 1));
    frame();   // (asked about on the next frame, when the cell shows the original)
    CHECK(B.col_req_tail != B.col_req_head, "not asked again");
    // ---- SWAY: a canopy tile's original listed once, frames given, the loop's frame shown by position ----
    {
        static const uint16_t TT_TWIG = 3;
        B.col_tt[TT_TWIG] = 1; B.tt_flags[TT_TWIG] = TT_TREE;
        ((uint16_t*)(blk(6, 2, 2) + OFF_TT))[tile_of(6, 2)] = TT_TWIG;
        ((uint16_t*)(blk(7, 2, 2) + OFF_TT))[tile_of(7, 2)] = TT_TWIG;
        B.sway_reset = 1; B.sway_playing = -1; B.sway_ux = 1.0; B.sway_uy = 0.0; B.sway_waves = 4;
        // (the prerender's sway reset)
        memset(g_sway_of, 0, sizeof g_sway_of); memset(g_canopy_listed, 0, sizeof g_canopy_listed); B.sway_reset_done = 1;
        cell(0, 1, 6, 2) = 1030; cell(0, 1, 7, 2) = 1030;
        frame(); lua_answers(); B.col_seen_tail = B.col_seen_head; frame();
        CHECK(cell(0, 1, 6, 2) == 31030, "canopy copy: %d", cell(0, 1, 6, 2));
        // listed with SEEN_CANOPY once
        uint32_t h0 = B.col_seen_tail; int listed = 0;
        cell(0, 1, 6, 2) = 1030; cell(0, 1, 7, 2) = 1030; frame(); frame();
        // (it was listed on the frame its copy first showed: look back over the ring)
        for (uint32_t k = 0; k < B.col_seen_head; k++) if (B.col_seen[k % COL_RING] == (1030 | SEEN_CANOPY)) listed++;
        CHECK(listed == 1, "canopy original listed %d times", listed);
        (void)h0;
        // frames: slot 0, frame p = 40000 + p (frame 5 is the copy itself: 0)
        for (int p = 0; p < 16; p++) g_sway_tex[0][p] = p == 5 ? 0 : 40000 + p;
        g_sway_of[1030] = 1;
        for (int p = 0; p < 16; p++) { g_cflag[40000 + p] = CF_SCREEN; g_link[40000 + p] = 1030; }
        B.sway_playing = 9; frame();
        // x 6: start floor(6 * 4) = 24, phase (9 - 24) mod 16 = 1; x 7: start 28, phase 13
        CHECK(cell(0, 1, 6, 2) == 40001, "x 6 frame: %d", cell(0, 1, 6, 2));
        CHECK(cell(0, 1, 7, 2) == 40013, "x 7 frame: %d", cell(0, 1, 7, 2));
        B.sway_playing = 10; frame();   // the cells now show frames: taken back to 1030, then on
        CHECK(cell(0, 1, 6, 2) == 40002 && cell(0, 1, 7, 2) == 40014, "next frame: %d %d", cell(0, 1, 6, 2), cell(0, 1, 7, 2));
        B.sway_playing = 12; frame();   // x 7: (12 - 28) mod 16 = 0; x 6: (12 - 24) mod 16 = 4; then 5 is the copy
        CHECK(cell(0, 1, 6, 2) == 40004 && cell(0, 1, 7, 2) == 40000, "frames 4 and 0: %d %d", cell(0, 1, 6, 2), cell(0, 1, 7, 2));
        B.sway_playing = 13; frame();
        CHECK(cell(0, 1, 6, 2) == 31030, "frame 5 (none) not the copy: %d", cell(0, 1, 6, 2));
        B.sway_playing = -1; frame();
        CHECK(cell(0, 1, 6, 2) == 31030 && cell(0, 1, 7, 2) == 31030, "still: not the copies: %d %d", cell(0, 1, 6, 2), cell(0, 1, 7, 2));
        // a non-canopy cell with the same original never rustles
        cell(0, 5, 6, 2) = 1030; B.sway_playing = 9; frame();
        CHECK(cell(0, 5, 6, 2) == 40001, "(the item layer on a canopy tile rustles too, as in Lua: %d)", cell(0, 5, 6, 2));
        cell(0, 1, 2, 4) = 1030; frame();
        CHECK(cell(0, 1, 2, 4) == 31030, "off the canopy rustled: %d", cell(0, 1, 2, 4));
        B.sway_playing = -1;
    }
    // ---- THE SHADE: a covered square at x 2..3, y 2..3 on z 2 (neither outside nor underground) ----
    {
        static const uint32_t OUT = 1u << 16;
        for (int x = 0; x < 16; x++) for (int y = 0; y < 16; y++) ((uint32_t*)(blk(x, y, 2) + OFF_DES))[tile_of(x, y)] = OUT;
        for (int x = 2; x <= 3; x++) for (int y = 2; y <= 3; y++) ((uint32_t*)(blk(x, y, 2) + OFF_DES))[tile_of(x, y)] = 0;
        ((uint32_t*)(blk(3, 3, 2) + OFF_DES))[tile_of(3, 3)] = 2;   // covered, with water
        static int32_t desig[N];
        struct VPX { uint32_t flag; int32_t dx, dy; int32_t pad; int32_t* L[8]; };
        // (the designation layer at offset 72: after the seven at 16..64)
        vps[0].L[0] = vps[0].L[0];
        B.off_designation_layer = 72; B.outside_mask = OUT; B.subterranean_mask = SUBT;
        for (int k = 0; k < SHADE_N; k++) B.shade_tex[k] = 50000 + k;
        B.shade_enabled = 1; wx = 0; wy = 0; wz = 2;
        // the fake viewport has room for an eighth pointer only if VP grows: use a raw one
        static uint8_t raw[128]; memcpy(raw, &vps[0], sizeof(VP));
        *(int32_t**)(raw + 72) = desig;
        uint8_t* rawp = raw; main_ptr = (VP*)rawp;
        read_view_as_drawn();
        draw_shade();
        auto sh = [&](int x, int y) { return desig[x * DY + y]; };
        // tile (2, 2): corners tl (2,2)=1 covered of 4 -> 1; tr (3,2)=2; bl (2,3)=2; br (3,3)=4
        CHECK(sh(2, 2) == 50000 + ((1 * 5 + 2) * 5 + 2) * 5 + 4, "shade at (2,2): %d", sh(2, 2));
        CHECK(sh(3, 3) == 0, "shade on water: %d", sh(3, 3));
        CHECK(sh(1, 1) == 50000 + ((0 * 5 + 0) * 5 + 0) * 5 + 1, "shade edge at (1,1): %d", sh(1, 1));
        CHECK(sh(5, 5) == 0 && sh(0, 0) == 0, "shade far off: %d %d", sh(5, 5), sh(0, 0));
        desig[4 * DY + 2] = 7;   // a mark of the game's own: left alone
        draw_shade();
        CHECK(desig[4 * DY + 2] == 7, "game's mark overwritten");
        main_ptr = &vps[0];
        printf("shade: %lld cells, %.1f us\n", (long long)B.shade_written, B.shade_us_total / B.shade_frames);
        B.shade_enabled = 0;
    }
    // ---- MURKY: stagnant water on the view's level, and a level below seen through open air ----
    {
        static const uint16_t TT_AIR = 9;
        B.tt_flags[TT_AIR] = TT_OPEN;
        const uint32_t STAG = 1u << 30;
        wx = 0; wy = 0; wz = 2;
        for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
        ((uint32_t*)(blk(2, 1, 2) + OFF_DES))[tile_of(2, 1)] = STAG | 3 | (1u << 16);     // stagnant, on the level shown
        ((uint32_t*)(blk(4, 1, 2) + OFF_DES))[tile_of(4, 1)] = STAG | 3 | (1u << 21);     // stagnant magma: no
        ((uint16_t*)(blk(5, 2, 2) + OFF_TT))[tile_of(5, 2)] = TT_AIR;                     // open air over (5, 2, 1)
        ((uint32_t*)(blk(5, 2, 1) + OFF_DES))[tile_of(5, 2)] = STAG | 2;                  // stagnant, a level below
        ((uint32_t*)(blk(6, 2, 1) + OFF_DES))[tile_of(6, 2)] = STAG | 2;                  // a level below, under solid: no
        cell(0, 1, 3, 3) = 0;
        B.stagnant_mask = STAG; B.magma_mask = 1u << 21; B.hidden_mask = 1u << 9; B.murky_depths = 3;
        for (int k = 0; k < 16; k++) B.murky_tex[k] = 60000 + k;
        B.murky_enabled = 1; B.col_enabled = 0;
        // (WATER SEEN FROM ABOVE: the main view's designation layer, at offset 72 past the seven)
        // (WATER SEEN FROM ABOVE: the main view's ground layer)
        auto desig_cell = [&](int x, int y) { return cell(0, 1, x, y); };
        read_view_as_drawn();
        draw_murky();
        CHECK(cell(0, 1, 2, 1) == 60000 + (1 & 3) * 4 + (2 & 3), "murky on the level shown: %d", cell(0, 1, 2, 1));
        CHECK(cell(0, 1, 4, 1) == 0, "murky on magma: %d", cell(0, 1, 4, 1));
        CHECK(desig_cell(5, 2) == 60000 + (2 & 3) * 4 + (5 & 3), "murky a level below through air: %d", desig_cell(5, 2));
        CHECK(desig_cell(6, 2) == 0, "murky under solid ground: %d", desig_cell(6, 2));
        cell(0, 1, 2, 1) = 77; draw_murky();
        CHECK(cell(0, 1, 2, 1) == 77, "murky over something already there");
        cell(0, 1, 5, 2) = 88; draw_murky();   // the water's ripple in the cell above the water
        CHECK(desig_cell(5, 2) == 60000 + (2 & 3) * 4 + (5 & 3), "murky not over the water's ripple from above: %d", desig_cell(5, 2));
        // (15G) water two levels down: the tint in the layer above the ground of the level just above it
        // (the lower viewport's), not the view's
        ((uint16_t*)(blk(5, 4, 2) + OFF_TT))[tile_of(5, 4)] = TT_AIR;   // open air on the view's level
        ((uint16_t*)(blk(5, 4, 1) + OFF_TT))[tile_of(5, 4)] = TT_AIR;   // and on the level below it
        ((uint32_t*)(blk(5, 4, 0) + OFF_DES))[tile_of(5, 4)] = STAG | 2;   // stagnant water two levels down
        cell(0, 1, 5, 4) = 0; cell(1, 1, 5, 4) = 0;
        draw_murky();
        CHECK(cell(1, 1, 5, 4) == 60000 + (4 & 3) * 4 + (5 & 3) && cell(0, 1, 5, 4) == 0,
              "murky two levels down not in the level above it: %d, view's %d", cell(1, 1, 5, 4), cell(0, 1, 5, 4));
        // (15P, 15Q: BRANCHES SEEN THROUGH) stagnant water seen through a tree's branches higher up is tinted
        // as through open air; with a branch on the level just above it, left untinted (that level's layer
        // may hold the branch), counted and probed first
        {
            static const uint16_t TT_BRANCH = 10;
            B.tt_flags[TT_BRANCH] = TT_TREE;   // (branches: a tree tile, not a trunk, not open)
            // a branch on the view's level directly above stagnant water a level down: left
            ((uint16_t*)(blk(3, 5, 2) + OFF_TT))[tile_of(3, 5)] = TT_BRANCH;
            ((uint32_t*)(blk(3, 5, 1) + OFF_DES))[tile_of(3, 5)] = STAG | 7;
            cell(0, 1, 3, 5) = 4321;   // (the branch as the game drew it, in the layer the tint would use)
            // a branch on the view's level, open air below it, stagnant water two levels down: tinted on the level between
            ((uint16_t*)(blk(7, 4, 2) + OFF_TT))[tile_of(7, 4)] = TT_BRANCH;
            ((uint16_t*)(blk(7, 4, 1) + OFF_TT))[tile_of(7, 4)] = TT_AIR;
            ((uint32_t*)(blk(7, 4, 0) + OFF_DES))[tile_of(7, 4)] = STAG | 7;
            cell(0, 1, 7, 4) = 5432; cell(1, 1, 7, 4) = 0;
            B.probe_ask++;
            draw_murky();
            CHECK(cell(0, 1, 3, 5) == 4321, "a branch just above the water drawn over: %d", cell(0, 1, 3, 5));
            CHECK(cell(1, 1, 7, 4) == 60000 + (4 & 3) * 4 + (7 & 3) && cell(0, 1, 7, 4) == 5432,
                  "water seen through a branch higher up not tinted on the level between (%d), or the branch touched (%d)",
                  cell(1, 1, 7, 4), cell(0, 1, 7, 4));
            // (branch just above: this one and (6, 2, 1), under a tree tile an earlier test left at (6, 2, 2))
            CHECK(B.murky_branch_above_last == 2 && B.murky_under_branch_last == 1,
                  "counts: branch just above %u (2 wanted), tinted through branches %u (1 wanted)",
                  B.murky_branch_above_last, B.murky_under_branch_last);
            CHECK(B.probe_done == B.probe_ask && B.probe_d == 1 && B.probe_x == 3 && B.probe_y == 5 && B.probe_through == 1,
                  "the probe didn't take the water with a branch just above: done %d, d %d at %d,%d, through %u",
                  B.probe_done == B.probe_ask, B.probe_d, B.probe_x, B.probe_y, B.probe_through);
            CHECK(desig_cell(5, 2) == 60000 + (2 & 3) * 4 + (5 & 3), "murky through air lost with branches in view: %d", desig_cell(5, 2));
            CHECK(cell(1, 1, 5, 4) == 60000 + (4 & 3) * 4 + (5 & 3), "murky two down lost with branches in view: %d", cell(1, 1, 5, 4));
            ((uint16_t*)(blk(3, 5, 2) + OFF_TT))[tile_of(3, 5)] = TT_PLAIN;
            ((uint32_t*)(blk(3, 5, 1) + OFF_DES))[tile_of(3, 5)] = SUBT;
            ((uint16_t*)(blk(7, 4, 2) + OFF_TT))[tile_of(7, 4)] = TT_PLAIN;
            ((uint16_t*)(blk(7, 4, 1) + OFF_TT))[tile_of(7, 4)] = TT_PLAIN;
            ((uint32_t*)(blk(7, 4, 0) + OFF_DES))[tile_of(7, 4)] = SUBT;
        }
        printf("murky: %lld cells, %.1f us\n", (long long)B.murky_written, B.murky_us_total / B.murky_frames);
        B.murky_enabled = 0; B.col_enabled = 1;
    }
    // ---- MURKY: EDGES OVER THE TINT: the tint with overhang pieces laid over it, made once a combination ----
    {
        static uint32_t tint[1024], south[1024], west[1024], blanks[2][1024];
        for (int p = 0; p < 1024; p++) {
            tint[p] = 0xff000000u | (40u << 16) | (60u << 8) | 30u;               // opaque murk
            south[p] = p < 512 ? 0 : (0xff000000u | (0u << 16) | (200u << 8) | 0u);   // bottom half opaque green
            west[p] = (p % 32) < 16 ? (0x80000000u | (0u << 16) | (0u << 8) | 255u) : 0;   // left half, half see-through red
        }
        B.murky_tex[5] = 60005;
        B.murky_tint_addr[5] = (uint64_t)(uintptr_t)tint;
        B.edge_addr[12][SIDE_PIECE[0]] = (uint64_t)(uintptr_t)south;   // edging ID 12, its S piece
        B.edge_addr[12][SIDE_PIECE[1]] = (uint64_t)(uintptr_t)west;    // and its W piece
        B.murky_blank_head = B.murky_blank_tail = 0;
        for (int b = 0; b < 2; b++) {
            B.murky_blank_tex[b] = 61000 + b;
            B.murky_blank_addr[b] = (uint64_t)(uintptr_t)blanks[b];
            B.murky_blank_head++;
        }
        const uint32_t sides = 12u | (12u << 8);   // S and W overhung by ID 12
        (void)sides;
        const uint16_t sw[2] = {12 * 8 + 1, 12 * 8 + 2}, s_only[1] = {12 * 8 + 1}, w_only[1] = {12 * 8 + 2};
        const int32_t t1 = tint_with_edges(5, sw, 2);
        CHECK(t1 == 61000, "edges: first bake: %d", t1);
        CHECK(blanks[0][0] == 0xff000000u + ((40u << 16) | (60u << 8) | 30u) + 0 * 0 ||
              blanks[0][0] == ((0xffu << 24) | (40u << 16) | (60u << 8) | (uint32_t)(30 + (255 - 30) * 128 / 255.0 + 0.5)) ||
              true, "");
        // top-left pixel: murk, then half red over it
        const uint32_t tl = blanks[0][0];
        CHECK((tl >> 24) == 255 && (tl & 0xff) == (uint32_t)(255 * (128 / 255.0) + 30 * (1 - 128 / 255.0) + 0.5) &&
              ((tl >> 8) & 0xff) == (uint32_t)(60 * (1 - 128 / 255.0) + 0.5), "edges: half red over the murk: %08x", tl);
        CHECK(blanks[0][1023] == (0xff000000u | (200u << 8)), "edges: opaque green over the murk: %08x", blanks[0][1023]);
        CHECK(blanks[0][31] == tint[31], "edges: murk where no piece: %08x", blanks[0][31]);
        const int32_t t2 = tint_with_edges(5, sw, 2);
        CHECK(t2 == 61000 && B.murky_blank_tail == 1, "edges: kept, not made again: %d %u", t2, B.murky_blank_tail);
        const int32_t t3 = tint_with_edges(5, s_only, 1);
        CHECK(t3 == 61001, "edges: another combination: %d", t3);
        const int32_t t4 = tint_with_edges(5, w_only, 1);
        CHECK(t4 == 0 && B.murky_bake_waits == 1, "edges: no blank left, plain tint: %d", t4);
        // the pieces over a cell of water two levels down: the level above the water's first (its
        // floor_flag, a lower viewport's), then the view's own
        static uint64_t ff0[4], ff1[4];
        ff0[2] = 12ull << 8;   // the view's level: an edge from the west
        ff1[2] = 12ull;        // the level just above the water: an edge from the south
        uint64_t* ffs2[MURKY_MOST_DEPTH] = {ff0, ff1};
        uint16_t codes[MURKY_MOST_PIECES];
        // (15G) water two levels down: only the level just above it, where the tint goes; the view's own
        // edges are drawn over it by the game
        const int nc = murky_pieces(2, 2, 40, 40, ffs2, codes);
        CHECK(nc == 1 && codes[0] == 12 * 8 + 1, "edges of the level above the water: %d (%d)", nc, codes[0]);
        const int nc1 = murky_pieces(1, 2, 40, 40, ffs2, codes);
        CHECK(nc1 == 1 && codes[0] == 12 * 8 + 2, "edges from the view's level for water one down: %d (%d)", nc1, codes[0]);
        printf("murky edges: %lld baked\n", (long long)B.murky_baked);
    }
    // ---- FOG OVER WATER: a deep water cell's haze set to its surface's level, put back before DF draws ----
    {
        static const uint16_t TT_AIR = 9, TT_TWIG = 11, TT_BRANCH = 12, TT_TRUNK_T = 13;
        B.tt_flags[TT_AIR] = TT_OPEN; B.tt_flags[TT_TWIG] = TT_TREE;
        B.tt_flags[TT_BRANCH] = TT_TREE | TT_FLOOR; B.tt_flags[TT_TRUNK_T] = TT_TREE | TT_TRUNK;
        static int32_t hazes[136];
        for (int k = 0; k < 8; k++) hazes[k] = 70000 + k;
        hazes[8] = 1;   // (the ramp overlays that follow in gps.texture_indices1)
        static int32_t ifa[3][N], ifb[3][N];   // each level's two interface buffers (swap_arrays)
        memset(ifa, 0, sizeof ifa); memset(ifb, 0, sizeof ifb);
        for (int d = 0; d < 3; d++) { vps[d].IF = ifa[d]; for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4); }
        B.off_interface = 80; B.fog_tex_addr = (uint64_t)(uintptr_t)hazes; B.flow_mask = 7; B.hidden_mask = 1u << 9;
        wx = 0; wy = 0; wz = 2;
        auto tt = [&](int x, int y, int z, uint16_t t) { ((uint16_t*)(blk(x, y, z) + OFF_TT))[tile_of(x, y)] = t; };
        auto des = [&](int x, int y, int z, uint32_t v) { ((uint32_t*)(blk(x, y, z) + OFF_DES))[tile_of(x, y)] = v; };
        auto idx = [&](int x, int y) { return x * DY + y; };
        // row y 5 (and one cell at y 4); the view's level z 2, the water's z 0 (two levels down)
        for (int x = 0; x < 8; x++) for (int y = 4; y < 6; y++) for (int z = 0; z < 3; z++) { tt(x, y, z, TT_PLAIN); des(x, y, z, 0); }
        auto deep = [&](int x, int y, uint16_t top, uint16_t mid, uint32_t water, bool surface) {
            tt(x, y, 2, top); tt(x, y, 1, mid); des(x, y, 0, water);
            if (surface) vps[1].L[0][idx(x, y)] = 555;   // (the game's surface on the level above the water)
        };
        deep(0, 5, TT_AIR, TT_AIR, 7, true);       // A: open space down to full water: moved
        deep(1, 5, TT_TWIG, TT_AIR, 7, true);      // B: leaves on the view's level over it: moved
        deep(2, 5, TT_AIR, TT_BRANCH, 7, false);   // C: a branch just above the water, no surface asked: moved
        deep(3, 5, TT_AIR, TT_AIR, 3, true);       // D: shallow water (3 of 7): not marked
        deep(4, 5, TT_AIR, TT_AIR, 7, false);      // E: nothing drawn on the open space above: left (dry)
        deep(5, 5, TT_AIR, TT_AIR, 7, true);       // F: the game already laid the surface's haze: left
        deep(6, 5, TT_AIR, TT_AIR, 7, true);       // G: another haze altogether: left (unmatched)
        tt(7, 5, 2, TT_AIR); des(7, 5, 1, 7);      // H: water one level down: not marked
        deep(0, 4, TT_AIR, TT_AIR, 7, true);       // A2: as A, its haze in the level two down
        tt(1, 4, 2, TT_TRUNK_T); des(1, 4, 0, 7);  // I: a trunk on the view's level: the bottom, not marked
        auto df_draws = [&]() {   // the game's own hazes, as it lays them (a buffer it draws into)
            for (int x = 0; x < 7; x++) vps[1].IF[idx(x, 5)] = hazes[1];   // (bottom two down: haze 1)
            vps[1].IF[idx(5, 5)] = hazes[0];
            vps[1].IF[idx(6, 5)] = hazes[3];
            vps[1].IF[idx(7, 5)] = hazes[0];                                 // (H: bottom one down)
            vps[2].IF[idx(0, 4)] = hazes[1];
        };
        auto swap = [&]() { for (int d = 0; d < 3; d++) vps[d].IF = vps[d].IF == ifa[d] ? ifb[d] : ifa[d]; };
        B.fog_enabled = 1;
        // frame 1: buffer a
        read_view_as_drawn(); fog_put_back(); plan_fog(); df_draws(); fix_water_fog();
        CHECK(B.fog_marked_last == 7, "fog: cells marked %u", B.fog_marked_last);
        CHECK(ifa[1][idx(0, 5)] == hazes[0] && ifa[1][idx(1, 5)] == hazes[0] && ifa[1][idx(2, 5)] == hazes[0],
              "fog: open space, leaves above, a branch above not moved: %d %d %d", ifa[1][idx(0, 5)], ifa[1][idx(1, 5)], ifa[1][idx(2, 5)]);
        CHECK(ifa[2][idx(0, 4)] == hazes[0], "fog: a haze two levels down not moved: %d", ifa[2][idx(0, 4)]);
        CHECK(ifa[1][idx(3, 5)] == hazes[1], "fog: shallow water moved: %d", ifa[1][idx(3, 5)]);
        CHECK(ifa[1][idx(4, 5)] == hazes[1] && B.fog_dry_last == 1, "fog: moved with nothing drawn above: %d (%u)", ifa[1][idx(4, 5)], B.fog_dry_last);
        CHECK(ifa[1][idx(5, 5)] == hazes[0] && B.fog_already_last == 1, "fog: already the surface's: %u", B.fog_already_last);
        CHECK(ifa[1][idx(6, 5)] == hazes[3] && B.fog_unmatched_last == 1 && B.fog_odd_d == 2 && B.fog_odd_slot == 1 && B.fog_odd_haze == 3,
              "fog: another haze touched, or not reported: %d (%u, %d %d %d)", ifa[1][idx(6, 5)], B.fog_unmatched_last, B.fog_odd_d, B.fog_odd_slot, B.fog_odd_haze);
        CHECK(ifa[1][idx(7, 5)] == hazes[0], "fog: water one level down touched: %d", ifa[1][idx(7, 5)]);
        CHECK(B.fog_moved_last == 4 && g_fog_kept_n == 4, "fog: moved %u, kept %d", B.fog_moved_last, g_fog_kept_n);
        // frame 2: buffer b, drawn afresh
        swap(); read_view_as_drawn(); fog_put_back(); plan_fog(); df_draws(); fix_water_fog();
        CHECK(ifb[1][idx(0, 5)] == hazes[0] && g_fog_kept_n == 8, "fog: second buffer: %d, kept %d", ifb[1][idx(0, 5)], g_fog_kept_n);
        // frame 3: buffer a again, which the game leaves as it was: the game's own back before it draws, then moved again
        swap(); read_view_as_drawn(); fog_put_back();
        CHECK(ifa[1][idx(0, 5)] == hazes[1] && ifa[2][idx(0, 4)] == hazes[1] && g_fog_kept_n == 4,
              "fog: not put back before the game draws: %d %d, kept %d", ifa[1][idx(0, 5)], ifa[2][idx(0, 4)], g_fog_kept_n);
        plan_fog(); fix_water_fog();
        CHECK(ifa[1][idx(0, 5)] == hazes[0] && B.fog_moved_last == 4, "fog: not moved again: %d (%u)", ifa[1][idx(0, 5)], B.fog_moved_last);
        // the census, asked: the game's own hazes in view, by level
        B.fog_census_ask++;
        swap(); read_view_as_drawn(); fog_put_back(); plan_fog(); fix_water_fog();
        CHECK(B.fog_census_done == B.fog_census_ask && B.fog_census[1][1] == 5 && B.fog_census[1][0] == 2 &&
              B.fog_census[1][3] == 1 && B.fog_census[2][1] == 1 && B.fog_census_cells == 9 && B.fog_census_multi == 0,
              "fog: census %u/%u: [1][1] %u, [1][0] %u, [1][3] %u, [2][1] %u, cells %u, more than one %u", B.fog_census_done, B.fog_census_ask,
              B.fog_census[1][1], B.fog_census[1][0], B.fog_census[1][3], B.fog_census[2][1], B.fog_census_cells, B.fog_census_multi);
        // turned off: the game's own everywhere two frames later
        B.fog_enabled = 0;
        swap(); read_view_as_drawn(); fog_put_back(); plan_fog(); fix_water_fog();
        swap(); read_view_as_drawn(); fog_put_back(); plan_fog(); fix_water_fog();
        CHECK(ifa[1][idx(0, 5)] == hazes[1] && ifb[1][idx(0, 5)] == hazes[1] && ifa[2][idx(0, 4)] == hazes[1] &&
              ifb[2][idx(0, 4)] == hazes[1] && g_fog_kept_n == 0,
              "fog: off, not all put back: %d %d %d %d, kept %d", ifa[1][idx(0, 5)], ifb[1][idx(0, 5)], ifa[2][idx(0, 4)], ifb[2][idx(0, 4)], g_fog_kept_n);
        printf("fog over water: %lld moved, %lld put back, plan %.1f us, fix %.1f us\n", (long long)B.fog_moved,
               (long long)B.fog_put_back, B.fog_plan_us_total / B.fog_plans, B.fog_us_total / B.fog_frames);
        for (int d = 0; d < 3; d++) vps[d].IF = nullptr;
        B.off_interface = 0;
    }
    // ---- PICTURES CHECKED HERE: a verified picture's fingerprint kept, checked, a change noticed ----
    {
        static uint32_t pix[32 * 40];   // a surface with a padded row (pitch 160)
        for (int i = 0; i < 32 * 40; i++) pix[i] = 0xff000000u | (uint32_t)(i * 2654435761u >> 8);
        struct Surf { uint32_t flags; uint32_t pad; void* fmt; int32_t w, h, pitch, pad2; uint32_t* px; } surf = {0, 0, nullptr, 32, 32, 160, 0, pix};
        static void* raws[600] = {};
        raws[555] = &surf;
        struct Vec { void** first; void** last; } vec = {raws, raws + 600};
        B.col_raws = (uint64_t)(uintptr_t)&vec;
        // colour.lua's sample_hash, by hand
        uint32_t want = 0;
        for (int k = 0; k < 16; k++) { int n = (k * 389) % 1024; uint32_t v = pix[(n / 32) * 40 + n % 32]; want = (v >> 24) ? want * 31u + v : want * 31u; }
        uint32_t got = 0;
        CHECK(pic_print(555, got) && got == want, "pictures: fingerprint %u, colour.lua's %u", got, want);
        CHECK(!pic_same(555), "pictures: checked here before it was verified");
        pic_keep(555);
        const int64_t c0 = B.col_pics_checked;
        CHECK(pic_same(555) && B.col_pics_checked == c0 + 1, "pictures: unchanged picture not same");
        pix[(389 / 32) * 40 + 389 % 32] ^= 0x00010101u;   // a sampled pixel changes
        CHECK(!pic_same(555) && B.col_pics_changed == 1, "pictures: a change not noticed");
        CHECK(!pic_same(555), "pictures: still checked after the change was sent");
        CHECK(!pic_print(599, got) && !pic_print(700, got), "pictures: no surface, or past the list, fingerprinted");
        printf("pictures checked here: checks pass\n");
        B.col_raws = 0;
    }
    // ---- CRACKS IN PLACE (15R to 15U): tiles worked out from the map, copies asked for and placed, held over, cleared ----
    {
        for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
        wx = 0; wy = 0; wz = 2; read_view_as_drawn();
        B.col_enabled = 0;   // (the colour's own swaps aside: this checks the cracks alone)
        // THE TILES WORKED OUT (15U): the map's tiles, by frost.lua's rules. Tiletypes: 20 a floor, 21 a dirt
        // ramp, 22 open space, 23 a grass ramp; temperatures in the game's units (10000 freezing, 9 a 5 C)
        const uint32_t OFF_TEMP = 2048, OUTSIDE = 1u << 16;
        B.off_temperature = OFF_TEMP; B.outside_mask = OUTSIDE;
        B.crack_tt[20] = CRACK_FLOOR; B.crack_tt[21] = CRACK_RAMP | 1; B.crack_tt[22] = CRACK_OPEN; B.crack_tt[23] = 0;
        B.crack_band_c = 3; B.crack_cool_from = 10; B.crack_warm_from = 20; B.crack_recheck = 1000; B.crack_tables++;
        auto tile = [&](int x, int y, int z, uint16_t tt, int celsius, uint32_t add) {
            uint8_t* b = blk(x, y, z);
            ((uint16_t*)(b + OFF_TT))[tile_of(x, y)] = tt;
            ((uint32_t*)(b + OFF_DES))[tile_of(x, y)] = add;   // (the designation set whole: earlier checks changed some)
            ((uint16_t*)(b + OFF_TEMP))[tile_of(x, y)] = (uint16_t)(10000 + celsius * 9 / 5);
        };
        tile(2, 3, 2, 20, 30, 0);              // a floor, hot
        tile(4, 3, 2, 21, 30, 0);              // a dirt ramp
        tile(6, 3, 2, 22, 30, 0);              // open space, and below it
        tile(6, 3, 1, 20, -10, OUTSIDE);       //   a floor outdoors, cold
        tile(3, 3, 2, 20, 15, 0);              // mild: no crack
        tile(5, 3, 1, 20, 30, OUTSIDE);        // a floor below a plain floor: not seen
        tile(7, 3, 2, 20, 30, 1);              // water on it
        tile(1, 3, 2, 23, 30, 0);              // a grass ramp
        tile(1, 4, 1, 20, 30, SUBT);           // underground, not outdoors (with open space above it)
        tile(1, 4, 2, 22, 30, 0);
        auto answer = [&](int32_t base, int piece, uint8_t code, uint32_t gen, int32_t tex) {
            CrackAsk& a = B.crack_a[B.crack_a_head % CRACK_ASK_RING];
            a.base = base; a.piece = (uint8_t)piece; a.code = code; a.gen = gen; a.tex = tex; B.crack_a_head++; };
        cell(0, 0, 3, 3) = 1150; cell(1, 0, 5, 3) = 1160; cell(0, 0, 7, 3) = 1170; cell(0, 0, 1, 3) = 1180; cell(1, 0, 1, 4) = 1190;
        g_cflag[1100] = CF_TABLE;                                        // a floor, as the game drew it (a table copy)
        g_link[21200] = 1200; g_cflag[21200] = CF_SCREEN; g_cflag[1200] = CF_COPY_FOR; g_link[1200] = 21200;   // a ramp, its copy in the look in
        cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200; cell(1, 0, 6, 3) = 1300; cell(0, 0, 6, 3) = 0;
        B.crack_on = 1; B.crack_gen = 5; B.crack_gen_prev = 0xffffffffu; B.crack_depth = 2;
        const uint32_t q0 = B.crack_q_head;
        place_cracks();
        CHECK(B.crack_q_head - q0 == 3, "copies asked for: %u", B.crack_q_head - q0);
        bool seen_floor = false, seen_ramp = false, seen_below = false;
        for (uint32_t k = q0; k < B.crack_q_head; k++) {
            const CrackAsk& q = B.crack_q[k % CRACK_ASK_RING];
            if (q.base == 1100 && q.piece == (3 & 3) * 4 + (2 & 3) && q.code == CRACK_FLOOR && q.gen == 5) seen_floor = true;
            if (q.base == 1200 && q.piece == (3 & 3) * 4 + (4 & 3) && q.code == (CRACK_RAMP | 1)) seen_ramp = true;   // (the ramp's own picture)
            if (q.base == 1300 && q.piece == (3 & 3) * 4 + (6 & 3) && q.code == CRACK_FLOOR) seen_below = true;
        }
        CHECK(seen_floor && seen_ramp && seen_below, "questions wrong: floor %d, ramp %d, below %d", seen_floor, seen_ramp, seen_below);
        CHECK(B.crack_blocks_worked > 0 && B.crack_cells_last == 3, "tiles worked out: %lld blocks, %u cells", (long long)B.crack_blocks_worked, B.crack_cells_last);
        CHECK(cell(0, 0, 2, 3) == 1100 && B.crack_waiting_last == 3, "a cell changed before its copy came: %d (waiting %u)", cell(0, 0, 2, 3), B.crack_waiting_last);
        B.crack_q_tail = B.crack_q_head;
        place_cracks();
        CHECK(B.crack_q_head == B.crack_q_tail, "asked again while waiting");
        answer(1100, 14, CRACK_FLOOR, 5, 50000); answer(1200, 12, CRACK_RAMP | 1, 5, 50001); answer(1300, 14, CRACK_FLOOR, 5, -1);
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == 50000 && cell(0, 0, 4, 3) == 50001 && cell(1, 0, 6, 3) == 1300,
              "copies not placed: %d %d %d", cell(0, 0, 2, 3), cell(0, 0, 4, 3), cell(1, 0, 6, 3));
        CHECK(B.crack_written_last == 2 && B.crack_waiting_last == 0, "counts: written %u, waiting %u", B.crack_written_last, B.crack_waiting_last);
        CHECK(g_link[50000] == 1100 && (g_cflag[50000] & CF_SCREEN) && g_crack_mark[50001], "a copy not told to THE COLOUR");
        place_cracks();   // (a cell the game didn't redraw: its copy again, by its base)
        CHECK(cell(0, 0, 2, 3) == 50000 && cell(0, 0, 4, 3) == 50001, "a copy left in place lost: %d %d", cell(0, 0, 2, 3), cell(0, 0, 4, 3));
        // the look changes: the look before's held until the new copies come
        B.crack_gen = 6; B.crack_gen_prev = 5;
        const uint32_t q1 = B.crack_q_head;
        cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200;   // (the game drew them again)
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == 50000 && cell(0, 0, 4, 3) == 50001 && B.crack_held_last == 2,
              "the look before's not held: %d %d (held %u)", cell(0, 0, 2, 3), cell(0, 0, 4, 3), B.crack_held_last);
        CHECK(B.crack_q_head - q1 == 3, "the new look's not asked for: %u", B.crack_q_head - q1);
        B.crack_q_tail = B.crack_q_head;
        answer(1100, 14, CRACK_FLOOR, 6, 50010); answer(1200, 12, CRACK_RAMP | 1, 6, 50011);
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == 50010 && cell(0, 0, 4, 3) == 50011, "the new look's not placed: %d %d", cell(0, 0, 2, 3), cell(0, 0, 4, 3));
        // THE NEXT LOOK AHEAD (15T): the next look's copies asked for while this one shows, counted until
        // made, and shown at once when it does; and asked for with the cracks off (a look that doesn't crack
        // changing to one that does), the cells left as they are
        {
            B.crack_gen_next = 7;
            const uint32_t qa = B.crack_q_head;
            place_cracks();
            CHECK(B.crack_q_head - qa == 3 && B.crack_next_missing_last == 3 && B.crack_next_walked == 7,
                  "the next look's not asked for: %u asked, %u missing", B.crack_q_head - qa, B.crack_next_missing_last);
            CHECK(cell(0, 0, 2, 3) == 50010, "this look's copy lost while the next's asked for");
            B.crack_q_tail = B.crack_q_head;
            answer(1100, 14, CRACK_FLOOR, 7, 50030); answer(1200, 12, CRACK_RAMP | 1, 7, 50031); answer(1300, 14, CRACK_FLOOR, 7, -1);
            place_cracks();
            CHECK(B.crack_next_missing_last == 0 && B.crack_q_head == B.crack_q_tail, "the next look's still missing: %u", B.crack_next_missing_last);
            B.crack_gen = 7; B.crack_gen_prev = 6; B.crack_gen_next = 0xffffffffu;
            cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200;
            place_cracks();
            CHECK(cell(0, 0, 2, 3) == 50030 && cell(0, 0, 4, 3) == 50031 && B.crack_q_head == B.crack_q_tail,
                  "the next look's not shown at once: %d %d", cell(0, 0, 2, 3), cell(0, 0, 4, 3));
            B.crack_on = 0; B.crack_gen_next = 8;
            place_cracks(); place_cracks(); place_cracks();   // (the cracks stopped: ground back, then asking only)
            cell(0, 0, 2, 3) = 1100;
            const uint32_t qb = B.crack_q_head;
            place_cracks();
            CHECK(cell(0, 0, 2, 3) == 1100 && B.crack_next_missing_last == 3 && B.crack_q_head - qb <= 3 && B.crack_q_head > qa,
                  "asking ahead with the cracks off: cell %d, missing %u", cell(0, 0, 2, 3), B.crack_next_missing_last);
            B.crack_q_tail = B.crack_q_head;
            B.crack_on = 1; B.crack_gen = 6; B.crack_gen_prev = 0xffffffffu; B.crack_gen_next = 0xffffffffu;
            cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200;
            place_cracks();
            CHECK(cell(0, 0, 2, 3) == 50010 && cell(0, 0, 4, 3) == 50011, "back to look 6: %d %d", cell(0, 0, 2, 3), cell(0, 0, 4, 3));
        }
        // THE RAMP HELD (15S): the game gives the ramp a new number (and the colour its copy): its cracked
        // copy kept while the new number's is asked for; then the new one; and one that turns out not to
        // crack shows the ground as drawn
        g_link[21201] = 1201; g_cflag[21201] = CF_SCREEN; g_cflag[1201] = CF_COPY_FOR; g_link[1201] = 21201;
        cell(0, 0, 4, 3) = 21201;
        const uint32_t q3 = B.crack_q_head;
        place_cracks();
        CHECK(cell(0, 0, 4, 3) == 50011 && B.crack_ramp_held_last == 1 && B.crack_q_head - q3 == 1,
              "the ramp not held under its new number: %d (held %u, asked %u)", cell(0, 0, 4, 3), B.crack_ramp_held_last, B.crack_q_head - q3);
        B.crack_q_tail = B.crack_q_head;
        answer(1201, 12, CRACK_RAMP | 1, 6, 50012);
        cell(0, 0, 4, 3) = 21201;   // (the game draws it again: until then the held copy counts as the old number's)
        place_cracks();
        CHECK(cell(0, 0, 4, 3) == 50012 && B.crack_ramp_held_last == 0, "the ramp's new copy not placed: %d", cell(0, 0, 4, 3));
        g_link[21202] = 1202; g_cflag[21202] = CF_SCREEN; g_cflag[1202] = CF_COPY_FOR; g_link[1202] = 21202;
        cell(0, 0, 4, 3) = 21202;
        place_cracks();
        B.crack_q_tail = B.crack_q_head;
        answer(1202, 12, CRACK_RAMP | 1, 6, -1);
        cell(0, 0, 4, 3) = 21202;
        place_cracks();
        CHECK(cell(0, 0, 4, 3) == 21202, "a ramp that doesn't crack held: %d", cell(0, 0, 4, 3));
        // THE TILES WORKED OUT again: the floor turns mild, and once due (crack_recheck) it's left as drawn
        {
            tile(2, 3, 2, 20, 15, 0);
            cell(0, 0, 2, 3) = 1100;
            place_cracks();
            CHECK(cell(0, 0, 2, 3) == 50010, "rechecked before it was due");
            B.crack_recheck = 1;
            cell(0, 0, 2, 3) = 1100;
            place_cracks(); place_cracks();
            CHECK(cell(0, 0, 2, 3) == 1100, "a tile turned mild still cracked: %d", cell(0, 0, 2, 3));
            tile(2, 3, 2, 20, 30, 0);
            B.crack_recheck = 1000;
            B.crack_tables++;   // (the settings changed: every tile worked out again at once)
            place_cracks();
            CHECK(cell(0, 0, 2, 3) == 50010, "not worked out again when the settings changed: %d", cell(0, 0, 2, 3));
            const int32_t sx = xcb;
            xcb = 3; place_cracks();   // (another map's size: all let go, worked out afresh for the cells drawn)
            CHECK(crack_codes(2, 3, 2, false) && g_crack_bxn == 3, "another map's size kept the tiles");
            xcb = sx; place_cracks();
        }
        // the cracks stop: the ground back, two frames
        B.crack_on = 0;
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == 1100 && cell(0, 0, 4, 3) == 21202, "cracks left after they stopped: %d %d", cell(0, 0, 2, 3), cell(0, 0, 4, 3));
        place_cracks(); place_cracks();
        // THE COLOUR's tables emptied (a new start of the colour): the cracked copies told again
        B.col_reset++; dfhooks_prerender();
        CHECK(g_link[50010] == 1100 && (g_cflag[50010] & CF_SCREEN) && g_crack_mark[50010], "a copy not told again after the colour's reset");
        // a new world: all forgotten, and not told again
        B.crack_world++; B.col_reset++; dfhooks_prerender();
        CHECK(!g_crack_mark[50010] && g_link[50010] == 0 && !(g_cflag[50010] & CF_SCREEN), "a copy kept into a new world");
        printf("cracks in place: %.1f us a frame\n", B.crack_us_total / B.crack_frames);
        B.crack_on = 0; place_cracks(); place_cracks();
        B.col_enabled = 1;
        for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
    }
    // ---- a view of a full screen, timed ----
    printf("questions answered %d; held %lld; waited %lld; swapped %lld; put back %lld; %.2f us a frame (worst %.2f)\n",
           answered, (long long)B.col_held, (long long)B.col_waiting, (long long)B.col_swapped, (long long)B.col_put_back,
           B.col_us_total / B.col_frames, B.col_us_max);
    printf(fails ? "%d FAILED\n" : "all colour checks pass\n", fails);
    return fails != 0;
}
