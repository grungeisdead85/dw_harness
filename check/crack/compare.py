# Runs frost.lua's own cracked() (cut from the real file) in Lua with stand-ins for DFHack, and the
# DLL's crack_in_place (crackcheck), on the same cases; reports any pixel that differs.
# Usage: python3 compare.py <frost.lua> <art folder>
import sys, struct, subprocess, random, re
import lupa
from PIL import Image
frost_src = open(sys.argv[1]).read()
art = sys.argv[2]
# the function and the constants it reads, cut from the file
def grab(pattern):
    m = re.search(pattern, frost_src, re.S)
    assert m, pattern
    return m.group(0)
consts = grab(r'local CRACK_DARK, CRACK_LIT, PLATE_PALE = [^\n]*\n') + grab(r'local CRACK_THIN = [^\n]*\n') \
       + grab(r'local STONE_CRACK_DARK = [^\n]*\n') + grab(r'local HEAT_TILES = [^\n]*\n')
helpers = grab(r'local function pack\(r, g, b, a\)[^\n]*\n') + grab(r'local function round_byte\(v\)[^\n]*\n')
cracked = grab(r'local function cracked\(px, kind, amount, piece\).*?\n    return packed\nend\n')
# the heat patterns, 16 pieces of 32 x 32 (loadTileset's order: left to right, top to bottom)
pat_img = Image.open(art + '/heat_patterns.png').convert('RGBA')
W, H = pat_img.size; P = pat_img.load()
pieces = []
for ty in range(H // 32):
    for tx in range(W // 32):
        pieces.append([P[tx*32 + x, ty*32 + y] for y in range(32) for x in range(32)])
assert len(pieces) >= 16, len(pieces)
L = lupa.LuaRuntime()
L.execute('TILE = 32\n' + consts + helpers + '''
patterns = {}
function set_piece(piece, main, fine) patterns[piece] = {main = main, fine = fine} end
local function heat_patterns() return patterns end
''' + cracked + '''
function run(px, kind, amount, piece) return cracked(px, kind, amount, piece) end
''')
for k in range(16):
    main = L.table_from([p[0] / 16 for p in pieces[k]]); fine = L.table_from([p[1] / 16 for p in pieces[k]])
    L.globals().set_piece(k, main, fine)
# the cases: real ground (heat_ground.png's tiles) and noise with see-through pixels
ground = Image.open(art + '/heat_ground.png').convert('RGBA'); GW, GH = ground.size; G = ground.load()
random.seed(15)
tiles = []
for _ in range(40):
    tx, ty = random.randrange(GW // 32), random.randrange(GH // 32)
    tiles.append([G[tx*32 + x, ty*32 + y] for y in range(32) for x in range(32)])
for _ in range(10):
    tiles.append([(random.randrange(256), random.randrange(256), random.randrange(256), random.choice([0, 255, 128, 255])) for _ in range(1024)])
cases = []
for t in tiles:
    for kind in ('dirt', 'stone', 'pebbles'):
        for amount in (0.05, 0.25, 0.55, 0.8, 1.0, random.random()):
            cases.append((t, kind, amount, random.randrange(16)))
KIND = {'dirt': 1, 'stone': 2, 'pebbles': 3}
def word(p): return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)
blob = bytearray()
for t, kind, amount, piece in cases:
    blob += struct.pack('<Id', KIND[kind], amount)
    blob += struct.pack('<1024I', *[word(p) for p in pieces[piece]])
    blob += struct.pack('<1024I', *[word(p) for p in pieces[(piece - 4) % 16]])
    blob += struct.pack('<1024I', *[word(p) for p in t])
out = subprocess.run(['./crackcheck'], input=bytes(blob), capture_output=True).stdout
assert len(out) == len(cases) * 4096, (len(out), len(cases))
bad = 0; first = None
for c, (t, kind, amount, piece) in enumerate(cases):
    px = L.table_from([L.table_from(list(p)) for p in t])
    lua = L.globals().run(px, kind, amount, piece)
    cw = struct.unpack_from('<1024I', out, c * 4096)
    for n in range(1024):
        if lua[n + 1] != cw[n]:
            bad += 1
            if first is None: first = (c, kind, amount, piece, n, hex(lua[n + 1]), hex(cw[n]))
print(f'cases {len(cases)} ({len(tiles)} textures x 3 kinds x 6 amounts), pixels {len(cases) * 1024}, differing {bad}')
print('first differing:', first)
