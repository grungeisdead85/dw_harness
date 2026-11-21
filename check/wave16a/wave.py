# 16A, THE GRASS WAVE DRAWN HERE: the DLL's draw_grass_wave set against colour.lua's own grass_wave.
# Two runs over the same frames, each in its own lupa (Lua 5.3) with stand-ins and its own ground layer:
#   ref: grass_wave as 16A drop 1 ran it with the DLL (its own loop over the cells, rustle frames sent
#        as PJ_RUSTLE jobs): native_wave stood in by one that always says no;
#   nat: grass_wave with the real native_wave, native_wave_write and native_wave_frame (colour.lua) and
#        the real colour_view (native.lua), over the DLL's Block in lib_wave.so; the DLL draws.
# Every cell of both layers must match after every frame, through a change of wind (another kind of
# rustle), a new set of frames (another wind direction) and wind too light to rustle. Rustle frames get
# numbers from their source and phase, so both runs make the same ones.
# Usage: python3 wave.py <folder with colour.lua and native.lua> [lib_wave.so]
import sys, re, ctypes, math
import lupa.lua53 as lupa

folder = sys.argv[1]
src = open(folder + '/colour.lua').read()
nsrc = open(folder + '/native.lua').read()
lib = ctypes.CDLL(sys.argv[2] if len(sys.argv) > 2 else './lib_wave.so')
lib.l_block.restype = ctypes.c_uint64
BASE = lib.l_block()

CT = {'uint8_t': ctypes.c_uint8, 'int8_t': ctypes.c_int8, 'uint16_t': ctypes.c_uint16, 'int16_t': ctypes.c_int16,
      'uint32_t': ctypes.c_uint32, 'int32_t': ctypes.c_int32, 'uint64_t': ctypes.c_uint64, 'int64_t': ctypes.c_int64,
      'double': ctypes.c_double}
def PEEK(t, addr): return CT[t].from_address(addr).value
def POKE(t, addr, v):
    CT[t].from_address(addr).value = float(v) if t == 'double' else int(v)

DX, DY = 44, 27                 # the port's size (the view's columns and rows: 40 by 25, less)
VX1, VY1, COLS, ROWS = 100, 50, 40, 25
bgL = (ctypes.c_int32 * (DX * DY))()
bgN = (ctypes.c_int32 * (DX * DY))()
lib.l_wave_init(DX, DY, bgN)

def grab(text, name):
    m = re.search(r'^local function ' + re.escape(name) + r'\(.*?^end$', text, re.S | re.M)
    assert m, name
    return m.group(0).replace('local function ', 'function ', 1)
def grab_global(text, name):
    m = re.search(r'^function ' + re.escape(name) + r'\(.*?^end$', text, re.S | re.M)
    assert m, name
    return m.group(0)
def const(text, name):
    return re.search(r'^local ' + name + r' = [^\n]*', text, re.M).group(0).replace('local ', '', 1)

COMMON = '''
SIZES = {uint8_t = 1, int8_t = 1, uint16_t = 2, int16_t = 2, uint32_t = 4, int32_t = 4, uint64_t = 8, int64_t = 8, double = 8}
function REF(t, addr)
    local size = SIZES[t]
    return setmetatable({}, {
        __index = function(_, k) if k == 'value' then return PEEK(t, addr) end return PEEK(t, addr + k * size) end,
        __newindex = function(_, k, v) if k == 'value' then POKE(t, addr, v) else POKE(t, addr + k * size, v) end end})
end
TICK = 0
dfhack = {getTickCount = function() return TICK end}
df = {global = {pause_state = false, gps = {main_viewport = {dim_x = DX, dim_y = DY}}},
      reinterpret_cast = function(t, addr) return REF(t, addr) end}
function peek(t, addr) return PEEK(t, addr) end
STRENGTH = 1.0
wind = {get = function() return nil, nil, STRENGTH end}
wave_mode, rustle_grass_mode = 'on', 'on'
frame_number, lean_frame = 0, {}
lean_state = {clock = 0}
lean_history = {}
DT = 0.4
function AMOUNT(c) return 0.5 + 0.45 * math.sin(c * 1.7) end
function wanted_lean()
    lean_state.clock = lean_state.clock + DT
    return AMOUNT(lean_state.clock), 1, 0, 0
end
function discard() end
native_colour = true
made_n = 0
function native_rustle(t, moves, all)
    assert(all == true, 'grass rustle: every pixel moves')
    made_n = made_n + 1
    return {handle = 1, texpos = 200000 + t * 16 + moves.phase, native = true}
end
function rustle_moves(phase, kind, sway) return {phase = phase} end
function native_room(n, kind) assert(kind == 3) return true end
'''

