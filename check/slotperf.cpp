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
int main() {
    g_us_per_tick = 1e-3;
    const int sizes[3][2] = {{160, 90}, {250, 100}, {320, 180}};
    for (auto& sz : sizes) {
        int32_t x = 0, y = 0, z = 100;
        B.drawn_x = x; B.drawn_y = y; B.drawn_z = z;
        held_ready(sz[0], sz[1]);
        double worst = 0, total = 0; const int K = 400;
        for (int k = 0; k < K; k++) {
            B.drawn_x = ++x; B.drawn_y = (k & 1) ? ++y : y;   // a pan every frame
            const double t0 = ticks_now(); held_ready(sz[0], sz[1]); const double us = (ticks_now() - t0) * g_us_per_tick;
            total += us; if (us > worst) worst = us;
        }
        printf("view %dx%d: held_ready on a panning frame %.1f us on average, worst %.1f; held memory %zu KB\n",
               sz[0], sz[1], total / K, worst, held_bytes() / 1024);
    }
}
