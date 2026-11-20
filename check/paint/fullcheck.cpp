// fullcheck.cpp: THE FULL REDRAW on given jobs, for compare_full.py.
#include <ctime>
#include <cstdio>
#include "windows.h"
SIZE_T VirtualQuery(LPCVOID p, MEMORY_BASIC_INFORMATION* m, SIZE_T) {
    m->BaseAddress = (PVOID)p; m->RegionSize = 1 << 20; m->State = MEM_COMMIT; m->Protect = PAGE_READWRITE; return sizeof *m; }
BOOL VirtualProtect(LPVOID, SIZE_T, DWORD, PDWORD) { return 1; }
HMODULE GetModuleHandleW(LPCWSTR) { return nullptr; }
BOOL QueryPerformanceCounter(LARGE_INTEGER* t) { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); t->QuadPart = ts.tv_sec * 1000000000LL + ts.tv_nsec; return 1; }
BOOL QueryPerformanceFrequency(LARGE_INTEGER* f) { f->QuadPart = 1000000000LL; return 1; }
PVOID InterlockedExchangePointer(PVOID volatile* p, PVOID v) { PVOID o = *p; *p = v; return o; }
DWORD GetLastError() { return 0; }
DWORD GetFileAttributesA(LPCSTR) { return INVALID_FILE_ATTRIBUTES; }
extern "C" int _putenv_s(const char*, const char*) { return 0; }
#include "../../detailed_weather_dll.cpp"
int main() {
    static uint32_t src[1024], dst[1024], grain[1024];
    PaintJob j{};
    unsigned has_grain;
    while (scanf("%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %u %lf %lf %u %lf %lf %lf %u %u %lf %lf %lf %lf %lf %u",
                 &j.m[0], &j.m[1], &j.m[2], &j.m[3], &j.m[4], &j.m[5], &j.m[6], &j.m[7], &j.m[8],
                 &j.lean[0], &j.lean[1], &j.lean[2], &j.has_lean, &j.dark, &j.lighten, &j.cls,
                 &j.soil[0], &j.soil[1], &j.soil[2], &j.has_soil, &j.weather, &j.dryness, &j.wet, &j.thaw,
                 &j.ice_k, &j.ice_dark, &has_grain) == 27) {
        for (int n = 0; n < 1024; n++) scanf("%u", &grain[n]);
        for (int n = 0; n < 1024; n++) scanf("%u", &src[n]);
        j.grain = has_grain ? (uint64_t)(uintptr_t)grain : 0;
        j.src = (uint64_t)(uintptr_t)src; j.dst = (uint64_t)(uintptr_t)dst;
        double t0 = ticks_now();
        paint_one(j);
        double us = (ticks_now() - t0) * 1e-3;
        for (int n = 0; n < 1024; n++) printf("%u\n", dst[n]);
        fprintf(stderr, "%.1f\n", us);
    }
    printf("bad %u\n", B.paint_bad);
}