def runtime(bg, native):
    L = lupa.LuaRuntime()
    g = L.globals()
    g.PEEK, g.POKE, g.DX, g.DY = PEEK, POKE, DX, DY
    L.execute(COMMON)
    L.execute('df.global.gps.main_viewport.screentexpos_background = REF("int32_t", %d)' % ctypes.addressof(bg))
    for c in ('LEAN_STEPS', 'LEAN_BREEZE_FROM', 'LEAN_STRONG_FROM', 'LEAN_GALE_FROM', 'LEAN_BREEZE_MOST',
              'LEAN_HISTORY_S', 'RUSTLE_PHASES', 'WAVE_PHASES_PER_TILE', 'WAVE_TILES_PER_S', 'GRASS_RUSTLE_MS'):
        L.execute(const(src, c))
    L.execute(re.search(r'^local RUSTLE = \{.*?^\}', src, re.S | re.M).group(0).replace('local ', '', 1))
    L.execute(grab(src, 'lean_now'))
    L.execute(grab(src, 'lean_ago'))
    L.execute(grab(src, 'grass_wave'))
    if native:
        # native.lua: its offsets and its colour_view, over the DLL's Block
        L.execute(re.search(r'^local OFF = \{.*?^\}', nsrc, re.S | re.M).group(0).replace('local ', '', 1))
        for c in ('PAINT_RING', 'COL_RING'):
            L.execute(const(nsrc, c))
        L.execute(grab(nsrc, 'colour_view'))
        L.execute('native_view = colour_view(%d)' % BASE)
        L.execute(re.search(r'^local CF = \{[^\n]*', src, re.M).group(0).replace('local ', '', 1))
        L.execute(grab(src, 'native_flags'))
        for c in ('WAVE_SLOT_INTS', 'WAVE_HIST', 'WAVE_WANT_RING'):
            L.execute(const(src, c))
        L.execute(grab(src, 'native_wave_frame'))
        L.execute(grab(src, 'native_wave_write'))
        L.execute(grab_global(src, 'native_wave'))
    else:
        L.execute('function native_wave() return false end')
    return L

