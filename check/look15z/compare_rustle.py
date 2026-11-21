# 16A: THE RUSTLE PAINTED HERE. colour.lua's own native_rustle (cut from the real file, run in lupa, Lua 5.3)
# writes each job into memory the DLL then paints (lib15z.so: paint_one), and every frame is set against
# colour.lua's own rustle_frame, pixel for pixel: trees (leafy pixels only) and grass (every pixel), every
# phase, the three winds and the eight ways, leafy, wood and mixed pictures; and the DLL's leaves test
# against is_leaves. Usage: python3 compare_rustle.py path/to/colour.lua path/to/native.lua
import sys, ctypes, random, re
import lupa.lua53 as lupa
lib = ctypes.CDLL('./lib15z.so')
lib.l_block.restype = ctypes.c_uint64
lib.l_paint_job.argtypes = [ctypes.c_uint64]
U32 = ctypes.c_uint32 * 1024
colour_src, native_src = open(sys.argv[1]).read(), open(sys.argv[2]).read()
off_table = native_src[native_src.index('local OFF = {'):native_src.index('\n}', native_src.index('local OFF = {'))]
OFF = {k: int(v) for k, v in re.findall(r'\b(\w+) = (\d+)', off_table)}
base = lib.l_block()

def grab(name):
    m = re.search(r'^(local )?function ' + re.escape(name) + r'\(.*?^end$', colour_src, re.S | re.M)
    if not m: raise SystemExit('no function ' + name)
    return re.sub(r'^local ', '', m.group(0))
def const(name):
    m = re.search(r'^local ' + re.escape(name) + r' = [^\n]*', colour_src, re.M)
    if not m: raise SystemExit('no constant ' + name)
    return re.sub(r'^local ', '', m.group(0))

CT = {'uint32_t': ctypes.c_uint32, 'int32_t': ctypes.c_int32, 'uint8_t': ctypes.c_uint8, 'double': ctypes.c_double,
      'uint64_t': ctypes.c_uint64}
class View:   # df.reinterpret_cast(t, a): .value is element 0, [i] element i
    def __init__(s, t, a): s.t, s.a, s.n = CT[t], int(a), ctypes.sizeof(CT[t])
    def __getitem__(s, k): return s.t.from_address(s.a + (0 if k == 'value' else k) * s.n).value
    def __setitem__(s, k, v): s.t.from_address(s.a + (0 if k == 'value' else k) * s.n).value = int(v) if s.t is not ctypes.c_double else v

L = lupa.LuaRuntime()
g = L.globals()
g.py_cast = lambda t, a: View(t, a)
L.execute('''
df = {reinterpret_cast = function(t, a) return py_cast(t, a) end}
ADDR = {}
grass = {original_address = function(t) return ADDR[t] end}
local seed_state = 0
dfhack = {random = {new = function(seed)
    local s = seed
    return {drandom = function(self) s = (s * 1103515245 + 12345) % 2147483648 return s / 2147483648 end}
end}}
clumps = nil
native_counts, native_pool, frame_number = {}, {}, 1
''')
for c in ('TILE', 'RUSTLE_PHASES', 'LEAF_SHARE', 'LEAF_SATURATION', 'OUTLINE_VALUE', 'CLUMP_SEEDS',
          'WAVE_PHASES_PER_TILE', 'PAINT_JOB_BYTES'):
    L.execute(const(c))
L.execute(re.search(r'^local RUSTLE = \{.*?^\}', colour_src, re.S | re.M).group(0).replace('local ', '', 1))
for f in ('clump_layout', 'pack', 'leafy', 'is_leaves', 'rustle_moves', 'rustle_frame'): L.execute(grab(f))
a = colour_src.index('local NATIVE_LEANS_PER_FRAME')
b = colour_src.index('\nend\n', colour_src.index('function native_rustle(')) + 5
L.execute(colour_src[a:b])
L.execute('function dfhack.textures_stub() end')
g.dfhack.textures = L.table_from({'createTile': L.eval('function(px, w, h) OUT = px return 1 end'),
                                  'getTexposByHandle': L.eval('function(h) return 9 end')})

# the job ring, the blanks and the pictures in ctypes memory
RING = 4
jobs = (ctypes.c_uint8 * (256 * RING))()
head = ctypes.c_uint32(0); tail = ctypes.c_uint32(0)
blanks = [U32() for _ in range(64)]
L.execute('native_view = {}')
nv = g.native_view
nv.paint_head = View('uint32_t', ctypes.addressof(head)); nv.paint_tail = View('uint32_t', ctypes.addressof(tail))
nv.paint_base = ctypes.addressof(jobs); nv.paint_ring = RING
nv.rustle_of = View('uint8_t', base + OFF['rustle_of']); nv.rustle_of_ready = View('uint32_t', base + OFF['rustle_of_ready'])

