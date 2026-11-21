# THE SPATTER DRAWN HERE (test 15Y): the DLL's spatter painting (spat_make_piece, spat_make_blades) against
# spatter.lua's own (mask_for and bake_spatter; blade_mask and bake_blades), run in lupa on the real file,
# pixel for pixel: pictures with solid pieces, soft edges, see-through gaps and grass of every brightness,
# at every share (0.05, 0.1 .. 1) and pattern, the weather's copy and its original apart.
# Usage: python3 compare.py path/to/spatter.lua
import sys, ctypes, random, lupa
lib = ctypes.CDLL('./spatlib.so')
U32 = ctypes.c_uint32 * 1024
src = open(sys.argv[1]).read()
L = lupa.LuaRuntime()
L.execute('''
PIX = {}
local grass = {pixels_of = function(t) return PIX[t] end, discard = function() end}
reqscript = function(name) if name:find('grass') then return grass end return {} end
dfhack = {textures = {}, maps = {}, internal = {}}
''')
chunk = src + '\nreturn {mask_for = mask_for, bake_spatter = bake_spatter, blade_mask = blade_mask, bake_blades = bake_blades}\n'
F = L.execute(chunk)
def to_lua(words):
    t = L.table()
    for i, v in enumerate(words):
        t[i + 1] = L.table(v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24)
    return t
rng = random.Random(7)
def spatter_pic(seed):
    r = random.Random(seed)
    w = [0] * 1024
    for _ in range(r.randint(3, 14)):   # blobs
        cx, cy, rad = r.randint(0, 31), r.randint(0, 31), r.randint(1, 4)
        col = (r.randint(0, 255), r.randint(0, 255), r.randint(0, 255))
        for y in range(32):
            for x in range(32):
                d2 = (x - cx) ** 2 + (y - cy) ** 2
                if d2 <= rad * rad:
                    w[y * 32 + x] = (255 << 24) | (col[2] << 16) | (col[1] << 8) | col[0]
                elif d2 <= (rad + 1) ** 2 and (w[y * 32 + x] >> 24) == 0:
                    w[y * 32 + x] = (r.randint(1, 127) << 24) | (col[2] << 16) | (col[1] << 8) | col[0]   # soft edge
    return w
def dots_pic(seed):   # many small separate pieces (so each share and pattern keeps or drops a lot of them)
    r = random.Random(seed)
    w = [0] * 1024
    for y in range(0, 32, 3):
        for x in range(0, 32, 3):
            if r.random() < 0.8:
                w[y * 32 + x] = (255 << 24) | (r.randint(0, 255) << 16) | (r.randint(0, 255) << 8) | r.randint(0, 255)
                if x + 1 < 32 and r.random() < 0.5: w[y * 32 + x + 1] = (r.randint(1, 127) << 24) | 0x334455
    return w
def recolour(w, seed):
    r = random.Random(seed)
    out = []
    for v in w:
        a = v >> 24
        out.append((a << 24) | (r.randint(0, 255) << 16) | (r.randint(0, 255) << 8) | r.randint(0, 255) if a else v)
    return out
def grass_pic(seed):
    r = random.Random(seed)
    return [((r.choice([255, 255, 255, 200, 0])) << 24) | (r.randint(0, 255) << 16) | (r.randint(0, 255) << 8) | r.randint(0, 255) for _ in range(1024)]
bad = cases = pixels = 0
first = None
for k in range(90):
    orig = spatter_pic(k) if k < 60 else dots_pic(k)
    copy = orig if k % 3 == 0 else recolour(orig, 1000 + k)
    oid, cid = 2 * k + 1, 2 * k + 2
    L.globals().PIX[oid] = to_lua(orig)
    L.globals().PIX[cid] = to_lua(copy)
    for share in [0.05] + [s / 10 for s in range(1, 11)]:
        for variant in range(4):
            mask = F.mask_for(oid, share, variant)
            lua = F.bake_spatter(cid, mask)
            out = U32()
            lib.sl_piece(U32(*copy), U32(*orig), ctypes.c_double(share), variant, out)
            cases += 1
            for i in range(1024):
                pixels += 1
                lv = lua[i + 1] & 0xffffffff
                if lv != out[i]:
                    bad += 1
                    if first is None: first = ('piece', k, share, variant, i, hex(lv), hex(out[i]))
for k in range(60):
    orig = grass_pic(500 + k)
    shown = orig if k % 2 == 0 else recolour(orig, 900 + k)
    oid, gid = 4001 + 2 * k, 4002 + 2 * k
    L.globals().PIX[oid] = to_lua(orig)
    L.globals().PIX[gid] = to_lua(shown)
    mask = F.blade_mask(oid)
    lua = F.bake_blades(gid, mask)
    out = U32()
    lib.sl_blades(U32(*shown), U32(*orig), out)
    cases += 1
    for i in range(1024):
        pixels += 1
        lv = lua[i + 1] & 0xffffffff
        if lv != out[i]:
            bad += 1
            if first is None: first = ('blades', k, i, hex(lv), hex(out[i]))
print('spatter painting: cases %d, pixels %d, differing %d' % (cases, pixels, bad))
print('first differing:', first)
