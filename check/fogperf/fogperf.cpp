// fogperf.cpp: times FOG OVER WATER's plan and fix on a big view looking down 7 open levels onto land and water.
#include <ctime>
#include <cstdio>
#include <vector>
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
static const uint32_t OFF_TT = 64, OFF_DES = 576;
struct VP { uint32_t flag; int32_t dx, dy; int32_t pad; int32_t* L[7]; uint64_t* FF; int32_t* IF; };
static const int DX = 160, DY = 90, N = DX * DY, LV = 9, BX = 12, BY = 8, ZN = 12;
static VP vps[LV]; static VP* main_ptr = &vps[0]; static VP* lower[8];
static int32_t wx = 0, wy = 0, wz = 10, xcb = BX, ycb = BY, zcb = ZN;
static void*** xs;
int main() {
    xs = (void***)calloc(BX, sizeof(void*));
    for (int bx = 0; bx < BX; bx++) { void** ys = (void**)calloc(BY, sizeof(void*)); xs[bx] = (void**)ys;
        for (int by = 0; by < BY; by++) { void** zs = (void**)calloc(ZN, sizeof(void*)); ys[by] = zs;
            for (int z = 0; z < ZN; z++) { uint8_t* b = (uint8_t*)calloc(1, 2048); zs[z] = b;
                for (int t = 0; t < 256; t++) {
                    int x = bx * 16 + (t >> 4);
                    // ground at z 3 (land) and full water at z 2 under open air at z 3 in a river band;
                    // open air above, leaves on z 5 and 6 here and there
                    uint16_t tt = 9; uint32_t des = 0;
                    if (z < 3) tt = 1;
                    if (z == 3) tt = (x % 40 < 12) ? 9 : 1;
                    if (z == 2 && x % 40 < 12) { tt = 1; des = 7; }
                    if ((z == 5 || z == 6) && (t % 7) < 3) tt = 11;
                    ((uint16_t*)(b + OFF_TT))[t] = tt; ((uint32_t*)(b + OFF_DES))[t] = des;
                } } } }
    B.tt_flags[9] = TT_OPEN; B.tt_flags[11] = TT_TREE;
    static int32_t hazes[16]; for (int k = 0; k < 8; k++) hazes[k] = 70000 + k;
    for (int d = 0; d < LV; d++) { vps[d].flag = 1; vps[d].dx = DX; vps[d].dy = DY;
        for (int l = 0; l < 7; l++) vps[d].L[l] = (int32_t*)calloc(N, 4);
        vps[d].IF = (int32_t*)calloc(N, 4); if (d) lower[d - 1] = &vps[d]; }
    B.off_flag = 0; B.off_dim_x = 4; B.off_dim_y = 8; B.off_background = 16; B.off_bg2 = 24; B.off_building_one = 40;
    B.off_tiletype = OFF_TT; B.off_designation = OFF_DES; B.off_interface = 80;
    B.main_viewport = (uint64_t)(uintptr_t)&main_ptr; B.lower_viewports = (uint64_t)(uintptr_t)&lower[0]; B.lower_count = 8;
    B.window_x = (uint64_t)(uintptr_t)&wx; B.window_y = (uint64_t)(uintptr_t)&wy; B.window_z = (uint64_t)(uintptr_t)&wz;
    B.block_index = (uint64_t)(uintptr_t)&xs; B.x_count_block = (uint64_t)(uintptr_t)&xcb;
    B.y_count_block = (uint64_t)(uintptr_t)&ycb; B.z_count_block = (uint64_t)(uintptr_t)&zcb;
    B.flow_mask = 7; B.hidden_mask = 1u << 9; B.fog_tex_addr = (uint64_t)(uintptr_t)hazes; B.fog_enabled = 1;
    g_us_per_tick = 1e-3;
    // the game's hazes: land bottom at z 3 (7 levels down: haze 6), water at z 2 (8 down: haze 7);
    // the surface on z 3's open space over the water
    for (int i = 0; i < N; i++) { int x = i / DY; vps[1].IF[i] = (x % 40 < 12) ? hazes[7] : hazes[6]; if (x % 40 < 12) vps[7].L[0][i] = 555; }
    for (int f = 0; f < 200; f++) { read_view_as_drawn(); fog_put_back(); plan_fog(); fix_water_fog(); }
    printf("view %dx%d, 7 open levels: marked %u, moved %u; plan %.1f us (worst %.1f), fix %.1f us (worst %.1f)\n", DX, DY,
           B.fog_marked_last, B.fog_moved_last, B.fog_plan_us_total / B.fog_plans, B.fog_plan_us_max,
           B.fog_us_total / B.fog_frames, B.fog_us_max);
    return 0;
}
