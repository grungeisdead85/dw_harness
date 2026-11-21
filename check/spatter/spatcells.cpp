// THE SPATTER DRAWN HERE (test 15Y): which cells the DLL puts in the grass and with what, against spatter.lua's
// rules (apply): spatter on grass (the ground a table copy of a grass original) thinned by how much has fallen
// there (item spatter events, read in the prerender call), the colour's screen copy of a fruit through its
// original, the blades in the item layer only where it's free; the snow's, the colour's table copies and what it
// leaves be left alone, and spatter on other ground; each texture the painting of spat_make_piece and
// spat_make_blades (themselves checked against spatter.lua by compare.py); the same picture under another number
// painted once; then put back over two frames of swapped buffers.
// Build: g++ -O2 -std=c++17 -I../shim -w '-D__declspec(x)=' spatcells.cpp -o spatcells
#include "../colsim_head.h"   // (the Windows stand-ins, then the DLL itself)
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const uint32_t OFF_TT = 64, OFF_DES = 576, OFF_EV = 1600;   // (the block's events vector at 1600)
static void*** xs; static int32_t xcb = 2, ycb = 2, zcb = 3;
struct VP { uint32_t flag; int32_t dx, dy; int32_t pad; int32_t* L[7]; };   // background 16, spatter 32, item 56
static VP vp; static VP* main_ptr = &vp; static VP* lower[2] = {nullptr, nullptr};
static const int DX = 12, DY = 10, N = DX * DY;
struct Surf { uint32_t flags; uint32_t pad; void* fmt; int32_t w, h, pitch, pad2; uint32_t* px; };
static void* raws[20000] = {};
static Surf surfs[64];
static uint32_t spix[64][32 * 32];
static int nsurf = 0;
static uint32_t* surface(int32_t t, uint32_t seed, bool pieces) {
    Surf& sf = surfs[nsurf]; uint32_t* px = spix[nsurf]; nsurf++;
    for (int i = 0; i < 1024; i++) {
        const uint32_t h = (uint32_t)(i * 2654435761u + seed * 40503u);
        const uint32_t a = pieces ? ((i % 5 == 0 || i % 7 == 0) ? 255u : (i % 11 == 0 ? 90u : 0u)) : ((h >> 30) ? 255u : 0u);
        px[i] = (a << 24) | ((h >> 8) & 0xffffff);
    }
    sf = {0, 0, nullptr, 32, 32, 128, 0, px};
    raws[t] = &sf;
    return px;
}
struct EvVec { void** first; void** last; void** end; };
struct FakeEvent { uint64_t vtable; uint8_t pad[24]; int32_t amount[16][16]; };   // (amount at 32)
static FakeEvent ev1, ev2, ev_other;
static uint32_t blanks[64][1024];

