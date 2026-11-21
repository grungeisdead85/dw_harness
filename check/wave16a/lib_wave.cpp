// 16A: THE GRASS WAVE DRAWN HERE as a library, for wave.py to set against colour.lua's own grass_wave
// (run in lupa with stand-ins): a fake main viewport whose ground layer both draw into.
// Build: g++ -O1 -std=c++17 -I../shim -w '-D__declspec(x)=' -shared -fPIC lib_wave.cpp -o lib_wave.so
#include "../colsim_head.h"   // (the Windows stand-ins, then the DLL itself)

static uint8_t g_fake_vp[256];
static uint8_t* g_fake_vp_ptr = g_fake_vp;
extern "C" uint64_t l_block() { return (uint64_t)(uintptr_t)&g_block; }
extern "C" uint64_t l_wave_slots() { return (uint64_t)(uintptr_t)&g_wave_slots[0]; }
extern "C" int l_slot_bytes() { return (int)sizeof(WaveSlot); }
// The main viewport: dx by dy cells, its ground layer at bg (column by column, as DF keeps it).
extern "C" void l_wave_init(int dx, int dy, int32_t* bg) {
    B.off_dim_x = 16; B.off_dim_y = 20; B.off_background = 24;   // (0 reads as not set)
    *(int32_t*)(g_fake_vp + 16) = dx;
    *(int32_t*)(g_fake_vp + 20) = dy;
    *(int32_t**)(g_fake_vp + 24) = bg;
    B.main_viewport = (uint64_t)(uintptr_t)&g_fake_vp_ptr;
    B.wave_slots = (uint64_t)(uintptr_t)&g_wave_slots[0];
    B.wave_slots_most = WAVE_SLOTS;
    B.col_link = (uint64_t)(uintptr_t)g_link;   // (as the DLL's start sets them)
    B.col_flags = (uint64_t)(uintptr_t)g_cflag;
    B.col_tex_map = TEX_MAP;
    B.sway_of = (uint64_t)(uintptr_t)g_sway_of;
    B.sway_tex = (uint64_t)(uintptr_t)&g_sway_tex[0][0];
    if (g_us_per_tick == 0) g_us_per_tick = 1e-3;   // (the shim's ticks are nanoseconds)
}
extern "C" void l_wave_draw() { draw_grass_wave(); }
extern "C" int l_wave_map_n() { return g_wave_map_n; }
