// THE SPATTER DRAWN HERE (test 15Y): the DLL's spatter painting as a library, for compare.py to set against
// spatter.lua's own mask_for, bake_spatter, blade_mask and bake_blades.
// Build: g++ -O1 -std=c++17 -I../shim -w '-D__declspec(x)=' -shared -fPIC spatlib.cpp -o spatlib.so
#include "../colsim_head.h"   // (the Windows stand-ins, then the DLL itself)
extern "C" void sl_piece(const uint32_t* t_px, const uint32_t* o_px, double share, int variant, uint32_t* out) {
    spat_make_piece(t_px, o_px, share, variant, out);
}
extern "C" void sl_blades(const uint32_t* g_px, const uint32_t* o_px, uint32_t* out) {
    spat_make_blades(g_px, o_px, out);
}
