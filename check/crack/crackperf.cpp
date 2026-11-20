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
#include "../../detailed_weather_dll.cpp"
int main() {
    static uint32_t here[1024], north[1024], px[1024], base[1024];
    srand(1);
    for (int n = 0; n < 1024; n++) { here[n] = rand(); north[n] = rand(); base[n] = rand() | 0xff000000u; }
    g_us_per_tick = 1e-3;
    double worst = 0, total = 0; const int K = 2000;
    for (int k = 0; k < K; k++) {
        memcpy(px, base, sizeof px);
        const double t0 = ticks_now(); crack_in_place(px, 1 + k % 3, 0.3 + (k % 7) * 0.1, here, north);
        const double us = (ticks_now() - t0) * g_us_per_tick; total += us; if (us > worst) worst = us;
    }
    printf("crack_in_place: %.1f us a tile on average, worst %.1f\n", total / K, worst);
}
