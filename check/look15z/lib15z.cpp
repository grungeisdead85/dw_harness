// 15Z: the DLL's new pixel work as a library, for compare.py to set against the real Lua (colour.lua's
// unpremultiplied, clump_offset and leaning; snow_ramps.lua's bake_fade, snow_with and snowy_overhang), and
// the snow's bakes made here checked end to end (bake_flow).
// Build: g++ -O1 -std=c++17 -I../shim -w '-D__declspec(x)=' -shared -fPIC lib15z.cpp -o lib15z.so
#include "../colsim_head.h"   // (the Windows stand-ins, then the DLL itself)

extern "C" void l_unpremult(const uint32_t* src, uint32_t* out) { paint_unpremultiplied(src, out); }
extern "C" void l_lean(const uint32_t* src, uint32_t* out, double amount, double ux, double uy) { paint_lean(src, out, amount, ux, uy); }
extern "C" int l_clump(const uint32_t* src, int* ox, int* oy) { return lean_clump(src, *ox, *oy) ? 1 : 0; }
extern "C" void l_fade(const uint32_t* thin, const uint32_t* full, int sides, uint32_t* out) { bake_fade_px(thin, full, sides, out); }
extern "C" void l_with(const uint32_t* base, const uint32_t* over, uint32_t* out) { bake_with_px(base, over, out); }
extern "C" void l_edge(const uint32_t* pieces, int n, const uint32_t* fill, uint32_t* out) {
    const uint32_t* pp[8];
    for (int k = 0; k < n && k < 8; k++) pp[k] = pieces + 1024 * k;
    bake_edge_px(pp, n, fill, out);
}
// A whole job through paint_one, its other fields left as a slot used before would have them
// (they must be ignored for these kinds).
extern "C" int l_job(int kind, const uint32_t* src, uint32_t* dst, double a, double b, double c) {
    PaintJob j;
    memset(&j, 0x5a, sizeof j);   // (stale bytes everywhere)
    j.src = (uint64_t)(uintptr_t)src; j.dst = (uint64_t)(uintptr_t)dst;
    j.kind = (uint32_t)kind; j.m[0] = a; j.m[1] = b; j.m[2] = c;
    const int64_t bad = B.paint_bad;
    paint_one(j);
    return B.paint_bad == bad;
}

