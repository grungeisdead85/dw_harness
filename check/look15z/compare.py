# 15Z: the DLL's new pixel work against the real Lua, run in lupa, pixel for pixel:
#   colour.lua: unpremultiplied (PJ_UNPREMULTIPLY), clump_offset and leaning (PJ_LEAN), through whole jobs too;
#   snow_ramps.lua: bake_fade, snow_with and snowy_overhang (THE SNOW'S BAKES HERE);
# then the snow's bakes made here, end to end (lib15z.cpp: l_bake_flow).
# Usage: python3 compare.py path/to/colour.lua path/to/snow_ramps.lua
import sys, ctypes, random, re
import lupa.lua53 as lupa
lib = ctypes.CDLL('./lib15z.so')
D = ctypes.c_double
lib.l_job.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, D, D, D]
lib.l_lean.argtypes = [ctypes.c_void_p, ctypes.c_void_p, D, D, D]
U32 = ctypes.c_uint32 * 1024
colour_src, snow_src = open(sys.argv[1]).read(), open(sys.argv[2]).read()

def grab(src, name):
    # a function's whole text: from its first line to the first `end` alone at the start of a line after it
    m = re.search(r'^(local )?function ' + re.escape(name) + r'\(.*?^end$', src, re.S | re.M)
    if not m: raise SystemExit('no function ' + name)
    return re.sub(r'^local ', '', m.group(0))
def const(src, name):
    m = re.search(r'^local ' + re.escape(name) + r' = [^\n]*', src, re.M)
    if not m: raise SystemExit('no constant ' + name)
    return re.sub(r'^local ', '', m.group(0))

L = lupa.LuaRuntime()
L.execute('''
PIX, OUT, SURF = {}, nil, nil
grass = {pixels_of = function(t) return PIX[t] end}
dfhack = {textures = {createTile = function(px, w, h) OUT = px return 1 end, getTexposByHandle = function(h) return 7 end}}
df = {global = {enabler = {textures = {raws = setmetatable({}, {__index = function(_, k) return SURF and 'surface' or nil end})}}}}
function df.reinterpret_cast(ty, obj)
    if obj == 'surface' and ty == 'int32_t' then
        return {_displace = function(self, n) return {[0] = ({[4] = SURF.w, [5] = SURF.h, [6] = SURF.pitch})[n]} end}
    elseif obj == 'surface' and ty == 'uint64_t' then
        return {_displace = function(self, n) return {[0] = 'pixels'} end}
    elseif obj == 'pixels' and ty == 'uint8_t' then
        return SURF.bytes
    end
end
function zero_based(t) local z = {} for i, v in ipairs(t) do z[i - 1] = v end return z end
made, snow_made, snowy_edge_texpos = {}, {}, {}
with_cache, thin_faded, snowy_overhangs = {}, {}, {}
piece_sides = true
pieces_for = {}
function kept_snowy_edge(key) return nil end
function keep_snowy_edge(pieces, out) end
''')
for c in ('TILE', 'LEAN_REACH_MOST'): L.execute(const(colour_src, c))
for f in ('unpremultiplied', 'clump_offset', 'leaning'): L.execute(grab(colour_src, f))
for c in ('THIN_FADE', 'SNOW_LIP', 'SNOW_RIM'): L.execute(const(snow_src, c))
for f in ('snow_with', 'bake_fade', 'snowy_overhang'): L.execute(grab(snow_src, f))
G = L.globals()

def to_px(words):   # grass.pixels_of's form: {r, g, b, a} per pixel
    t = L.table()
    for i, v in enumerate(words): t[i + 1] = L.table(v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24)
    return t
def out_words():
    o = G.OUT
    return [o[i + 1] & 0xffffffff for i in range(1024)]
def c_words(words): return U32(*words)