# The grass pictures: originals 21 to 27, each a copy and lean frames 1 to 10 (26: no frames; 27: no copy;
# 24: frames 1 to 6 only). `base` keeps one set's numbers apart from another's.
SET_LUA = '''
function make_set(base, ux, uy)
    local copies, frames = {}, {}
    for o = 21, 27 do
        if o ~= 27 then copies[o] = {texpos = base + o * 100} end
        if o ~= 26 then
            frames[o] = {}
            for s = 1, (o == 24 and 6 or 10) do frames[o][s] = {texpos = base + o * 100 + s} end
        end
    end
    local grass = {}
    for o = 21, 27 do grass[#grass + 1] = o end
    return {key = 'K:' .. base, look = 'K', ux = ux, uy = uy, grass = grass, frames = frames,
            position = #grass + 1, shown = 0, original = {}}, copies
end
function use_set(base, ux, uy)
    local l, copies = make_set(base, ux, uy)
    state = {lean = l, copies = {K = copies}, shown = 'K', lean_turning = false}
end
-- what the game draws in a cell: the tables' texture for its grass (the middle lean, MID), or other things
MID = 3
function drawn(sx, sy)
    local h = (sx * 7 + sy * 13 + (sx * sy) % 5) % 10
    if h == 0 then return 0 end
    if h == 1 then return 5000 + sx end
    local o = 21 + (sx * 3 + sy) % 7
    local l, copies = state.lean, state.copies.K
    local f = MID > 0 and l.frames[o] and l.frames[o][MID]
    if f then return f.texpos end
    return copies[o] and copies[o].texpos or 6000
end
VIEW = {x1 = VX1, y1 = VY1, x2 = VX1 + COLS - 1, y2 = VY1 + ROWS - 1}
function frame(bgref)
    TICK = TICK + 16
    frame_number = frame_number + 1
    for sx = 0, DX - 1 do for sy = 0, DY - 1 do bgref[sx * DY + sy] = drawn(sx, sy) end end
    lean_now()
    grass_wave(VIEW)
end
'''
ref = runtime(bgL, False)
nat = runtime(bgN, True)
for L, bg in ((ref, bgL), (nat, bgN)):
    g = L.globals()
    g.VX1, g.VY1, g.COLS, g.ROWS = VX1, VY1, COLS, ROWS
    L.execute(SET_LUA)
    L.execute('BG = REF("int32_t", %d)' % ctypes.addressof(bg))
    L.execute('use_set(10000, 1, 0)')

def nv_peek(name, t='uint32_t'):
    off = int(re.search(r'\b' + name + r' = (\d+)', nsrc).group(1))
    return PEEK(t, BASE + off)

failures = 0
def check(cond, what):
    global failures
    if not cond:
        failures += 1
        print('FAIL:', what)

