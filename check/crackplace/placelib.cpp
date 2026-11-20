// placelib.cpp: the DLL built as a shared library over a fake DF (as colsim.cpp's), for place.py to drive
// frost.lua's NATIVE CRACKS (the real Lua, through lupa) against the DLL's CRACKS IN PLACE (tests 15R to 15V),
// with the textures' surfaces (enabler.textures.raws), the heat patterns and blanks for THE PAINTING HERE.
#include <ctime>
#include <cstdio>
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
#include "../../detailed_weather_dll.cpp"

// map: 2 x 2 blocks, 3 levels; the view 8 x 6 at (0, 0, 2), and two levels below it
struct VP { uint32_t flag; int32_t dx, dy; int32_t pad; int32_t* L[7]; uint64_t* FF; int32_t* IF; };
static const int DX = 8, DY = 6, N = DX * DY;
static VP vps[3]; static VP* main_ptr = &vps[0]; static VP* lower[2] = {&vps[1], &vps[2]};
static int32_t wx = 0, wy = 0, wz = 2;
static void*** xs; static int32_t xcb = 2, ycb = 2, zcb = 3;

extern "C" {
uint64_t pl_block() { return (uint64_t)(uintptr_t)&B; }
void pl_init() {
    xs = (void***)calloc(2, sizeof(void*));
    for (int bx = 0; bx < 2; bx++) {
        void** ys = (void**)calloc(2, sizeof(void*)); xs[bx] = (void**)ys;
        for (int by = 0; by < 2; by++) {
            void** zs = (void**)calloc(3, sizeof(void*)); ys[by] = zs;
            for (int z = 0; z < 3; z++) zs[z] = calloc(1, 4096);   // (tiletype 64, designation 576, temperature 2048)
        }
    }
    for (int d = 0; d < 3; d++) {
        vps[d].flag = 1; vps[d].dx = DX; vps[d].dy = DY;
        for (int l = 0; l < 7; l++) vps[d].L[l] = (int32_t*)calloc(N, 4);
    }
    B.off_flag = 0; B.off_dim_x = 4; B.off_dim_y = 8; B.off_background = 16;
    B.off_tiletype = 64; B.off_designation = 576; B.off_temperature = 2048;
    B.outside_mask = 1u << 16; B.subterranean_mask = 1u << 15; B.flow_mask = 7; B.snowy_from = 100;
    B.main_viewport = (uint64_t)(uintptr_t)&main_ptr; B.lower_viewports = (uint64_t)(uintptr_t)&lower[0]; B.lower_count = 2;
    B.window_x = (uint64_t)(uintptr_t)&wx; B.window_y = (uint64_t)(uintptr_t)&wy; B.window_z = (uint64_t)(uintptr_t)&wz;
    B.block_index = (uint64_t)(uintptr_t)&xs;
    B.x_count_block = (uint64_t)(uintptr_t)&xcb; B.y_count_block = (uint64_t)(uintptr_t)&ycb; B.z_count_block = (uint64_t)(uintptr_t)&zcb;
    B.black_texpos = 3;
    B.crack_ground = (uint64_t)(uintptr_t)g_ground;   // (as dfhooks_init publishes it)
    g_us_per_tick = 1e-3;
}
// a map tile: its tiletype, designation and temperature (the game's units)
void pl_tile(int x, int y, int z, int tt, uint32_t des, int units) {
    uint8_t* b = (uint8_t*)((void**)((void**)xs[x >> 4])[y >> 4])[z];
    ((uint16_t*)(b + 64))[tile_of(x, y)] = (uint16_t)tt;
    ((uint32_t*)(b + 576))[tile_of(x, y)] = des;
    ((uint16_t*)(b + 2048))[tile_of(x, y)] = (uint16_t)units;
}
// THE PAINTING HERE: a texture's surface (32 x 32, `pitch` bytes a row), its pixels from `seed`; a copy
// of another's pixels (the same picture under a new number); the heat patterns' pieces; blanks
struct Surf { uint32_t flags; uint32_t pad; void* fmt; int32_t w, h, pitch, pad2; uint32_t* px; };
static void* g_raws[60000];
static struct { void** first; void** last; } g_raws_vec = {g_raws, g_raws + 60000};
static Surf g_surf[64]; static uint32_t g_spix[64][32 * 40]; static int g_nsurf = 0;
static uint32_t g_heat[16][1024];
static uint32_t g_blank[1024][1024]; static int g_nblank = 0;
void pl_surface(int32_t t, uint32_t seed, int32_t pitch, int32_t like) {
    Surf& sf = g_surf[g_nsurf]; uint32_t* px = g_spix[g_nsurf]; g_nsurf++;
    for (int i = 0; i < 32 * 40; i++) px[i] = 0xff000000u | ((uint32_t)(i * 2654435761u + seed * 40503u) >> 8);
    if (like > 0) {   // (the same pixels as texture `like`)
        const Surf* o = (const Surf*)g_raws[like];
        for (int y = 0; y < 32; y++) memcpy(px + y * (pitch / 4), o->px + y * (o->pitch / 4), 128);
    }
    sf = {0, 0, nullptr, 32, 32, pitch, 0, px};
    g_raws[t] = &sf;
    B.col_raws = (uint64_t)(uintptr_t)&g_raws_vec;
}
uint64_t pl_heat_addr(int k) {
    for (int n = 0; n < 1024; n++) g_heat[k][n] = 0xff000000u | ((uint32_t)((n * 7 + k * 131) % 9) << 8) | (uint32_t)((n * 13 + k * 17) % 11);
    return (uint64_t)(uintptr_t)g_heat[k];
}
// n blanks into the ring (as native.lua's crack_blanks), textures 50000 on
void pl_blanks(int n) {
    for (int k = 0; k < n; k++, g_nblank++) {
        CrackBlank& bl = B.crack_b[B.crack_b_head % CRACK_BLANKS];
        bl.tex = 50000 + g_nblank; bl.addr = (uint64_t)(uintptr_t)g_blank[g_nblank % 1024]; B.crack_b_head++;
    }
}
// whether copy `tex` is exactly what painting `base` (piece, ramp kind or floor ground kind) with look slot
// `slot`'s numbers gives (the same paint job built by hand, as colsim's expect)
int pl_expect(int32_t tex, int32_t base, int piece, int ramp_kind, uint32_t ground_kind, int slot) {
    static uint32_t want[1024], src[1024];
    PaintJob j;
    if (ramp_kind) j = B.crack_look[slot].ramp[ramp_kind - 1];
    else { memset(&j, 0, sizeof j); j.m[0] = j.m[4] = j.m[8] = 1; j.lean[0] = j.lean[1] = j.lean[2] = 1;
           j.soil[0] = j.soil[1] = j.soil[2] = 1; j.dark = 1; j.ice_dark = 1; }
    const Surf* sf = (const Surf*)g_raws[base];
    for (int y = 0; y < 32; y++) memcpy(src + y * 32, sf->px + y * (sf->pitch / 4), 128);
    j.src = (uint64_t)(uintptr_t)src; j.dst = (uint64_t)(uintptr_t)want;
    j.crack_kind = ramp_kind ? (uint32_t)ramp_kind : ground_kind; j.crack_piece = piece; j.crack_amount = B.crack_look[slot].amount;
    j.crack_here = B.crack_heat[piece]; j.crack_north = B.crack_heat[(piece + 12) & 15];
    const int64_t pd = B.paint_done; paint_one(j); B.paint_done = pd;
    return tex >= 50000 && memcmp(want, g_blank[(tex - 50000) % 1024], 4096) == 0;
}
// a texture's pixel n (rows unpadded), for looks.lua's redraw in place.py
uint32_t pl_pixel(int32_t t, int n) { const Surf* sf = (const Surf*)g_raws[t]; return sf->px[(n / 32) * (sf->pitch / 4) + n % 32]; }
// the DLL's cracks (crack_in_place, bit for bit frost.lua's cracked(): check/crack) over 1024 words in place
void pl_crack(uint32_t* words, uint32_t kind, double amount, int piece) {
    crack_in_place(words, kind, amount, (const uint32_t*)(uintptr_t)B.crack_heat[piece], (const uint32_t*)(uintptr_t)B.crack_heat[(piece + 12) & 15]);
}
// a painted copy's pixel n
uint32_t pl_copy_pixel(int32_t tex, int n) { return g_blank[(tex - 50000) % 1024][n]; }
void pl_table_copy(int32_t copy, int32_t orig) { g_cflag[copy] |= CF_TABLE; g_link[copy] = orig; }
int pl_ground(int32_t t) { return g_ground[t]; }
int32_t pl_get(int d, int sx, int sy) { return vps[d].L[0][sx * DY + sy]; }
void pl_set(int d, int sx, int sy, int32_t t) { vps[d].L[0][sx * DY + sy] = t; }
// THE COLOUR's tables, as colour.lua would have them: a ramp's copy in the look over its own picture
void pl_publish(int32_t orig, int32_t copy) {
    g_link[orig] = copy; g_cflag[orig] |= CF_COPY_FOR;
    g_link[copy] = orig; g_cflag[copy] = CF_SCREEN;
}
int32_t pl_link(int32_t t) { return g_link[t]; }
int32_t pl_flags(int32_t t) { return g_cflag[t]; }
int32_t pl_mark(int32_t t) { return g_crack_mark[t]; }
// the block's offsets, by name (place.py checks native.lua's OFF against them)
int64_t pl_off(const char* n) {
    if (!strcmp(n, "crack_on")) return (int64_t)offsetof(Block, crack_on);
    if (!strcmp(n, "crack_gen")) return (int64_t)offsetof(Block, crack_gen);
    if (!strcmp(n, "crack_gen_prev")) return (int64_t)offsetof(Block, crack_gen_prev);
    if (!strcmp(n, "crack_gen_next")) return (int64_t)offsetof(Block, crack_gen_next);
    if (!strcmp(n, "crack_next_walked")) return (int64_t)offsetof(Block, crack_next_walked);
    if (!strcmp(n, "crack_next_missing_last")) return (int64_t)offsetof(Block, crack_next_missing_last);
    if (!strcmp(n, "crack_depth")) return (int64_t)offsetof(Block, crack_depth);
    if (!strcmp(n, "crack_world")) return (int64_t)offsetof(Block, crack_world);
    if (!strcmp(n, "crack_recheck")) return (int64_t)offsetof(Block, crack_recheck);
    if (!strcmp(n, "off_temperature")) return (int64_t)offsetof(Block, off_temperature);
    if (!strcmp(n, "crack_forced_on")) return (int64_t)offsetof(Block, crack_forced_on);
    if (!strcmp(n, "crack_tables")) return (int64_t)offsetof(Block, crack_tables);
    if (!strcmp(n, "crack_band_c")) return (int64_t)offsetof(Block, crack_band_c);
    if (!strcmp(n, "crack_cool_from")) return (int64_t)offsetof(Block, crack_cool_from);
    if (!strcmp(n, "crack_warm_from")) return (int64_t)offsetof(Block, crack_warm_from);
    if (!strcmp(n, "crack_forced_c")) return (int64_t)offsetof(Block, crack_forced_c);
    if (!strcmp(n, "crack_tt")) return (int64_t)offsetof(Block, crack_tt);
    if (!strcmp(n, "crack_look")) return (int64_t)offsetof(Block, crack_look);
    if (!strcmp(n, "crack_heat")) return (int64_t)offsetof(Block, crack_heat);
    if (!strcmp(n, "crack_ground")) return (int64_t)offsetof(Block, crack_ground);
    if (!strcmp(n, "crack_b_head")) return (int64_t)offsetof(Block, crack_b_head);
    if (!strcmp(n, "crack_b_tail")) return (int64_t)offsetof(Block, crack_b_tail);
    if (!strcmp(n, "crack_b")) return (int64_t)offsetof(Block, crack_b);
    if (!strcmp(n, "crack_d_head")) return (int64_t)offsetof(Block, crack_d_head);
    if (!strcmp(n, "crack_d_tail")) return (int64_t)offsetof(Block, crack_d_tail);
    if (!strcmp(n, "crack_d")) return (int64_t)offsetof(Block, crack_d);
    if (!strcmp(n, "crack_frames")) return (int64_t)offsetof(Block, crack_frames);
    if (!strcmp(n, "crack_written")) return (int64_t)offsetof(Block, crack_written);
    if (!strcmp(n, "crack_painted")) return (int64_t)offsetof(Block, crack_painted);
    if (!strcmp(n, "crack_ahead_painted")) return (int64_t)offsetof(Block, crack_ahead_painted);
    if (!strcmp(n, "crack_ramp_held")) return (int64_t)offsetof(Block, crack_ramp_held);
    if (!strcmp(n, "crack_blocks_worked")) return (int64_t)offsetof(Block, crack_blocks_worked);
    if (!strcmp(n, "crack_waiting")) return (int64_t)offsetof(Block, crack_waiting);
    if (!strcmp(n, "crack_us_total")) return (int64_t)offsetof(Block, crack_us_total);
    if (!strcmp(n, "crack_us_max")) return (int64_t)offsetof(Block, crack_us_max);
    if (!strcmp(n, "crack_paint_us_total")) return (int64_t)offsetof(Block, crack_paint_us_total);
    if (!strcmp(n, "crack_paint_us_max")) return (int64_t)offsetof(Block, crack_paint_us_max);
    if (!strcmp(n, "crack_cells_last")) return (int64_t)offsetof(Block, crack_cells_last);
    if (!strcmp(n, "crack_written_last")) return (int64_t)offsetof(Block, crack_written_last);
    if (!strcmp(n, "crack_held_last")) return (int64_t)offsetof(Block, crack_held_last);
    if (!strcmp(n, "crack_waiting_last")) return (int64_t)offsetof(Block, crack_waiting_last);
    if (!strcmp(n, "crack_ramp_held_last")) return (int64_t)offsetof(Block, crack_ramp_held_last);
    if (!strcmp(n, "crack_tiles_kept")) return (int64_t)offsetof(Block, crack_tiles_kept);
    if (!strcmp(n, "crack_cleared")) return (int64_t)offsetof(Block, crack_cleared);
    if (!strcmp(n, "crack_worked_last")) return (int64_t)offsetof(Block, crack_worked_last);
    if (!strcmp(n, "crack_painted_last")) return (int64_t)offsetof(Block, crack_painted_last);
    if (!strcmp(n, "crack_no_blank")) return (int64_t)offsetof(Block, crack_no_blank);
    if (!strcmp(n, "crack_over_time")) return (int64_t)offsetof(Block, crack_over_time);
    if (!strcmp(n, "crack_unreadable")) return (int64_t)offsetof(Block, crack_unreadable);
    if (!strcmp(n, "crack_copies_kept")) return (int64_t)offsetof(Block, crack_copies_kept);
    if (!strcmp(n, "crack_table_full")) return (int64_t)offsetof(Block, crack_table_full);
    if (!strcmp(n, "crack_pics_kept")) return (int64_t)offsetof(Block, crack_pics_kept);
    if (!strcmp(n, "crack_done_full")) return (int64_t)offsetof(Block, crack_done_full);
    return -1;
}
// the hook's part of a frame (the colour's own swaps off: the cracks alone)
void pl_frame() { dfhooks_prerender(); read_view_as_drawn(); place_cracks(); }
}