def pic(seed, kind):
    r = random.Random(seed)
    w = []
    for i in range(1024):
        x, y = i % 32, i // 32
        if kind == 'clump': a = 255 if r.random() < 0.95 else r.randint(0, 254)
        elif kind == 'edge':   # grass massed towards one side (an edge piece), see-through elsewhere
            side = seed % 4
            d = (x, 31 - x, y, 31 - y)[side]
            a = 255 if d < 8 else (r.randint(1, 254) if d < 12 else 0)
        elif kind == 'soft': a = r.choice([0, 0, 255, r.randint(1, 254)])
        elif kind == 'clear': a = 0
        elif kind.startswith('off'):   # a disc of grass a little off the middle (clump_offset's 3 px either side)
            d = float(kind[3:])
            cx, cy = (15.5 + d, 15.5) if seed % 2 else (15.5, 15.5 + d)
            a = 255 if (x - cx) ** 2 + (y - cy) ** 2 <= 100 else 0
        else: a = r.randint(0, 255)
        w.append((a << 24) | (r.randint(0, 255) << 16) | (r.randint(0, 255) << 8) | r.randint(0, 255))
    return w
def premultiplied(w):
    out = []
    for v in w:
        a = v >> 24
        ch = [((v >> s) & 255) * a // 255 for s in (0, 8, 16)]
        out.append((a << 24) | (ch[2] << 16) | (ch[1] << 8) | ch[0])
    return out

bad = cases = pixels = 0
first = []
def compare(name, want, got):
    global bad, cases, pixels
    cases += 1; pixels += 1024
    diff = [i for i in range(1024) if want[i] != got[i]]
    if diff:
        bad += 1
        if len(first) < 5: first.append((name, diff[0], hex(want[diff[0]]), hex(got[diff[0]]), len(diff)))

# ---- PJ_UNPREMULTIPLY against unpremultiplied (the surface read as the game keeps it) ----
for k in range(40):
    w = premultiplied(pic(k, ['soft', 'any', 'clump', 'edge'][k % 4])) if k % 5 else pic(k, 'any')
    pitch = 128 if k % 3 else 160
    bytes_ = [0] * (pitch * 32)
    for i, v in enumerate(w):
        o = (i // 32) * pitch + (i % 32) * 4
        bytes_[o:o + 4] = [v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24]
    G.SURF = L.table_from({'w': 32, 'h': 32, 'pitch': pitch, 'bytes': G.zero_based(L.table_from(bytes_))})
    G.OUT = None
    G.unpremultiplied(5)
    want = out_words()
    got = U32(); lib.l_unpremult(c_words(w), got)
    compare('unpremultiplied %d' % k, want, list(got))
    got2 = U32(); ok = lib.l_job(1, c_words(w), got2, 0.0, 0.0, 0.0)
    if not ok: bad += 1; first.append(('unpremultiply job refused', k))
    compare('unpremultiply job %d' % k, want, list(got2))
G.SURF = None

# ---- PJ_LEAN against clump_offset and leaning, every step and the eight ways ----
WAYS = [(1, 0), (0.7071067811865476, 0.7071067811865476), (0, 1), (-0.7071067811865476, 0.7071067811865476),
        (-1, 0), (-0.7071067811865476, -0.7071067811865476), (0, -1), (0.7071067811865476, -0.7071067811865476)]
LEAN_MOST_PX, LEAN_STEPS = 6, 10
clumps_seen = set()
OFFS = ['off2.4', 'off2.9', 'off-2.9', 'off3.1', 'off-3.1', 'off3.6', 'off-2.2', 'off0.4']
for k in range(48 + 2 * len(OFFS)):
    w = pic(100 + k, ['clump', 'edge', 'edge', 'soft', 'any', 'edge'][k % 6] if k < 48 else OFFS[(k - 48) // 2])
    px = to_px(w)
    res = G.clump_offset(px)
    ox_c, oy_c = ctypes.c_int(), ctypes.c_int()
    has = lib.l_clump(c_words(w), ctypes.byref(ox_c), ctypes.byref(oy_c))
    if res is None:
        if has: bad += 1; first.append(('clump: Lua none, DLL some', k))
        continue
    ox, oy = (res[0], res[1]) if isinstance(res, tuple) else (res, None)
    clumps_seen.add((ox, oy))
    if not has or (ox_c.value, oy_c.value) != (ox, oy):
        bad += 1; first.append(('clump offset', k, (ox, oy), (ox_c.value, oy_c.value))); continue
    for step in (1, 3, 5, 7, 10):
        ux, uy = WAYS[(k + step) % 8]
        amount = LEAN_MOST_PX * step / LEAN_STEPS
        lt = G.leaning(px, ox, oy, amount, ux, uy)
        want = [lt[i + 1] & 0xffffffff for i in range(1024)]
        got = U32(); lib.l_lean(c_words(w), got, ctypes.c_double(amount), ctypes.c_double(ux), ctypes.c_double(uy))
        compare('lean %d step %d' % (k, step), want, list(got))
        got2 = U32(); lib.l_job(2, c_words(w), got2, ctypes.c_double(amount), ctypes.c_double(ux), ctypes.c_double(uy))
        compare('lean job %d step %d' % (k, step), want, list(got2))
# a picture with no pixels: Lua makes no frames (clump_offset nil); the DLL's frame is all clear
w = pic(1, 'clear')
got = U32(); lib.l_lean(c_words(w), got, ctypes.c_double(3.0), ctypes.c_double(1.0), ctypes.c_double(0.0))
if G.clump_offset(to_px(w)) is not None or any(got): bad += 1; first.append(('clear picture',))

# ---- THE SNOW'S BAKES: bake_fade, snow_with, snowy_overhang ----
SIDES = {1: 'S', 2: 'W', 4: 'E', 8: 'N'}
for k in range(30):
    thin, full = pic(200 + k, 'soft'), pic(300 + k, 'any')
    sides = 1 + k % 15
    G.PIX = L.table_from({11: to_px(thin), 12: to_px(full)})
    G.thin_faded = L.table()
    fs = L.table_from({SIDES[b]: True for b in SIDES if sides & b})
    G.OUT = None
    G.bake_fade(11, 12, fs, L.table_from({'budget': float('inf')}))
    want = out_words()
    got = U32(); lib.l_fade(c_words(thin), c_words(full), sides, got)
    compare('fade %d sides %d' % (k, sides), want, list(got))
for k in range(30):
    base, over = pic(400 + k, ['soft', 'any', 'clump'][k % 3]), pic(500 + k, ['soft', 'any', 'clear', 'edge'][k % 4])
    G.PIX = L.table_from({21: to_px(base), 22: to_px(over)})
    G.with_cache = L.table()
    G.OUT = None
    G.snow_with(21, 22, L.table_from({'budget': float('inf')}))
    want = out_words()
    got = U32(); lib.l_with(c_words(base), c_words(over), got)
    compare('with %d' % k, want, list(got))
for k in range(30):
    n = 1 + k % 8
    pieces = [pic(600 + k * 8 + j, ['edge', 'soft', 'edge', 'any'][j % 4]) for j in range(n)]
    fill = pic(700 + k, 'clump')
    pix = {31 + j: to_px(pieces[j]) for j in range(n)}
    pix[30] = to_px(fill)
    G.PIX = L.table_from(pix)
    G.pieces_for = L.table_from({'NSWE': L.table_from([30])})
    G.snowy_overhangs = L.table()
    G.OUT = None
    G.snowy_overhang('k%d' % k, L.table_from([31 + j for j in range(n)]), L.table_from({'budget': float('inf')}))
    want = out_words()
    flat = (ctypes.c_uint32 * (1024 * n))(*[v for p in pieces for v in p])
    got = U32(); lib.l_edge(flat, n, c_words(fill), got)
    compare('edge %d (%d pieces)' % (k, n), want, list(got))

print('clump offsets seen:', sorted(clumps_seen))
flow = lib.l_bake_flow()
print('cases %d pixels %d differing cases %d; bake flow failures %d' % (cases, pixels, bad, flow))
print('first differing:', first)
