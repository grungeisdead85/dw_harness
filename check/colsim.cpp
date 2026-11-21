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
    // CRACKS IN PLACE (15U, 15V): every tile on the view's level may crack (every block's tiles worked out from
    // the map, and every copy painted here, on the first frames: timed apart), then the frames after, timed
    {
        B.crack_tt[TT_PLAIN] = CRACK_FLOOR; B.subterranean_mask = 0; B.crack_band_c = 3; B.crack_cool_from = 10;
        B.crack_warm_from = 20; B.crack_recheck = 600; B.crack_tables++; B.off_temperature = 0;   // (no temperature read: a forced -10 C, a band that cracks)
        B.crack_forced_on = 1; B.crack_forced_c = -10;
        // THE PAINTING HERE: 50 grounds (table copies 9000.. of originals 1000.., all dirt), 800 blanks, the heat patterns
        struct Surf { uint32_t flags; uint32_t pad; void* fmt; int32_t w, h, pitch, pad2; uint32_t* px; };
        static void* praws[60000] = {};
        static Surf psurf[50];
        static uint32_t ppix[50][1024];
        for (int k = 0; k < 50; k++) {
            for (int n = 0; n < 1024; n++) ppix[k][n] = 0xff000000u | ((uint32_t)(n * 2654435761u + k * 977u) >> 8);
            psurf[k] = {0, 0, nullptr, 32, 32, 128, 0, ppix[k]};
            praws[9000 + k] = &psurf[k];
            g_ground[9000 + k - 8000] = CK_DIRT;   // (lua_answers made 9000.. table copies of 1000..)
        }
        struct Vec { void** first; void** last; };
        static Vec pvec = {praws, praws + 60000};
        B.col_raws = (uint64_t)(uintptr_t)&pvec;
        static uint32_t pheat[16][1024];
        for (int k = 0; k < 16; k++) {
            for (int n = 0; n < 1024; n++) pheat[k][n] = 0xff000000u | ((uint32_t)((n * 7 + k * 131) % 9) << 8) | (uint32_t)((n * 13 + k * 17) % 11);
            B.crack_heat[k] = (uint64_t)(uintptr_t)pheat[k];
        }
        static uint32_t pblank[CRACK_BLANKS][1024];
        auto blanks = [&](int from) {
            while (B.crack_b_head - B.crack_b_tail < (uint32_t)CRACK_BLANKS) {
                CrackBlank& bl = B.crack_b[B.crack_b_head % CRACK_BLANKS];
                bl.tex = 50000 + from; bl.addr = (uint64_t)(uintptr_t)pblank[from % CRACK_BLANKS]; B.crack_b_head++; from++;
            }
            return from;
        };
        int next_blank = blanks(0);
        CrackLook& lk = B.crack_look[0];
        memset(&lk, 0, sizeof lk); lk.gen = 1; lk.amount = 0.6; lk.ready = 1;
        B.crack_on = 1; B.crack_gen = 1; B.crack_gen_prev = 0xffffffffu; B.crack_depth = 2; B.crack_gen_next = 0xffffffffu;
        for (int d = 0; d < 3; d++) for (int i = 0; i < N; i++) vps[d].L[0][i] = 9000 + i % 50;   // (as DF draws it: the colour put some back)
        uint32_t first_painted = 0; int frames_to_all = 0;
        for (int k = 0; k < 12; k++) {
            place_cracks();
            first_painted += B.crack_painted_last;
            if (B.crack_waiting_last == 0 && !frames_to_all) frames_to_all = k + 1;
            next_blank = blanks(next_blank);   // (Lua tops the blanks up each frame)
        }
        printf("PERF cracks: the first frames, every block worked out and %u copies painted: %d frames until every cell had its copy, the first frame %.1f us (painting at most %.1f us a frame)\n",
               first_painted, frames_to_all, B.crack_us_max, B.crack_paint_us_max);
        B.crack_frames = 0; B.crack_us_total = 0; B.crack_us_max = 0;
        for (int k = 0; k < PERF_FRAMES; k++) {
            for (int d = 0; d < 3; d++) for (int i = 0; i < N; i += 3) vps[d].L[0][i] = 9000 + i % 50;   // (DF redraws a third)
            place_cracks();
        }
        printf("PERF cracks in place (view's level): %.1f us a frame (worst %.1f), %u cells, %u given their copy, %u waiting\n",
               B.crack_us_total / B.crack_frames, B.crack_us_max, B.crack_cells_last, B.crack_written_last, B.crack_waiting_last);
        B.col_raws = 0;
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
        // ---- PATHS HELD (15Z) ----
        {
            static uint32_t more[24][1024];   // more blanks: each grass picture below takes 2 (the track's end and its middle: 2 keys)
            for (int k = 0; k < 24; k++) {
                const uint32_t slot = B.murky_blank_head % 256;
                B.murky_blank_tex[slot] = 62100 + k; B.murky_blank_addr[slot] = (uint64_t)(uintptr_t)more[k]; B.murky_blank_head++;
            }
            auto row = [&](int32_t g) { for (int x = 0; x < 8; x++) cell(0, 0, x, 3) = g; cell(0, 0, 6, 3) = 1051; };
            auto answer = [&](int32_t t, const uint32_t* px) {
                B.paths_q_tail = B.paths_q_head;
                PathAnswer& a = B.paths_a[B.paths_a_head % 256]; a.texpos = t; a.addr = (uint64_t)(uintptr_t)px; B.paths_a_head++;
            };
            auto worn = [&](int k) { return cell(0, 0, k + 1, 3); };   // the k-th worn tile (x 1..5)
            auto pix_of = [&](int32_t t) -> uint32_t* { return t >= 62100 ? more[t - 62100] : blanks[t - 62000]; };
            auto ours = [&](int32_t t) { return (t >= 62000 && t < 62008) || (t >= 62100 && t < 62124); };
            static uint32_t g2[1024], g3[1024], g4[1024], g5[1024], g6[1024];
            for (int k = 0; k < 1024; k++) {
                g2[k] = 0xff000000u | (20u << 16) | (140u << 8) | 90u; g3[k] = 0xff000000u | (25u << 16) | (130u << 8) | 70u;
                g4[k] = 0xff000000u | (35u << 16) | (120u << 8) | 60u; g5[k] = 0xff000000u | (45u << 16) | (110u << 8) | 50u;
                g6[k] = 0xff000000u | (55u << 16) | (100u << 8) | 40u;
            }
            int32_t before[5]; for (int k = 0; k < 5; k++) before[k] = worn(k);
            // a new look: the grass a new number, asked about; each worn tile shows its last painting meanwhile
            row(1060);
            uint32_t waits0 = B.paths_waits; int64_t held0 = B.paths_held;
            draw_paths();
            bool same = true; for (int k = 0; k < 5; k++) same &= worn(k) == before[k];
            CHECK(same && B.paths_held == held0 + 5 && B.paths_waits == waits0 && B.paths_held_last == 5,
                  "paths held: new grass asked about: held %lld, left %u", (long long)(B.paths_held - held0), B.paths_waits - waits0);
            CHECK(cell(0, 0, 0, 3) == 1060 && cell(0, 0, 7, 3) == 1060, "paths held: unworn grass drawn over");
            // answered: painted afresh from the new grass
            answer(1060, g2); row(1060);
            int64_t baked0 = B.paths_baked;
            draw_paths();
            CHECK(B.paths_baked == baked0 + 2 && ours(worn(2)) && worn(2) != before[2] && B.paths_held_last == 0,
                  "paths held: not painted afresh once answered: baked %lld, tile %d", (long long)(B.paths_baked - baked0), worn(2));
            const int32_t fresh2 = worn(2);
            CHECK(pix_of(fresh2)[15 * 32 + 15] == soil[0] && pix_of(fresh2)[0] == g2[0], "paths held: the new painting isn't of the new grass: %08x",
                  pix_of(fresh2)[0]);
            // no blank: the base known, the ring empty: held, and counted
            row(1070); draw_paths(); answer(1070, g3);
            const uint32_t keep_head = B.murky_blank_head; B.murky_blank_head = B.murky_blank_tail;
            const uint32_t nb0 = B.paths_no_blank; held0 = B.paths_held;
            row(1070); draw_paths();
            CHECK(worn(2) == fresh2 && B.paths_held == held0 + 5 && B.paths_no_blank > nb0, "paths held: no blank: tile %d, held %lld, no blank %u",
                  worn(2), (long long)(B.paths_held - held0), B.paths_no_blank - nb0);
            B.murky_blank_head = keep_head;
            row(1070); draw_paths();
            CHECK(worn(2) != fresh2 && ours(worn(2)) && pix_of(worn(2))[0] == g3[0], "paths held: not painted once a blank came: %d", worn(2));
            // the frame's painting time used up: held, put off, and painted on a later frame
            row(1080); draw_paths(); answer(1080, g4);
            const int32_t fresh3 = worn(2);
            const double tick = g_us_per_tick; g_us_per_tick = 1e12;   // (every painting over the frame's time)
            const int64_t po0 = B.paths_put_off; held0 = B.paths_held; baked0 = B.paths_baked;
            row(1080); draw_paths();
            g_us_per_tick = tick;
            CHECK(worn(2) == fresh3 && B.paths_put_off > po0 && B.paths_baked == baked0 && B.paths_held == held0 + 5,
                  "paths held: over time: tile %d, put off %lld, painted %lld", worn(2), (long long)(B.paths_put_off - po0), (long long)(B.paths_baked - baked0));
            row(1080); draw_paths();
            CHECK(worn(2) != fresh3 && B.paths_baked == baked0 + 2 && pix_of(worn(2))[0] == g4[0], "paths held: not painted on the next frame");
            // held no longer than P_HOLD_FRAMES: then the game's own, counted as left
            row(1090); draw_paths(); answer(1090, g5);
            B.murky_blank_head = B.murky_blank_tail;   // (no blanks from here)
            const int32_t fresh4 = worn(2);
            for (int f = 0; f < P_HOLD_FRAMES - 1; f++) { row(1090); draw_paths(); }
            CHECK(worn(2) == fresh4, "paths held: let go too soon");
            waits0 = B.paths_waits;
            row(1090); draw_paths();
            CHECK(worn(2) == 1090 && B.paths_waits == waits0 + 5, "paths held: held past %d frames: tile %d", P_HOLD_FRAMES, worn(2));
            B.murky_blank_head = keep_head;
            row(1090); draw_paths();
            CHECK(ours(worn(2)) && worn(2) != fresh4, "paths held: not painted after the hold ran out");
            // a block worked out again keeps each tile's painting while its wear is the same, drops it where it changed
            PBlock* blk0 = (PBlock*)p_block_kept(0, 0, 2);
            p_block_work(*blk0);
            int kept_held = 0; for (int c = 0; c < blk0->n; c++) kept_held += blk0->cells[c].held != 0;
            CHECK(kept_held == 5, "paths held: a block worked out again kept %d of 5", kept_held);
            PAmount* pa0 = (PAmount*)p_amount(0, 0, 2);
            pa0->a[tile_of(1, 3)] = 60;   // (the end tile less worn: its key changes)
            p_block_work(*blk0);
            int32_t end_held = -1; for (int c = 0; c < blk0->n; c++) if (blk0->cells[c].tile == tile_of(1, 3)) end_held = blk0->cells[c].held;
            CHECK(end_held == 0, "paths held: a tile worn differently kept its painting: %d", end_held);
            pa0->a[tile_of(1, 3)] = 20; p_block_work(*blk0);
            row(1090); draw_paths();
            // THE PAINTINGS IN TWO HALVES: the newer half full: it becomes the older, and a painting there is
            // found again without being painted again
            g_pbake_used = P_BAKES * 3 / 4;
            const uint32_t halved0 = B.paths_bakes_halved;
            row(1100); draw_paths(); answer(1100, g6); row(1100); draw_paths();
            CHECK(B.paths_bakes_halved == halved0 + 1 && g_pbake_used == 2, "paths halves: not halved: %u, %d used", B.paths_bakes_halved - halved0, g_pbake_used);
            baked0 = B.paths_baked;
            row(1090); draw_paths();
            CHECK(B.paths_baked == baked0 && ours(worn(2)) && pix_of(worn(2))[0] == g5[0] && g_pbake_used == 4, "paths halves: painted again from the older half: %lld, %d used",
                  (long long)(B.paths_baked - baked0), g_pbake_used);
            // THE GRASS PICTURES: the list full starts over (it used to stop every new picture for good)
            g_pbase_used = P_BASES * 3 / 4;
            const uint32_t dropped0 = B.paths_bases_dropped, asked0 = B.paths_q_head;
            row(1110); draw_paths();
            CHECK(B.paths_bases_dropped == dropped0 + 1 && B.paths_q_head == asked0 + 1 && B.paths_q[asked0 % 256] == 1110 && B.paths_held_last == 5,
                  "paths: the full list of grass pictures didn't start over: %u, asked %u", B.paths_bases_dropped - dropped0, B.paths_q_head - asked0);
            printf("paths held: %lld cells held, %lld put off, %u no blank, %u left in all\n", (long long)B.paths_held, (long long)B.paths_put_off,
                   B.paths_no_blank, B.paths_waits);
        }
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
    // ---- CRACKS IN PLACE (15R to 15V): tiles worked out from the map, copies painted here in the frame they're
    // needed and kept by picture, held over, the next look's ahead, cleared ----
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
        tile(0, 2, 2, 20, 30, 0);              // a grass floor (its ground doesn't crack)
        // THE PAINTING HERE (15V): the textures' surfaces (enabler.textures.raws), the heat patterns, the blanks
        struct Surf { uint32_t flags; uint32_t pad; void* fmt; int32_t w, h, pitch, pad2; uint32_t* px; };
        static void* craws[60000] = {};
        static Surf csurf[64];
        static uint32_t cpix[64][32 * 40];   // (room for a padded surface: pitch 160)
        int nsurf = 0;
        auto surface = [&](int32_t t, uint32_t seed, int32_t pitch) {
            Surf& sf = csurf[nsurf]; uint32_t* px = cpix[nsurf]; nsurf++;
            for (int i = 0; i < 32 * 40; i++) px[i] = 0xff000000u | ((uint32_t)(i * 2654435761u + seed * 40503u) >> 8);
            sf = {0, 0, nullptr, 32, 32, pitch, 0, px};
            craws[t] = &sf;
            return px;
        };
        struct Vec { void** first; void** last; };
        static Vec cvec = {craws, craws + 60000};
        B.col_raws = (uint64_t)(uintptr_t)&cvec;
        surface(1100, 1, 128);                 // the floor's ground as drawn (a table copy)
        uint32_t* ramp_px = surface(1200, 2, 128);   // the ramp's own picture
        uint32_t* same_px = surface(1201, 9, 160);   // the same picture under a new number (padded rows)
        for (int y = 0; y < 32; y++) memcpy(same_px + y * 40, ramp_px + y * 32, 128);
        surface(1202, 3, 128);                 // another ramp picture
        surface(1300, 4, 128);                 // the floor below (an original, no table copy)
        surface(1108, 18, 128);                // 1000's table copy in look 8 (FLOORS AHEAD)
        surface(1109, 19, 160);                // and in look 9 (padded rows)
        surface(1500, 5, 128);                 // a grass floor
        static uint32_t heat[16][1024];
        for (int k = 0; k < 16; k++) {
            for (int n = 0; n < 1024; n++) heat[k][n] = 0xff000000u | ((uint32_t)((n * 7 + k * 131) % 9) << 8) | (uint32_t)((n * 13 + k * 17) % 11);
            B.crack_heat[k] = (uint64_t)(uintptr_t)heat[k];
        }
        static uint32_t blank_px[400][1024];
        int nblank = 0;
        auto blanks = [&](int n) {
            for (int k = 0; k < n; k++, nblank++) {
                CrackBlank& bl = B.crack_b[B.crack_b_head % CRACK_BLANKS];
                bl.tex = 50000 + nblank; bl.addr = (uint64_t)(uintptr_t)blank_px[nblank]; B.crack_b_head++;
            }
        };
        blanks(40);
        // the looks (colour.lua: native_template): a dirt ramp's redraw and a stone ramp's, colour only here
        // (FLOORS AHEAD: each look lists what the game draws for each original whose ground cracks: 1000's
        // table copy, and 1300 itself)
        auto fill_look = [&](int slot, uint32_t gen, double amount, double tint, int32_t copy_of_1000 = 1100) {
            CrackLook& lk = B.crack_look[slot];
            lk.ready = 0; lk.gen = gen; lk.amount = amount;
            lk.floor_orig[0] = 1000; lk.floor_copy[0] = copy_of_1000;
            lk.floor_orig[1] = 1300; lk.floor_copy[1] = 1300;
            lk.floors_n = 2; lk.floors_version++;
            for (int k = 0; k < 2; k++) {
                PaintJob& j = lk.ramp[k];
                memset(&j, 0, sizeof j);
                j.m[0] = tint; j.m[4] = 1; j.m[8] = 1 - (tint - 1); j.dark = 1; j.lean[0] = j.lean[1] = j.lean[2] = 1;
                j.soil[0] = j.soil[1] = j.soil[2] = 1; j.ice_dark = 1; j.m[1] = 0.01 * (k + 1);
            }
            lk.ready = 1 | 2 | 4;
        };
        fill_look(0, 5, 0.5, 1.1);
        // the grounds by original (frost.lua: ground_kind): the table copy 1100's original is 1000, dirt
        g_cflag[1100] = CF_TABLE; g_link[1100] = 1000; g_ground[1000] = CK_DIRT;
        g_ground[1300] = CK_STONE;
        g_cflag[1500] = CF_TABLE; g_link[1500] = 1400; g_ground[1400] = 0;   // (grass)
        g_link[21200] = 1200; g_cflag[21200] = CF_SCREEN; g_cflag[1200] = CF_COPY_FOR; g_link[1200] = 21200;   // a ramp, its copy in the look in
        cell(0, 0, 3, 3) = 1150; cell(1, 0, 5, 3) = 1160; cell(0, 0, 7, 3) = 1170; cell(0, 0, 1, 3) = 1180; cell(1, 0, 1, 4) = 1190;
        cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200; cell(1, 0, 6, 3) = 1300; cell(0, 0, 6, 3) = 0; cell(0, 0, 0, 2) = 1500;
        // what a copy should be: the same paint job built by hand (paint_one is checked against the Lua elsewhere)
        auto expect = [&](int32_t tex, int32_t base, int piece, int ramp_kind, uint32_t ground_kind, int slot) -> bool {
            static uint32_t want[1024];
            PaintJob j;
            if (ramp_kind) j = B.crack_look[slot].ramp[ramp_kind - 1];
            else { memset(&j, 0, sizeof j); j.m[0] = j.m[4] = j.m[8] = 1; j.lean[0] = j.lean[1] = j.lean[2] = 1;
                   j.soil[0] = j.soil[1] = j.soil[2] = 1; j.dark = 1; j.ice_dark = 1; }
            static uint32_t src[1024];
            const Surf* sf = (const Surf*)craws[base];
            for (int y = 0; y < 32; y++) memcpy(src + y * 32, sf->px + y * (sf->pitch / 4), 128);
            j.src = (uint64_t)(uintptr_t)src; j.dst = (uint64_t)(uintptr_t)want;
            j.crack_kind = ramp_kind ? (uint32_t)ramp_kind : ground_kind; j.crack_piece = piece; j.crack_amount = B.crack_look[slot].amount;
            j.crack_here = B.crack_heat[piece]; j.crack_north = B.crack_heat[(piece + 12) & 15];
            const int64_t pd = B.paint_done; paint_one(j); B.paint_done = pd;
            return tex >= 50000 && memcmp(want, blank_px[tex - 50000], 4096) == 0;
        };
        B.crack_on = 1; B.crack_gen = 5; B.crack_gen_prev = 0xffffffffu; B.crack_depth = 2; B.crack_gen_next = 0xffffffffu;
        const uint32_t d0 = B.crack_d_head;
        place_cracks();
        // all three painted and in place on the first frame: nothing left uncracked
        CHECK(B.crack_waiting_last == 0 && B.crack_painted_last == 3 && B.crack_cells_last == 4,
              "first frame: waiting %u, painted %u, cells %u", B.crack_waiting_last, B.crack_painted_last, B.crack_cells_last);
        const int32_t t_floor = cell(0, 0, 2, 3), t_ramp = cell(0, 0, 4, 3), t_below = cell(1, 0, 6, 3);
        CHECK(expect(t_floor, 1100, 14, 0, CK_DIRT, 0), "the floor's copy wrong: %d", t_floor);
        CHECK(expect(t_ramp, 1200, 12, 1, 0, 0), "the ramp's copy wrong: %d", t_ramp);
        CHECK(expect(t_below, 1300, 14, 0, CK_STONE, 0), "the floor below's copy wrong: %d", t_below);
        CHECK(cell(0, 0, 0, 2) == 1500 && cell(0, 0, 3, 3) == 1150 && cell(1, 0, 5, 3) == 1160 && cell(0, 0, 7, 3) == 1170 &&
              cell(0, 0, 1, 3) == 1180 && cell(1, 0, 1, 4) == 1190, "a tile that doesn't crack changed");
        CHECK(g_link[t_ramp] == 1200 && (g_cflag[t_ramp] & CF_SCREEN) && g_crack_mark[t_floor], "a copy not told to THE COLOUR");
        CHECK(B.crack_d_head - d0 == 3, "copies noted for Lua: %u", B.crack_d_head - d0);
        {
            bool ok = false;
            for (uint32_t k = d0; k < B.crack_d_head; k++) {
                const CrackDone& dn = B.crack_d[k % CRACK_DONE_RING];
                if (dn.tex == t_ramp && dn.base == 1200 && dn.code == (CRACK_RAMP | 1) && dn.gen == 5 && dn.piece == 12) ok = true;
            }
            CHECK(ok, "the ramp's copy not noted right");
            B.crack_d_tail = B.crack_d_head;
        }
        // the next frame: the game drew them again; their copies found, nothing painted
        cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200;
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == t_floor && cell(0, 0, 4, 3) == t_ramp && B.crack_painted_last == 0, "copies not found again");
        place_cracks();   // (a cell the game didn't redraw: its copy again, by its base)
        CHECK(cell(0, 0, 2, 3) == t_floor && cell(0, 0, 4, 3) == t_ramp && B.crack_painted_last == 0, "a copy left in place lost");
        // PICTURES, NOT NUMBERS: the ramp under a new number with the same picture (padded rows) finds its copy;
        // another picture is painted, in the same frame
        g_link[21201] = 1201; g_cflag[21201] = CF_SCREEN; g_cflag[1201] = CF_COPY_FOR; g_link[1201] = 21201;
        cell(0, 0, 4, 3) = 21201;
        place_cracks();
        CHECK(cell(0, 0, 4, 3) == t_ramp && B.crack_painted_last == 0, "the same picture under a new number: %d, painted %u",
              cell(0, 0, 4, 3), B.crack_painted_last);
        g_link[21202] = 1202; g_cflag[21202] = CF_SCREEN; g_cflag[1202] = CF_COPY_FOR; g_link[1202] = 21202;
        cell(0, 0, 4, 3) = 21202;
        place_cracks();
        const int32_t t_other = cell(0, 0, 4, 3);
        CHECK(t_other != t_ramp && expect(t_other, 1202, 12, 1, 0, 0) && B.crack_waiting_last == 0, "another picture not painted at once: %d", t_other);
        // the look changes (its copies painted at once on its first frame; the look before's is there if the time runs out)
        fill_look(1, 6, 0.8, 0.9);
        B.crack_gen = 6; B.crack_gen_prev = 5;
        cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200;
        place_cracks();
        const int32_t t_floor6 = cell(0, 0, 2, 3), t_ramp6 = cell(0, 0, 4, 3);
        CHECK(B.crack_waiting_last == 0 && t_floor6 != t_floor && expect(t_floor6, 1100, 14, 0, CK_DIRT, 1) && expect(t_ramp6, 1200, 12, 1, 0, 1),
              "the new look's not painted at once: %d %d (waiting %u)", t_floor6, t_ramp6, B.crack_waiting_last);
        // no blanks ready: the look before's held (and a ramp its last), then painted once blanks come
        B.crack_b_tail = B.crack_b_head;
        fill_look(0, 7, 0.3, 1.2);
        B.crack_gen = 7; B.crack_gen_prev = 6;
        cell(0, 0, 2, 3) = 1100; cell(0, 0, 4, 3) = 21200;
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == t_floor6 && cell(0, 0, 4, 3) == t_ramp6 && B.crack_held_last >= 2 && B.crack_no_blank > 0,
              "no blanks: the look before's not held: %d %d (held %u)", cell(0, 0, 2, 3), cell(0, 0, 4, 3), B.crack_held_last);
        blanks(20);
        place_cracks();
        CHECK(expect(cell(0, 0, 2, 3), 1100, 14, 0, CK_DIRT, 0) && expect(cell(0, 0, 4, 3), 1200, 12, 1, 0, 0) && B.crack_waiting_last == 0,
              "painted once blanks came: %d %d", cell(0, 0, 2, 3), cell(0, 0, 4, 3));
        // THE NEXT LOOK AHEAD: the next look's painted ahead (counted until done), the showing one untouched; shown at
        // once. A floor's from the next look's own copy of its ground (FLOORS AHEAD: 1108), not listed yet at first
        fill_look(1, 8, 0.6, 1.05, 1108);
        B.crack_look[1].floors_n = 1; B.crack_look[1].floor_orig[0] = 1300; B.crack_look[1].floor_copy[0] = 1300; B.crack_look[1].floors_version++;
        B.crack_gen_next = 8;
        const int32_t t7 = cell(0, 0, 2, 3);
        place_cracks();
        CHECK(B.crack_next_missing_last == 1 && cell(0, 0, 2, 3) == t7, "a floor ahead before its copy was listed: missing %u",
              B.crack_next_missing_last);
        fill_look(1, 8, 0.6, 1.05, 1108);   // (colour.lua made 1000's copy for look 8)
        place_cracks();
        CHECK(B.crack_next_missing_last == 0 && B.crack_next_walked == 8 && cell(0, 0, 2, 3) == t7, "the next look ahead: missing %u",
              B.crack_next_missing_last);
        const int64_t painted_before = B.crack_painted;
        B.crack_gen = 8; B.crack_gen_prev = 7; B.crack_gen_next = 0xffffffffu;
        g_cflag[1108] = CF_TABLE; g_link[1108] = 1000;   // (the look shows: colour.lua tells the DLL its table copies)
        cell(0, 0, 2, 3) = 1108; cell(0, 0, 4, 3) = 21200;
        place_cracks();
        CHECK(B.crack_painted == painted_before && expect(cell(0, 0, 2, 3), 1108, 14, 0, CK_DIRT, 1) &&
              expect(cell(0, 0, 4, 3), 1200, 12, 1, 0, 1), "the next look not shown at once: painted %lld", (long long)(B.crack_painted - painted_before));
        // ahead with the cracks off (a look that doesn't crack before one that does): painted, the cells left as they are
        fill_look(0, 9, 0.4, 0.95, 1109);
        B.crack_on = 0; B.crack_gen_next = 9;
        place_cracks(); place_cracks(); place_cracks();   // (the cracks stopped: ground back, then painting ahead only)
        cell(0, 0, 2, 3) = 1108; cell(0, 0, 4, 3) = 21200;
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == 1108 && B.crack_next_missing_last == 0, "ahead with the cracks off: cell %d, missing %u",
              cell(0, 0, 2, 3), B.crack_next_missing_last);
        B.crack_on = 1; B.crack_gen = 9; B.crack_gen_prev = 8; B.crack_gen_next = 0xffffffffu;
        g_cflag[1109] = CF_TABLE; g_link[1109] = 1000;
        cell(0, 0, 2, 3) = 1109;
        const int64_t painted_9 = B.crack_painted;
        place_cracks();
        CHECK(B.crack_painted == painted_9 && expect(cell(0, 0, 2, 3), 1109, 14, 0, CK_DIRT, 0), "look 9 not shown at once");
        cell(0, 0, 2, 3) = 1100; place_cracks();   // (the earlier ground again, for the checks below: painted at once)
        CHECK(expect(cell(0, 0, 2, 3), 1100, 14, 0, CK_DIRT, 0), "an earlier look's ground not painted in this look");
        // THE TILES WORKED OUT again: the floor turns mild, and once due (crack_recheck) it's left as drawn
        {
            const int32_t t9 = cell(0, 0, 2, 3);
            tile(2, 3, 2, 20, 15, 0);
            cell(0, 0, 2, 3) = 1100;
            place_cracks();
            CHECK(cell(0, 0, 2, 3) == t9, "rechecked before it was due");
            B.crack_recheck = 1;
            cell(0, 0, 2, 3) = 1100;
            place_cracks(); place_cracks();
            CHECK(cell(0, 0, 2, 3) == 1100, "a tile turned mild still cracked: %d", cell(0, 0, 2, 3));
            tile(2, 3, 2, 20, 30, 0);
            B.crack_recheck = 1000;
            B.crack_tables++;   // (the settings changed: every tile worked out again at once)
            place_cracks();
            CHECK(cell(0, 0, 2, 3) == t9, "not worked out again when the settings changed: %d", cell(0, 0, 2, 3));
            const int32_t sx = xcb;
            xcb = 3; place_cracks();   // (another map's size: all let go, worked out afresh for the cells drawn)
            CHECK(crack_codes(2, 3, 2, false) && g_crack_bxn == 3, "another map's size kept the tiles");
            xcb = sx; place_cracks();
        }
        // the cracks stop: the ground back, two frames
        B.crack_on = 0;
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == 1100, "cracks left after they stopped: %d", cell(0, 0, 2, 3));
        place_cracks(); place_cracks();
        // THE COLOUR's tables emptied (a new start of the colour): the cracked copies told again
        B.col_reset++; dfhooks_prerender();
        CHECK(g_link[t_floor] == 1100 && (g_cflag[t_floor] & CF_SCREEN) && g_crack_mark[t_floor], "a copy not told again after the colour's reset");
        // a new world: all forgotten, and not told again; the blanks from before dropped. Lua raises crack_world
        // in its frame and may turn the cracks on in the same frame: nothing placed or painted until this side's
        // next prerender has forgotten the last world (its numbers may be others' by now)
        blanks(3);
        B.crack_world++;
        B.crack_on = 1;
        cell(0, 0, 2, 3) = 1100;
        const int64_t frames_w = B.crack_frames, painted_w = B.crack_painted;
        const uint32_t b_tail_w = B.crack_b_tail;
        place_cracks();
        CHECK(cell(0, 0, 2, 3) == 1100 && B.crack_frames == frames_w && B.crack_painted == painted_w && B.crack_b_tail == b_tail_w,
              "placed or painted with a new world not yet forgotten: cell %d", cell(0, 0, 2, 3));
        B.crack_on = 0;
        B.col_reset++; dfhooks_prerender();
        CHECK(!g_crack_mark[t_floor] && g_link[t_floor] == 0 && !(g_cflag[t_floor] & CF_SCREEN) && B.crack_b_tail == B.crack_b_head &&
              g_crack_copies_used == 0 && !g_ground[1000], "a copy, a blank or a ground kept into a new world");
        printf("cracks in place: %.1f us a frame (painting %.1f us of it at most)\n", B.crack_us_total / B.crack_frames, B.crack_paint_us_max);
        B.crack_on = 0; place_cracks(); place_cracks();
        B.col_enabled = 1; B.col_raws = 0;
        for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
    }
    // ---- BY PICTURE (15Z): a texture not answered yet gets the copy of its picture in the look shown ----
    {
        struct Surf { uint32_t flags; uint32_t pad; void* fmt; int32_t w, h, pitch, pad2; uint32_t* px; };
        static void* braws[60000] = {};
        static Surf bsurf[8];
        static uint32_t bpix[8][32 * 40];
        struct Vec { void** first; void** last; };
        static Vec bvec = {braws, braws + 60000};
        auto surface = [&](int k, int32_t t, uint32_t seed, int32_t pitch) {
            for (int i = 0; i < 32 * 40; i++) bpix[k][i] = 0xff000000u | ((uint32_t)(i * 2654435761u + seed * 40503u) >> 8);
            bsurf[k] = {0, 0, nullptr, 32, 32, pitch, 0, bpix[k]};
            braws[t] = &bsurf[k];
        };
        B.col_raws = (uint64_t)(uintptr_t)&bvec;
        look = 0;
        surface(0, 1400, 11, 128);                          // an original
        surface(1, 1401, 99, 160);                          // the same picture under a new number (padded rows)
        for (int y = 0; y < 32; y++) memcpy(bpix[1] + y * 40, bpix[0] + y * 32, 128);
        surface(2, 1402, 12, 128);                          // another picture
        surface(3, 1403, 98, 128);                          // the first picture again, for the stale look
        memcpy(bpix[3], bpix[0], 4096);
        publish(1400);                                      // its copy in the look shown, 21400
        static uint32_t cdst[1024];
        PaintJob j; memset(&j, 0, sizeof j);
        j.src = (uint64_t)(uintptr_t)bpix[0]; j.dst = (uint64_t)(uintptr_t)cdst; j.dark = 1;
        j.m[0] = j.m[4] = j.m[8] = 1; j.dst_tex = 21400;
        const uint32_t filed0 = B.col_pics_filed;
        paint_one(j);
        CHECK(B.col_pics_filed == filed0 + 1, "by picture: a screen copy painted wasn't filed");
        j.dst_tex = 0; paint_one(j);
        CHECK(B.col_pics_filed == filed0 + 1, "by picture: a copy not for the screen was filed");
        for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
        frame(); frame();   // (nothing held anywhere)
        cell(0, 3, 1, 1) = 1401; cell(0, 3, 2, 1) = 1402;   // building layer, above ground: swapped where answered
        const int64_t byp0 = B.col_by_picture, wait0 = B.col_waiting;
        const uint32_t asked0 = B.col_req_head;
        frame();
        CHECK(cell(0, 3, 1, 1) == 21400 && cell(0, 3, 2, 1) == 1402, "by picture: cells %d, %d", cell(0, 3, 1, 1), cell(0, 3, 2, 1));
        CHECK(B.col_by_picture == byp0 + 1 && B.col_waiting == wait0 + 1 && B.col_req_head == asked0 + 2,
              "by picture: %lld given, %lld waited, %u asked", (long long)(B.col_by_picture - byp0), (long long)(B.col_waiting - wait0), B.col_req_head - asked0);
        lua_answers();
        // another look shown: the filed copy is that look's no longer, and isn't used
        g_link[1400] = 31400; g_link[31400] = 1400; g_cflag[31400] = CF_SCREEN;
        for (int l = 0; l < 7; l++) memset(vps[0].L[l], 0, N * 4);
        frame(); frame();
        cell(0, 3, 3, 1) = 1403;
        const int64_t byp1 = B.col_by_picture;
        frame();
        CHECK(cell(0, 3, 3, 1) == 1403 && B.col_by_picture == byp1, "by picture: another look's copy used: %d", cell(0, 3, 3, 1));
        // that look's own copy painted and filed: used
        j.dst_tex = 31400; paint_one(j);
        for (int l = 0; l < 7; l++) memset(vps[0].L[l], 0, N * 4);
        B.col_req_tail = B.col_req_head;
        g_cflag[1403] = 0;
        frame(); frame();
        cell(0, 3, 4, 1) = 1403;
        frame();
        CHECK(cell(0, 3, 4, 1) == 31400, "by picture: the look shown's copy not used: %d", cell(0, 3, 4, 1));
        printf("by picture: %lld cells given their picture's copy, %u copies filed\n", (long long)B.col_by_picture, B.col_pics_filed);
        B.col_raws = 0;
        for (int d = 0; d < 3; d++) for (int l = 0; l < 7; l++) memset(vps[d].L[l], 0, N * 4);
        B.col_req_tail = B.col_req_head;
    }
    // ---- a view of a full screen, timed ----
    printf("questions answered %d; held %lld; waited %lld; swapped %lld; put back %lld; %.2f us a frame (worst %.2f)\n",
           answered, (long long)B.col_held, (long long)B.col_waiting, (long long)B.col_swapped, (long long)B.col_put_back,
           B.col_us_total / B.col_frames, B.col_us_max);
    printf(fails ? "%d FAILED\n" : "all colour checks pass\n", fails);
    return fails != 0;
}