int main() {
    { LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_us_per_tick = 1e6 / (double)f.QuadPart; }
    xs = (void***)calloc(2, sizeof(void*));
    for (int bx = 0; bx < 2; bx++) {
        void** ys = (void**)calloc(2, sizeof(void*)); xs[bx] = (void**)ys;
        for (int by = 0; by < 2; by++) {
            void** zs = (void**)calloc(3, sizeof(void*)); ys[by] = zs;
            for (int z = 0; z < 3; z++) zs[z] = calloc(1, 4096);
        }
    }
    // the block at (0, 0, 1): two item spatter events and one of another kind
    ev1.vtable = 0xABC; ev2.vtable = 0xABC; ev_other.vtable = 0xDEF;
    ev1.amount[2][3] = 300; ev2.amount[2][3] = 100;     // 400 in all at (2, 3): a share of 0.1
    ev1.amount[4][1] = 5000;                            // (4, 1): 0.6
    ev_other.amount[5][5] = 9999;                       // (not item spatter: all of it)
    static void* evs[3] = {&ev1, &ev_other, &ev2};
    EvVec* v = (EvVec*)((uint8_t*)((void**)((void**)xs[0])[0])[1] + OFF_EV);
    v->first = evs; v->last = evs + 3; v->end = evs + 3;
    vp.flag = 1; vp.dx = DX; vp.dy = DY;
    for (int l = 0; l < 7; l++) vp.L[l] = (int32_t*)calloc(N, 4);
    int32_t* bufs[2][3];   // (two of each of the three layers, swapped each frame)
    for (int k = 0; k < 2; k++) for (int l = 0; l < 3; l++) bufs[k][l] = (int32_t*)calloc(N, 4);
    auto use = [&](int k) { vp.L[0] = bufs[k][0]; vp.L[2] = bufs[k][1]; vp.L[5] = bufs[k][2]; };
    use(0);
    B.off_flag = 0; B.off_dim_x = 4; B.off_dim_y = 8; B.off_background = 16; B.off_spatter = 32; B.off_item = 56;
    B.off_tiletype = OFF_TT; B.off_designation = OFF_DES; B.off_block_events = OFF_EV;
    B.main_viewport = (uint64_t)(uintptr_t)&main_ptr; B.lower_viewports = (uint64_t)(uintptr_t)&lower[0];
    B.block_index = (uint64_t)(uintptr_t)&xs;
    B.x_count_block = (uint64_t)(uintptr_t)&xcb; B.y_count_block = (uint64_t)(uintptr_t)&ycb; B.z_count_block = (uint64_t)(uintptr_t)&zcb;
    B.have_drawn = 1; B.drawn_x = 0; B.drawn_y = 0; B.drawn_z = 1;
    struct Vec { void** first; void** last; };
    static Vec rv = {raws, raws + 20000};
    B.col_raws = (uint64_t)(uintptr_t)&rv;
    // the textures: grass originals 100, 101; 200 the look's table copy of 100, 210 a leaning frame of 101
    // (linked as a table copy); 300 the game's fruit, 301 the colour's screen copy of it, 305 the same picture
    // as 300 under another number; 302 the snow page's; 303 one the colour leaves be; 400 ground that isn't grass
    uint32_t* g100 = surface(100, 1, false); surface(101, 2, false);
    uint32_t* g200 = surface(200, 3, false); uint32_t* g210 = surface(210, 4, false);
    uint32_t* f300 = surface(300, 5, true); uint32_t* f301 = surface(301, 6, true);
    uint32_t* f305 = surface(305, 99, true); memcpy(f305, f300, 4096);
    surface(302, 7, true); surface(303, 8, true); surface(400, 9, false);
    for (int i = 0; i < 1024; i++) f301[i] = (f301[i] & 0x00ffffff) | (f300[i] & 0xff000000u);   // (a copy keeps its alpha)
    g_spat_grass[100] = g_spat_grass[101] = 1;
    g_cflag[200] = CF_TABLE; g_link[200] = 100;
    g_cflag[210] = CF_TABLE; g_link[210] = 101;
    g_cflag[301] = CF_SCREEN; g_link[301] = 300;
    g_cflag[303] = CF_LEAVE;
    mark(302, TEX_PAGE);
    // the blanks
    for (int k = 0; k < 64; k++) { B.crack_b[k].tex = 5000 + k; B.crack_b[k].addr = (uint64_t)(uintptr_t)blanks[k]; }
    B.crack_b_head = 64; B.crack_b_tail = 0;
    B.spat_on = 1; B.spat_blades = 1; B.spat_vtable = 0xABC; B.off_isp_amount = 32;
    // the scene: (x, y) -> ground, spatter, item
    auto at = [&](int x, int y) { return x * DY + y; };
    auto put = [&](int x, int y, int32_t g, int32_t s, int32_t it) {
        for (int k = 0; k < 2; k++) { bufs[k][0][at(x, y)] = g; bufs[k][1][at(x, y)] = s; bufs[k][2][at(x, y)] = it; }
    };
    put(2, 3, 200, 300, 0);    // fruit on grass, 400 fallen: thinned (0.1), blades
    put(4, 1, 200, 301, 0);    // the colour's copy of the fruit, 5000 fallen (0.6): through 300's mask
    put(5, 5, 210, 300, 0);    // on a leaning frame of 101; other spatter only: all of it
    put(6, 6, 200, 300, 77);   // an item there: the spatter, no blades
    put(7, 2, 200, 302, 0);    // the snow page's: left
    put(8, 2, 200, 303, 0);    // left be by the colour: left
    put(9, 4, 400, 300, 0);    // on ground that isn't grass: left
    put(3, 8, 200, 200, 0);    // the colour's table copy in the spatter layer: left
    put(10, 9, 200, 305, 0);   // the same picture as 300 under another number (no fallen there: share 1)
    put(11, 9, 200, 300, 0);   // and 300 itself, share 1, same pattern ((11*7 + 9*13) mod 4 vs (10*7+9*13) mod 4)
    // the frame: the prerender call, then the hook
    g_leaf_frame = 1;
    plan_spatter();
    draw_spatter();
    auto px_of = [&](int32_t tex) -> const uint32_t* { return (tex >= 5000 && tex < 5064) ? blanks[tex - 5000] : nullptr; };
    auto share_for = [](int amount) { if (amount <= 0) return 1.0; double s = amount / 8000.0; if (s > 1) s = 1; if (s < 0.05) s = 0.05;
                                      double r = floor(s * 10 + 0.5) / 10; return r < 0.05 ? 0.05 : r; };
    auto variant = [](int x, int y) { return ((x * 7 + y * 13) % 4 + 4) % 4; };
    uint32_t want[1024];
    auto piece_ok = [&](int x, int y, const uint32_t* t_px, const uint32_t* o_px, int amount) {
        const uint32_t* got = px_of(vp.L[2][at(x, y)]);
        if (!got) return false;
        spat_make_piece(t_px, o_px, share_for(amount), variant(x, y), want);
        return memcmp(got, want, 4096) == 0;
    };
    auto blades_ok = [&](int x, int y, const uint32_t* g_px, const uint32_t* o_px) {
        const uint32_t* got = px_of(vp.L[5][at(x, y)]);
        if (!got) return false;
        spat_make_blades(g_px, o_px, want);
        return memcmp(got, want, 4096) == 0;
    };
    CHECK(piece_ok(2, 3, f300, f300, 400), "fruit on grass, 400 fallen: not thinned to 0.1 as spatter.lua would (%d)", vp.L[2][at(2, 3)]);
    CHECK(blades_ok(2, 3, g200, g100), "the blades over it: not the grass's (%d)", vp.L[5][at(2, 3)]);
    CHECK(piece_ok(4, 1, f301, f300, 5000), "the colour's copy of a fruit: not through its original's mask at 0.6 (%d)", vp.L[2][at(4, 1)]);
    CHECK(piece_ok(5, 5, f300, f300, 0), "other spatter only fallen: not all of it (%d)", vp.L[2][at(5, 5)]);
    CHECK(blades_ok(5, 5, g210, spix[1]), "blades on a leaning frame: not from its original's brightness (%d)", vp.L[5][at(5, 5)]);
    CHECK(px_of(vp.L[2][at(6, 6)]) && vp.L[5][at(6, 6)] == 77, "an item's cell: spatter %d, item %d", vp.L[2][at(6, 6)], vp.L[5][at(6, 6)]);
    CHECK(vp.L[2][at(7, 2)] == 302 && vp.L[5][at(7, 2)] == 0, "the snow's spatter touched: %d", vp.L[2][at(7, 2)]);
    CHECK(vp.L[2][at(8, 2)] == 303, "spatter the colour leaves be touched: %d", vp.L[2][at(8, 2)]);
    CHECK(vp.L[2][at(9, 4)] == 300 && vp.L[5][at(9, 4)] == 0, "spatter off the grass touched: %d", vp.L[2][at(9, 4)]);
    CHECK(vp.L[2][at(3, 8)] == 200, "a table copy in the spatter layer touched: %d", vp.L[2][at(3, 8)]);
    if (variant(10, 9) == variant(11, 9)) {
        CHECK(vp.L[2][at(10, 9)] == vp.L[2][at(11, 9)] && px_of(vp.L[2][at(10, 9)]), "the same picture under another number painted twice: %d %d",
              vp.L[2][at(10, 9)], vp.L[2][at(11, 9)]);
    } else {
        CHECK(piece_ok(10, 9, f305, f305, 0) && piece_ok(11, 9, f300, f300, 0), "the same picture, two patterns");
    }
    const int64_t painted = B.spat_painted;
    // the next frame: the other buffers, the game's own in them; nothing painted again
    use(1);
    kept_put_back(g_blade_kept, B.off_item, B.spat_put_back, B.spat_dropped);
    kept_put_back(g_spat_kept, B.off_spatter, B.spat_put_back, B.spat_dropped);
    g_leaf_frame++; plan_spatter(); draw_spatter();
    CHECK(B.spat_painted == painted && piece_ok(2, 3, f300, f300, 400), "painted again, or not drawn, the next frame: %lld", (long long)(B.spat_painted - painted));
    // turned off: both buffers given the game's own back over the next two frames
    B.spat_on = 0;
    use(0); kept_put_back(g_blade_kept, B.off_item, B.spat_put_back, B.spat_dropped); kept_put_back(g_spat_kept, B.off_spatter, B.spat_put_back, B.spat_dropped);
    use(1); kept_put_back(g_blade_kept, B.off_item, B.spat_put_back, B.spat_dropped); kept_put_back(g_spat_kept, B.off_spatter, B.spat_put_back, B.spat_dropped);
    int left = 0;
    for (int k = 0; k < 2; k++) for (int i = 0; i < N; i++) left += (bufs[k][1][i] >= 5000) + (bufs[k][2][i] >= 5000);
    CHECK(left == 0 && bufs[0][1][at(2, 3)] == 300 && bufs[1][2][at(2, 3)] == 0, "not all put back: %d left", left);
    printf("spatter cells: %lld textures painted, %u cells on grass the last frame\n", (long long)B.spat_painted, B.spat_cells_last);
    printf(fails ? "%d FAILED\n" : "spatter cells: all checks pass\n", fails);
    return fails != 0;
}
