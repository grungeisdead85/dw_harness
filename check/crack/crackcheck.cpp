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
// crackcheck.cpp: runs the DLL's crack_in_place on cases from stdin (compare.py writes them):
// each case: kind (uint32), amount (double), the piece here, the piece north, the pixels (1,024 uint32 each).
// Writes the cracked pixels to stdout, 1,024 uint32 a case.
int main() {
    uint32_t kind; double amount;
    static uint32_t here[1024], north[1024], px[1024];
    while (fread(&kind, 4, 1, stdin) == 1) {
        if (fread(&amount, 8, 1, stdin) != 1 || fread(here, 4, 1024, stdin) != 1024 || fread(north, 4, 1024, stdin) != 1024 ||
            fread(px, 4, 1024, stdin) != 1024) return 1;
        crack_in_place(px, kind, amount, here, north);
        fwrite(px, 4, 1024, stdout);
    }
    return 0;
}