def pic(seed, kind):
    r = random.Random(seed)
    w = []
    for i in range(1024):
        a = r.choice([0, 255, 255, 255, r.randint(1, 254)]) if kind != 'solid' else 255
        if kind == 'leafy':   # saturated greens, dark outlines, some grey
            c = r.choice([(40, 160, 40), (90, 200, 60), (20, 30, 20), (120, 120, 110), (200, 220, 90)])
        elif kind == 'wood':
            c = r.choice([(110, 90, 70), (140, 120, 100), (90, 85, 80), (60, 50, 45)])
        else:
            c = (r.randint(0, 255), r.randint(0, 255), r.randint(0, 255))
        c = tuple(max(0, min(255, v + r.randint(-12, 12))) for v in c)
        w.append((a << 24) | (c[2] << 16) | (c[1] << 8) | c[0])
    return w
def to_px(words):
    t = L.table()
    for i, v in enumerate(words): t[i + 1] = L.table(v & 255, (v >> 8) & 255, (v >> 16) & 255, v >> 24)
    return t

WAYS = [(1, 0), (0.7071067811865476, 0.7071067811865476), (0, 1), (-0.7071067811865476, 0.7071067811865476),
        (-1, 0), (-0.7071067811865476, -0.7071067811865476), (0, -1), (0.7071067811865476, -0.7071067811865476)]
cases = bad = 0
changed = 0
first = []
leaves_bad = 0
for k in range(36):
    kind = ['leafy', 'wood', 'any', 'solid'][k % 4]
    words = pic(900 + k, kind)
    src = U32(*words)
    g.ADDR[50 + k] = ctypes.addressof(src)
    px = to_px(words)
    if bool(g.is_leaves(px)) != bool(lib.l_is_leaves(src)): leaves_bad += 1
    for wind in ('breeze', 'strong', 'gale'):
        for w, (ux, uy) in enumerate(WAYS):
            if (k + w) % 3: continue   # (a third of the ways for each picture: every way over the pictures)
            sway = L.table_from({'ux': ux, 'uy': uy})
            for phase in range(16):
                for all_ in (False, True):
                    g.OUT = None
                    if all_: g.rustle_frame(px, phase, wind, sway, L.eval('function(p) return true end'))
                    else: g.rustle_frame(px, phase, wind, sway)
                    want = [g.OUT[i + 1] & 0xffffffff for i in range(1024)]
                    blank = blanks[(cases) % 64]
                    g.native_pool[1] = L.table_from({'handle': 1, 'texpos': 7000 + cases, 'address': ctypes.addressof(blank)})
                    g.native_counts.pixel_frame = None   # (a new frame for each: the share never limits here)
                    tail.value = head.value
                    f = g.native_rustle(50 + k, g.rustle_moves(phase, wind, sway), all_)
                    if not f:
                        bad += 1; first.append(('no job', k, wind, phase)); continue
                    job = ctypes.addressof(jobs) + ((head.value - 1) % RING) * 256
                    if not lib.l_paint_job(job): bad += 1; first.append(('refused', k)); continue
                    got = list(blank)
                    cases += 1
                    if want != words: changed += 1
                    if got != want:
                        bad += 1
                        if len(first) < 5:
                            d = [i for i in range(1024) if got[i] != want[i]]
                            first.append((kind, wind, w, phase, all_, len(d), hex(want[d[0]]), hex(got[d[0]])))
# the leaves test near its threshold: pictures with a set share of leaf-coloured opaque pixels
SPLITS = [(400 + k * 37, round((400 + k * 37) * s)) for k, s in enumerate([0.2, 0.3, 0.34, 0.349, 0.351, 0.36, 0.4, 0.5, 0.0, 1.0])]
SPLITS += [(400, 140), (200, 70), (20, 7), (400, 139), (400, 141)]   # (exactly at LEAF_SHARE, and either side)
for k, (opaque, leafs) in enumerate(SPLITS):
    r = random.Random(77 + k)
    w = []
    for i in range(1024):
        if i < leafs: c, a = (40, 170, 50), 255
        elif i < opaque: c, a = (120, 110, 100), r.choice([129, 200, 255])
        else: c, a = (r.randint(0, 255), r.randint(0, 255), r.randint(0, 255)), r.choice([0, 100, 128])
        w.append((a << 24) | (c[2] << 16) | (c[1] << 8) | c[0])
    if bool(g.is_leaves(to_px(w))) != bool(lib.l_is_leaves(U32(*w))): leaves_bad += 1
print('rustle frames: cases %d (%d with pixels moved), pixels %d, differing %d; leaves test differing %d' % (cases, changed, cases * 1024, bad, leaves_bad))
print('first differing:', first)