// ---- the snow's bakes made here, end to end ----
struct Surf { uint32_t flags; uint32_t pad; void* fmt; int32_t w, h, pitch, pad2; uint32_t* px; };
static void* g_raws[70000];
static Surf g_surf[16];
static uint32_t g_spix[16][32 * 40];
struct Vec { void** first; void** last; };
static Vec g_vec = {g_raws, g_raws + 70000};
static uint32_t g_blank[8][1024];
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static uint32_t* surf(int k, int32_t t, uint32_t seed, int32_t pitch) {
    for (int i = 0; i < 32 * 40; i++) {
        const uint32_t v = (uint32_t)(i * 2654435761u + seed * 40503u);
        const uint32_t a = (i % 7 == 0) ? 0 : (i % 5 == 0) ? (v >> 24) : 255;
        g_spix[k][i] = (a << 24) | ((v >> 4) & 0xffffff);
    }
    g_surf[k] = {0, 0, nullptr, 32, 32, pitch, 0, g_spix[k]};
    g_raws[t] = &g_surf[k];
    return g_spix[k];
}
extern "C" int l_bake_flow() {
    fails = 0;
    B.col_raws = (uint64_t)(uintptr_t)&g_vec;
    uint32_t* thin = surf(0, 61000, 1, 128);
    uint32_t* full = surf(1, 61001, 2, 160);   // (padded rows: copied out)
    uint32_t* top = surf(2, 61002, 3, 128);
    uint32_t* e1 = surf(3, 61003, 4, 128);
    uint32_t* e2 = surf(4, 61004, 5, 128);
    uint32_t* fill = surf(5, 61005, 6, 128);
    B.pieces_n[15] = 1; B.pieces_tex[15][0] = 61005;
    B.crack_b_head = B.crack_b_tail = 0;
    for (int k = 0; k < 4; k++) { B.crack_b[k].tex = 63000 + k; B.crack_b[k].addr = (uint64_t)(uintptr_t)g_blank[k]; B.crack_b_head++; }
    B.snow_bake_here = 1;
    // a fade: painted here at once, told to Lua, the colour told to leave it be, kept
    uint32_t want[1024], fullc[1024];
    for (int y = 0; y < 32; y++) memcpy(fullc + y * 32, full + y * 40, 128);
    bake_fade_px(thin, fullc, 1 | 8, want);
    const int32_t f = bake(fade_key(61000, 61001, 9), 1, 61000, 61001, 9, nullptr, 0);
    CHECK(f == 63000 && !memcmp(g_blank[0], want, 4096), "fade not painted here: %d", f);
    CHECK(B.bake_here_head == 1 && B.bake_here[0].tex == 63000 && B.bake_here[0].kind == 1 && B.bake_here[0].key == fade_key(61000, 61001, 9),
          "fade not told to Lua");
    CHECK(is_ours(63000) && g_cflag[63000] == CF_LEAVE, "fade not marked as ours");
    CHECK(bake(fade_key(61000, 61001, 9), 1, 61000, 61001, 9, nullptr, 0) == 63000 && B.crack_b_tail == 1 && B.bake_here_head == 1, "fade made again");
    CHECK(bake(fade_key(61000, 61001, 0), 1, 61000, 61001, 0, nullptr, 0) == 61000 && B.crack_b_tail == 1, "a fade to no side isn't the thin piece");
    // a piece with something over it, and a snowy edge of two pieces
    bake_with_px(thin, top, want);
    CHECK(bake(with_key(61000, 61002), 2, 61000, 61002, 0, nullptr, 0) == 63001 && !memcmp(g_blank[1], want, 4096), "with not painted here");
    const int32_t pieces[2] = {61003, 61004};
    const uint32_t* pp[2] = {e1, e2};
    bake_edge_px(pp, 2, fill, want);
    CHECK(bake(edge_key(pieces, 2), 3, 0, 0, 0, pieces, 2) == 63002 && !memcmp(g_blank[2], want, 4096) && B.bake_here[2].kind == 3, "edge not painted here");
    // no plain snow to fill an edge with: asked of Lua (it makes a clear one), not painted here
    B.pieces_n[15] = 0;
    const uint32_t req00 = B.req_head;
    const int32_t one[1] = {61004};
    CHECK(bake(edge_key(one, 1), 3, 0, 0, 0, one, 1) == 0 && B.req_head == req00 + 1 && B.crack_b_tail == 3, "an edge with no fill painted here");
    B.pieces_n[15] = 1;
    // a piece that can't be read: asked of Lua, as before
    const uint32_t req0 = B.req_head, off0 = B.bakes_here_put_off;
    const int32_t bad_pieces[2] = {61003, 61999};
    CHECK(bake(edge_key(bad_pieces, 2), 3, 0, 0, 0, bad_pieces, 2) == 0 && B.req_head == req0 + 1 && B.bakes_here_put_off == off0 + 1 &&
          B.crack_b_tail == 3, "an unreadable piece not asked of Lua");
    // no blank: asked of Lua
    bake(with_key(61001, 61002), 2, 61001, 61002, 0, nullptr, 0);   // (the last blank)
    CHECK(B.crack_b_tail == 4, "the last blank not used");
    CHECK(bake(with_key(61002, 61001), 2, 61002, 61001, 0, nullptr, 0) == 0 && B.req_head == req0 + 2, "no blank: not asked of Lua");
    // the list to tell Lua full: asked of Lua
    B.crack_b_head++; B.crack_b[4].tex = 63004; B.crack_b[4].addr = (uint64_t)(uintptr_t)g_blank[4];
    B.bake_here_tail = B.bake_here_head - 256;
    CHECK(bake(with_key(61003, 61002), 2, 61003, 61002, 0, nullptr, 0) == 0 && B.req_head == req0 + 3 && B.crack_b_tail == 4, "the list full: painted anyway");
    B.bake_here_tail = B.bake_here_head;
    // the frame's painting time used up: put off (not asked of Lua), painted on the next frame
    g_bake_here_frame = B.tidy_calls; g_bake_here_us = 1e9;
    const uint32_t over0 = B.bakes_here_over_time, req1 = B.req_head;
    B.crack_b_head++; B.crack_b[5].tex = 63005; B.crack_b[5].addr = (uint64_t)(uintptr_t)g_blank[5];
    CHECK(bake(with_key(61004, 61003), 2, 61004, 61003, 0, nullptr, 0) == 0 && B.bakes_here_over_time == over0 + 1 && B.req_head == req1 &&
          B.crack_b_tail == 4, "over the frame's time: painted or asked of Lua");
    B.tidy_calls++;
    CHECK(bake(with_key(61004, 61003), 2, 61004, 61003, 0, nullptr, 0) == 63004, "not painted on the next frame");
    // off: asked of Lua
    B.snow_bake_here = 0;
    CHECK(bake(with_key(61004, 61002), 2, 61004, 61002, 0, nullptr, 0) == 0 && B.req_head == req0 + 4 && B.crack_b_tail == 5, "off: painted here");
    printf("bake flow: %lld painted here, %u asked of Lua instead\n", (long long)B.bakes_here, B.bakes_here_put_off);
    return fails;
}

// ---- timing: each kind of painting, a thousand times ----
extern "C" void l_perf() {
    g_us_per_tick = 1e-3;   // (the stand-in counter ticks in ns)
    static uint32_t a[1024], b[1024], c[1024], out[1024];
    for (int i = 0; i < 1024; i++) {
        const uint32_t v = (uint32_t)(i * 2654435761u);
        a[i] = ((i % 9 ? 255u : (v >> 24)) << 24) | ((v >> 4) & 0xffffff);
        b[i] = ((i % 3 ? 200u : 0u) << 24) | ((v >> 8) & 0xffffff);
        c[i] = (255u << 24) | (v & 0xffffff);
    }
    const uint32_t* pp[2] = {a, b};
    struct T { const char* name; double us; } t[6];
    auto time = [&](const char* name, int k, auto f) {
        const double t0 = ticks_now();
        for (int n = 0; n < 1000; n++) f();
        t[k] = {name, (ticks_now() - t0) * g_us_per_tick / 1000};
    };
    time("lean frame", 0, [&] { paint_lean(a, out, 4.2, 0.7071, 0.7071); });
    time("alpha divided out", 1, [&] { paint_unpremultiplied(b, out); });
    time("snowy edge (2 pieces)", 2, [&] { bake_edge_px(pp, 2, c, out); });
    time("thin snow faded", 3, [&] { bake_fade_px(a, c, 9, out); });
    time("piece with something over it", 4, [&] { bake_with_px(c, b, out); });
    time("a picture worked out (by picture)", 5, [&] { out[0] += (uint32_t)crack_hash_pixels(a); });
    for (int k = 0; k < 6; k++) printf("PERF %s: %.2f us each\n", t[k].name, t[k].us);
}
