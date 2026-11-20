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
// crackjob.cpp: DRY CRACKS through a whole PaintJob (paint_one), with the fields at their offsets as
// colour.lua writes them (byte offsets 216, 220, 224, 232, 240).
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main() {
    static uint32_t src[1024], dst[1024], want[1024], here[1024], north[1024];
    srand(7);
    for (int n = 0; n < 1024; n++) { src[n] = (uint32_t)rand() | ((n % 9 == 0) ? 0 : 0xff000000u); here[n] = rand(); north[n] = rand(); }
    static_assert(offsetof(PaintJob, crack_kind) == 216 && offsetof(PaintJob, crack_piece) == 220 &&
                  offsetof(PaintJob, crack_amount) == 224 && offsetof(PaintJob, crack_here) == 232 &&
                  offsetof(PaintJob, crack_north) == 240, "crack fields where colour.lua writes them");
    // a floor: colour.lua's NO_CHANGE job (identity matrix, dark 1, lighten 0), with cracks
    uint8_t raw[256] = {0};
    PaintJob& j = *(PaintJob*)raw;
    j.src = (uint64_t)(uintptr_t)src; j.dst = (uint64_t)(uintptr_t)dst;
    const double m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    memcpy(j.m, m, sizeof m); j.lean[0] = j.lean[1] = j.lean[2] = 1; j.dark = 1; j.lighten = 0; j.cls = 0;
    j.crack_kind = 1; j.crack_piece = 5; j.crack_amount = 0.7;
    j.crack_here = (uint64_t)(uintptr_t)here; j.crack_north = (uint64_t)(uintptr_t)north;
    memcpy(want, src, sizeof want);
    crack_in_place(want, 1, 0.7, here, north);
    const int64_t done0 = B.paint_done, bad0 = B.paint_bad;
    paint_one(j);
    CHECK(memcmp(dst, want, sizeof want) == 0, "a floor's crack job differs from crack_in_place on its pixels");
    CHECK(B.paint_done == done0 + 1 && B.paint_bad == bad0, "not counted as painted");
    // no crack: the copy unchanged (as before 15O)
    j.crack_kind = 0; j.crack_here = j.crack_north = 0;
    paint_one(j);
    CHECK(memcmp(dst, src, sizeof src) == 0, "a job without cracks changed the pixels");
    // cracks asked for with a pattern piece missing: refused, nothing painted
    memset(dst, 0, sizeof dst);
    j.crack_kind = 2; j.crack_here = (uint64_t)(uintptr_t)here; j.crack_north = 0;
    paint_one(j);
    CHECK(B.paint_bad == bad0 + 1, "a crack job with no north piece was not refused");
    // a kind out of range: refused
    j.crack_kind = 9; j.crack_north = (uint64_t)(uintptr_t)north;
    paint_one(j);
    CHECK(B.paint_bad == bad0 + 2, "a crack kind out of range was not refused");
    printf(fails ? "%d FAILS\n" : "crack jobs: checks pass\n", fails);
    return fails ? 1 : 0;
}