drawn_by_dll = 0
def step(n, label, expect_native=None):
    global drawn_by_dll
    for k in range(n):
        before = nv_peek('wave_frames', 'int64_t')
        ref.execute('frame(BG)')
        nat.execute('frame(BG)')
        lib.l_wave_draw()
        after = nv_peek('wave_frames', 'int64_t')
        native = after > before
        drawn_by_dll += native
        diff = [i for i in range(DX * DY) if bgL[i] != bgN[i]]
        check(not diff, '%s frame %d: %d cells differ, first %s' % (label, k + 1, len(diff),
              [(i // DY, i % DY, bgL[i], bgN[i]) for i in diff[:4]]))
        if expect_native is not None:
            want = expect_native(k)
            check(native == want, '%s frame %d: drawn by the DLL %s, wanted %s' % (label, k + 1, native, want))

# 1. strong wind from the west: the first frame drawn in Lua (the DLL emptying its tables), then the DLL
step(12, 'strong, east', lambda k: k > 0)
changed = sum(1 for i in range(DX * DY) if bgN[i] != nat.eval('drawn(%d, %d)' % (i // DY, i % DY)))
check(changed > 100, 'the wave changed only %d cells' % changed)
rustled = sum(1 for i in range(DX * DY) if bgN[i] >= 200000)
check(rustled > 100, 'only %d cells rustling' % rustled)
outside = [i for i in range(DX * DY) if (i // DY >= COLS or i % DY >= ROWS) and bgN[i] != nat.eval('drawn(%d, %d)' % (i // DY, i % DY))]
check(not outside, 'cells outside the view changed: %d' % len(outside))
check(nv_peek('wave_wants') > 0, 'no rustle frames asked of Lua')
# the frames handed over count as their originals (a copy drawn through the tables)
flags = PEEK('uint64_t', BASE + int(re.search(r'\bcol_flags = (\d+)', nsrc).group(1)))
link = PEEK('uint64_t', BASE + int(re.search(r'\bcol_link = (\d+)', nsrc).group(1)))
for tp, orig in ((10000 + 21 * 100 + 5, 21), (10000 + 21 * 100, 21), (10000 + 24 * 100 + 6, 24), (10000 + 27 * 100 + 10, 27)):
    check(PEEK('uint8_t', flags + tp) & 16 and PEEK('int32_t', link + tp * 4) == orig, 'lean frame %d not filed as %d' % (tp, orig))
# (26 has no frames, so grass_wave's step_of has none of it, its copy included: never waved, never filed here)
check(not PEEK('uint8_t', flags + 10000 + 26 * 100) & 16, 'a picture without frames filed')
rf = [bgN[i] for i in range(DX * DY) if bgN[i] >= 200000][:3]
for tp in rf:
    check(PEEK('uint8_t', flags + tp) & 16 and PEEK('int32_t', link + tp * 4) in range(21, 28), 'rustle frame %d not filed' % tp)
# 2. a gale: another kind of rustle, its frames all new: one frame in Lua, then the DLL
for L in (ref, nat): L.execute('STRENGTH = 1.5')
step(8, 'gale', lambda k: k > 0)
# 3. a new set (the wind turned to the south-east), shorter frames between gusts
for L in (ref, nat): L.execute('use_set(20000, 0.7071067811865476, 0.7071067811865476) DT = 0.1')
step(10, 'gale, south-east', lambda k: k > 0)
# 4. a breeze too light to rustle (lean only), with the middle lean the copy
for L in (ref, nat): L.execute('STRENGTH = 0.2 MID = 0')
step(6, 'calm lean', lambda k: k > 0)
# 5. paused: the rustle's clock stops, the gusts' history still sent
for L in (ref, nat): L.execute('STRENGTH = 1.0 df.global.pause_state = true')
step(6, 'paused', lambda k: k > 0)
# 6. the wave turned off: neither draws
for L in (ref, nat): L.execute('wave_mode = "off"')
step(2, 'off', lambda k: False)
for L in (ref, nat): L.execute('wave_mode = "on" df.global.pause_state = false')
step(4, 'back on', lambda k: True)
hist_head = nv_peek('wave_hist_head')
n_hist = nat.eval('#lean_history')
check(hist_head >= n_hist, 'history entries sent %d, kept %d' % (hist_head, n_hist))
print('grass wave: %d frames drawn by the DLL, %d rustle frames asked of Lua; history sent %d, kept in Lua %d'
      % (drawn_by_dll, nv_peek('wave_wants'), hist_head, n_hist))
# the DLL's time on a big view: 200 by 120 cells, three in four of them grass (-O1 here; MSVC builds /O2)
BX, BY = 200, 120
big = (ctypes.c_int32 * (BX * BY))()
nat.execute('STRENGTH = 1.0')
pattern = [nat.eval('drawn(%d, %d)' % (x % DX, y % DY)) for x in range(BX) for y in range(BY)]
saved = (ctypes.c_int32 * (BX * BY))(*pattern)
lib.l_wave_init(BX, BY, big)
offs = {n: int(re.search(r'\b' + n + r' = (\d+)', nsrc).group(1)) for n in ('wave_cols', 'wave_rows', 'wave_on', 'wave_us_total', 'wave_frames', 'wave_reach', 'wave_cx', 'wave_cy')}
POKE('int32_t', BASE + offs['wave_cols'], BX); POKE('int32_t', BASE + offs['wave_rows'], BY)
POKE('double', BASE + offs['wave_cx'], (BX - 1) / 2); POKE('double', BASE + offs['wave_cy'], (BY - 1) / 2)
POKE('double', BASE + offs['wave_reach'], (BX * 0.7071067811865476 + BY * 0.7071067811865476) / 2)
us0, f0 = PEEK('double', BASE + offs['wave_us_total']), PEEK('int64_t', BASE + offs['wave_frames'])
for k in range(200):
    ctypes.memmove(big, saved, ctypes.sizeof(big))
    POKE('uint32_t', BASE + offs['wave_on'], 1)
    lib.l_wave_draw()
us1, f1 = PEEK('double', BASE + offs['wave_us_total']), PEEK('int64_t', BASE + offs['wave_frames'])
print('the DLL on a 200 by 120 view: %.1f us a frame (%d frames)' % ((us1 - us0) / max(1, f1 - f0), f1 - f0))
if failures:
    print('%d checks FAILED' % failures)
    sys.exit(1)
print('16A grass wave: all checks pass')
