# place.py: frost.lua's NATIVE CRACKS (its own Lua, through lupa) against the DLL's CRACKS IN PLACE (the
# DLL's own code, built over a fake DF with a map, surfaces, heat patterns and blanks: placelib.cpp),
# tests 15R to 15V. The views native.lua hands frost.lua (crack_view) are made here the same way, over the
# block: OFF read from native.lua's own OFF table and checked against the DLL's struct first. Each look's
# numbers are written by colour.lua's own native_template (cut from the real file) from the real
# looks.lua's native_numbers, and a ramp's copy painted in the hook is checked pixel for pixel against the
# Lua's own: looks.lua's redraw, then the cracks (the DLL's crack_in_place, itself checked bit for bit
# against frost.lua's cracked() in check/crack).
#   python3 place.py frost.lua native.lua colour.lua looks.lua
import ctypes, re, sys
import lupa.lua53 as lupa53

frost_path, native_path, colour_path, looks_path = sys.argv[1:5]
lib = ctypes.CDLL('./placelib.so')
lib.pl_block.restype = ctypes.c_uint64
lib.pl_off.restype = ctypes.c_int64
lib.pl_heat_addr.restype = ctypes.c_uint64
lib.pl_pixel.restype = ctypes.c_uint32
lib.pl_copy_pixel.restype = ctypes.c_uint32
lib.pl_crack.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint32, ctypes.c_double, ctypes.c_int]
lib.pl_init()
base = lib.pl_block()

# ---- OFF and the rings' sizes, from native.lua ----
native = open(native_path).read()
off_table = native[native.index('local OFF = {'):native.index('\n}', native.index('local OFF = {'))]   # (the OFF table alone)
OFF = {k: int(v) for k, v in re.findall(r'\b((?:crack_\w+|off_temperature)) = (\d+)', off_table)}
BLANKS = int(re.search(r'local CRACK_BLANKS = (\d+)', native).group(1))
DONE_RING = int(re.search(r'local CRACK_DONE_RING = (\d+)', native).group(1))
LOOK_BYTES, JOB_BYTES = [int(v) for v in re.search(r'local CRACK_LOOK_BYTES, PAINT_JOB_BYTES = (\d+), (\d+)', native).groups()]
FLOORS = int(re.search(r'local CRACK_FLOORS = (\d+)', native).group(1))

fails = 0
for k, v in OFF.items():   # native.lua's OFF against the DLL's struct
    real = lib.pl_off(k.encode())
    if real != v: fails += 1; print('FAIL: native.lua OFF.%s = %d, the DLL\'s %d' % (k, v, real))
print('native.lua\'s crack offsets against the DLL\'s: %d checked' % len(OFF))

CT = {'uint32_t': ctypes.c_uint32, 'int32_t': ctypes.c_int32, 'uint8_t': ctypes.c_uint8, 'double': ctypes.c_double,
      'int64_t': ctypes.c_int64, 'uint64_t': ctypes.c_uint64}
class View:      # df.reinterpret_cast(t, a): .value is element 0, [i] element i (lupa indexes it either way)
    def __init__(s, t, a): s.t, s.a, s.n = CT[t], int(a), ctypes.sizeof(CT[t])
    def __getitem__(s, k): return s.t.from_address(s.a + (0 if k == 'value' else k) * s.n).value
    def __setitem__(s, k, v): s.t.from_address(s.a + (0 if k == 'value' else k) * s.n).value = v
def at(t, name): return View(t, base + OFF[name])
def u32(name): return ctypes.c_uint32.from_address(base + OFF[name]).value
def i64(name): return ctypes.c_int64.from_address(base + OFF[name]).value
def set_(t, name, v): CT[t].from_address(base + OFF[name]).value = v

L = lupa53.LuaRuntime()
g = L.globals()
g.py_cast = lambda t, a: View(t, a)
g.py_pixel = lambda t, n: lib.pl_pixel(t, n)

# ---- the real looks.lua (native_numbers, redraw), with stand-ins for DFHack as check/paint/compare_full.py ----
L.execute(r'''
dfhack = {textures = {loadTileset = function() return {1} end, getTexposByHandle = function(h) return h end}}
grass_stub = {
    pixels_of = function(t)
        local px = {}
        for n = 1, 1024 do local v = py_pixel(t, n - 1) px[n] = {v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24} end
        return px
    end,
    original_address = function(t) return heat_address and heat_address[t] or nil end,
}
function reqscript() return grass_stub end
df = {reinterpret_cast = function(t, a) return py_cast(t, a) end}
''')
looks = L.eval('function(src) local env = setmetatable({}, {__index = _G}); local f = assert(load(src, "looks", "t", env)); f(); return env end')(open(looks_path).read())
g.looks = looks

# ---- colour.lua's paint numbers writer (paint_numbers, write_numbers, native_template), cut from the real file ----
colour_src = open(colour_path).read()
no_change = re.search(r'local NO_CHANGE = \{.*?\n.*?\}\n', colour_src, re.S).group(0)
a = colour_src.index('-- The numbers for redrawing `class` in `look` as the DLL paints them')
b = colour_src.index('local function native_job(t, class, look, crack)')
colour = L.execute(no_change + colour_src[a:b] + '\nreturn {native_template = native_template}\n')

# ---- frost.lua's NATIVE CRACKS section, with what it uses from the rest of frost.lua stubbed ----
frost = open(frost_path).read()
a = frost.index('-- NATIVE CRACKS (grungeisdead\'s copy, 2026-11-20, test 15R)\n')
a = frost.rindex('\n---', 0, a) + 1
b = frost.index('\nlocal function resting()')
b = frost.rindex('\n-- ', 0, b)
section = frost[a:b]
section = section[:section.index('local SLICE_MS')] if 'local SLICE_MS' in section else section
# the map's tiletypes, as frost.lua's shape() would read them: 1 a wall, 20 a floor, 21 a dirt ramp (SOIL),
# 22 open space, 23 a grass ramp, 24 a stone ramp, 25 the top of a ramp
stub = r'''
local CRACK_KIND_CODE = {dirt = 1, stone = 2, pebbles = 3}
local CRACK_KINDS = {dirt = true, stone = true, pebbles = true}
local MOST_DEPTH, BAND_C, COOL_FROM, WARM_FROM, HEAT_TILES = 8, 3, 10, 20, 4
local EMPTY, RAMP_TOP, RAMP, WALL, FLOOR = 'EMPTY', 'RAMP_TOP', 'RAMP', 'WALL', 'FLOOR'
local SHAPES = {[1] = WALL, [20] = FLOOR, [21] = RAMP, [22] = EMPTY, [23] = RAMP, [24] = RAMP, [25] = RAMP_TOP}
local floor_like, class_of_tiletype = {}, {}
local function shape(tt)
    local s = SHAPES[tt] or WALL
    floor_like[tt] = s == FLOOR
    class_of_tiletype[tt] = tt == 23 and 'grass' or tt == 21 and 'dirt' or 'stone'
    return s
end
local real_df = df
df = setmetatable({tiletype = setmetatable({_last_item = 30}, {__index = function(_, k) return type(k) == 'number' and 'tt' or nil end})},
                  {__index = real_df})
local counts = {}
local function count(w) counts[w] = (counts[w] or 0) + 1 end
-- grounds by original (frost.lua: ground_kind): 1000 dirt (the table copy 1100's), 1300 stone, 1400 grass
local kind_of_texpos = {[1000] = 'dirt', [1300] = 'stone', [1400] = 'grass'}
local function ground_kind(t) return kind_of_texpos[t] end
local colour = {native_template = colour_native_template, state = {refs_of = {[1000] = {}, [1300] = {}, [1400] = {}, [1600] = {}}}}
-- (FLOORS AHEAD) what the game draws for an original in each look: 1000's table copy per look; 1300 itself
local copies_by_key = {L1 = {[1000] = 1100}, L2 = {[1000] = 1102}, L3 = {[1000] = 1106}}
function colour.look_copy_of(orig, key)
    if orig == 1300 then return 1300 end
    local set = copies_by_key[key]
    return set and set[orig] or nil
end
local pending = nil
function colour.pending_look() if pending then return pending_look, pending end return nil, nil end
looks_of = {}
local function crack_amount(look) return look and look.amount or nil end
local grass = grass_stub
local function heat_pattern_handles() local h = {} for k = 1, 16 do h[k] = 1000000 + k end return h end
made = {}
local made = made
cracked_from = {}
function forget() end
'''
tail = r'''
return {frame = native_crack_frame, begin = native_crack_begin, rules = crack_rules, kinds = crack_ground_kinds,
        set_pending = function(k, look) pending, pending_look = k, look end,
        ahead_left = cracks_ahead_left, counts = counts}
'''
g.colour_native_template = colour.native_template
heat = L.table()
for k in range(16): heat[1000001 + k] = lib.pl_heat_addr(k)
g.heat_address = heat
F = L.execute(stub + section + tail)

def check(c, what):
    global fails
    if not c: fails += 1; print('FAIL:', what)

# the views, as native.lua's crack_view makes them
looks_t = L.table()
for k in range(2):
    slot = base + OFF['crack_look'] + k * LOOK_BYTES
    looks_t[k] = L.table_from({'gen': View('uint32_t', slot), 'ready': View('uint32_t', slot + 4), 'amount': View('double', slot + 8),
                               'ramp': L.table_from([slot + 16, slot + 16 + JOB_BYTES]),
                               'floors_n': View('uint32_t', slot + 528), 'floors_version': View('uint32_t', slot + 532),
                               'floor_orig': View('int32_t', slot + 536), 'floor_copy': View('int32_t', slot + 536 + 4 * FLOORS),
                               'floors_most': FLOORS})
cv = L.table_from({
    'on': at('uint32_t', 'crack_on'), 'gen': at('uint32_t', 'crack_gen'), 'gen_prev': at('uint32_t', 'crack_gen_prev'),
    'depth': at('uint32_t', 'crack_depth'),
    'forced_on': at('uint32_t', 'crack_forced_on'), 'forced_c': at('double', 'crack_forced_c'), 'tables': at('uint32_t', 'crack_tables'),
    'gen_next': at('uint32_t', 'crack_gen_next'), 'next_walked': at('uint32_t', 'crack_next_walked'),
    'next_missing': at('uint32_t', 'crack_next_missing_last'),
    'looks': looks_t, 'heat': at('uint64_t', 'crack_heat'),
    'd_head': at('uint32_t', 'crack_d_head'), 'd_tail': at('uint32_t', 'crack_d_tail'),
    'd32': at('int32_t', 'crack_d'), 'd8': at('uint8_t', 'crack_d'), 'done_ring': DONE_RING})

def look(name, temperature, wet, amount):
    return L.table_from({'k': name, 'temperature': temperature, 'wet': wet, 'amount': amount})
def frame(lk, n=1, cracking=True):
    for _ in range(n):
        F.frame(lk, lk.k, lk.amount if cracking else None, L.table_from({'baked': 0}))
        lib.pl_frame()

# ---- started: frost.lua's rules and each original's ground handed over as native.lua does ----
F.begin(cv)
rules = F.rules()
tt = rules['tiletype']
check(tt[20] == 0x40 and tt[21] == 0x81 and tt[24] == 0x82 and tt[23] == 0 and tt[22] == 0x20 and tt[25] == 0x20 and tt[1] == 0,
      'the rules by tiletype: %s' % [tt[k] for k in (1, 20, 21, 22, 23, 24, 25)])
for k in range(31): ctypes.c_uint8.from_address(base + OFF['crack_tt'] + k).value = tt[k] or 0
set_('double', 'crack_band_c', rules['band_c']); set_('double', 'crack_cool_from', rules['cool_from'])
set_('double', 'crack_warm_from', rules['warm_from']); set_('uint32_t', 'crack_depth', rules['most_depth'])
set_('uint32_t', 'crack_recheck', 600); set_('uint32_t', 'crack_tables', u32('crack_tables') + 1)
kinds = F.kinds()
ground = ctypes.c_uint64.from_address(base + OFF['crack_ground']).value
for t, k in kinds.items(): ctypes.c_uint8.from_address(ground + t).value = k
check(dict(kinds.items()) == {1000: 1, 1300: 2}, 'grounds handed over: %s' % dict(kinds.items()))
check(lib.pl_ground(1000) == 1 and lib.pl_ground(1300) == 2 and lib.pl_ground(1400) == 0, 'the DLL\'s grounds')
OUT, SUB = 1 << 16, 1 << 15
def units(c): return int(10000 + c * 9 / 5)
lib.pl_tile(2, 3, 2, 20, 0, units(30))        # a dirt floor (its picture 1100, a table copy of 1000), hot
lib.pl_tile(4, 3, 2, 21, 0, units(30))        # a dirt ramp (its own picture 1200)
lib.pl_tile(5, 3, 2, 24, 0, units(30))        # a stone ramp (1250)
lib.pl_tile(6, 3, 2, 22, 0, units(30))        # open space, and under it
lib.pl_tile(6, 3, 1, 20, OUT, units(-10))     #   a stone floor outdoors, cold (1300)
lib.pl_tile(1, 1, 2, 20, 0, units(30))        # a grass floor (1500, a table copy of 1400): no copy
lib.pl_tile(3, 3, 2, 20, 0, units(15))        # mild
lib.pl_tile(7, 3, 2, 20, 2, units(30))        # water on it
for t, seed in ((1100, 1), (1200, 2), (1250, 7), (1300, 4), (1500, 5), (1101, 6), (1103, 8), (1102, 10), (1106, 11)): lib.pl_surface(t, seed, 128, 0)
lib.pl_table_copy(1100, 1000); lib.pl_table_copy(1500, 1400)
lib.pl_blanks(64)
lib.pl_set(0, 2, 3, 1100); lib.pl_set(0, 1, 1, 1500); lib.pl_set(1, 6, 3, 1300); lib.pl_set(0, 3, 3, 1101); lib.pl_set(0, 7, 3, 1103)
lib.pl_publish(1200, 21200); lib.pl_set(0, 4, 3, 21200)     # the ramps show their copies in the look
lib.pl_publish(1250, 21250); lib.pl_set(0, 5, 3, 21250)
# ---- the first frame of a dry hot look: every copy painted in the hook, nothing left uncracked ----
L1 = look('L1', 4, 0, 0.7)
frame(L1)
check(u32('crack_waiting_last') == 0 and u32('crack_painted_last') == 4, 'the first frame: %d uncracked, %d painted' % (u32('crack_waiting_last'), u32('crack_painted_last')))
t_floor, t_ramp, t_stone, t_below = lib.pl_get(0, 2, 3), lib.pl_get(0, 4, 3), lib.pl_get(0, 5, 3), lib.pl_get(1, 6, 3)
check(t_floor >= 50000 and t_ramp >= 50000 and t_stone >= 50000 and t_below >= 50000, 'not painted: %s' % [t_floor, t_ramp, t_stone, t_below])
check([lib.pl_get(0, 1, 1), lib.pl_get(0, 3, 3), lib.pl_get(0, 7, 3)] == [1500, 1101, 1103], 'a tile that doesn\'t crack changed')
# the look's numbers as colour.lua wrote them, read where the DLL reads them: the slot of L1's number
slot = 0 if ctypes.c_uint32.from_address(base + OFF['crack_look']).value == ctypes.c_uint32.from_address(base + OFF['crack_gen']).value else 1
ready = ctypes.c_uint32.from_address(base + OFF['crack_look'] + slot * LOOK_BYTES + 4).value
check(ready == 7, 'the look\'s slot not ready with both kinds: %d' % ready)
# the ramps against the Lua's own: looks.lua's redraw for the look, then the cracks
def lua_copy(base_tex, cls, lk, kind, piece):
    px = looks.redraw(base_tex, cls, lk, True)
    words = (ctypes.c_uint32 * 1024)(*[px[n] for n in range(1, 1025)])
    lib.pl_crack(words, kind, lk.amount, piece)
    return list(words)
def copy_px(tex): return [lib.pl_copy_pixel(tex, n) for n in range(1024)]
check(copy_px(t_ramp) == lua_copy(1200, 'dirt', L1, 1, 12), 'the dirt ramp\'s copy differs from looks.lua\'s redraw with its cracks')
check(copy_px(t_stone) == lua_copy(1250, 'stone', L1, 2, 13), 'the stone ramp\'s copy differs from looks.lua\'s redraw with its cracks')
check(lib.pl_expect(t_floor, 1100, 14, 0, 1, slot) and lib.pl_expect(t_below, 1300, 14, 0, 2, slot), 'a floor\'s copy wrong')
# ---- the next frame: their copies found; frost.lua notes the copies painted last frame as Detailed Weather's
# (a floor's ground kept for paths.lua's soil); a ramp under a new number with the same picture finds its copy ----
lib.pl_set(0, 2, 3, 1100); lib.pl_set(0, 4, 3, 21200)
frame(L1)
check(lib.pl_get(0, 2, 3) == t_floor and lib.pl_get(0, 4, 3) == t_ramp and u32('crack_painted_last') == 0, 'copies not found again')
made, cracked = g.made, g.cracked_from
check(made[t_ramp] and made[t_floor] and cracked[t_floor] == 1100 and cracked[t_below] == 1300 and cracked[t_ramp] is None,
      'copies not noted right by frost.lua')
check(u32('crack_d_head') == u32('crack_d_tail'), 'copies left un-noted')
lib.pl_surface(1201, 0, 160, 1200); lib.pl_publish(1201, 21201); lib.pl_set(0, 4, 3, 21201)
frame(L1)
check(lib.pl_get(0, 4, 3) == t_ramp and u32('crack_painted_last') == 0, 'the same picture under a new number not found')
# ---- a change of look: painted on its first frame, by its own numbers (the floor from L2's own copy, 1102) ----
L2 = look('L2', 2, 1, 0.4)
lib.pl_table_copy(1102, 1000)
lib.pl_set(0, 2, 3, 1102); lib.pl_set(0, 4, 3, 21200)
frame(L2)
t_ramp2 = lib.pl_get(0, 4, 3)
slot2 = 0 if ctypes.c_uint32.from_address(base + OFF['crack_look']).value == ctypes.c_uint32.from_address(base + OFF['crack_gen']).value else 1
check(u32('crack_waiting_last') == 0 and t_ramp2 != t_ramp and copy_px(t_ramp2) == lua_copy(1200, 'dirt', L2, 1, 12),
      'the new look\'s ramp not painted at once, or not looks.lua\'s')
check(lib.pl_expect(lib.pl_get(0, 2, 3), 1102, 14, 0, 1, slot2), 'the new look\'s floor not painted from its own copy')
# ---- back to the first look: its copies still the DLL's, nothing painted ----
painted = i64('crack_painted')
lib.pl_set(0, 2, 3, 1100)
frame(L1)
check(lib.pl_get(0, 4, 3) == t_ramp and i64('crack_painted') == painted, 'the first look\'s not shown at once')
# ---- THE NEXT LOOK AHEAD: while L3 is made, its copies painted (its floor from L3's own copy of the ground, 1106:
# FLOORS AHEAD); shown at once with nothing painted ----
L3 = look('L3', 3, 0, 0.9)
F.set_pending('L3', L3)
frame(L1, 2)
check(F.ahead_left() == 0 and i64('crack_ahead_painted') >= 4, 'ahead: %d left, %d painted' % (F.ahead_left(), i64('crack_ahead_painted')))
check(lib.pl_get(0, 4, 3) == t_ramp and lib.pl_get(0, 2, 3) == t_floor, 'the look showing changed while the next was made')
F.set_pending(None, None)
painted = i64('crack_painted')
lib.pl_table_copy(1106, 1000)   # (the look shows: the game draws its table copy)
lib.pl_set(0, 4, 3, 21200); lib.pl_set(0, 2, 3, 1106)
frame(L3)
slot3 = 0 if ctypes.c_uint32.from_address(base + OFF['crack_look']).value == ctypes.c_uint32.from_address(base + OFF['crack_gen']).value else 1
check(i64('crack_painted') == painted and copy_px(lib.pl_get(0, 4, 3)) == lua_copy(1200, 'dirt', L3, 1, 12), 'L3\'s ramp not shown at once, or not looks.lua\'s')
check(lib.pl_expect(lib.pl_get(0, 2, 3), 1106, 14, 0, 1, slot3) and u32('crack_waiting_last') == 0, 'L3\'s floor not shown at once from its own copy')
# ---- a forced temperature (`detailed_weather temperature 15`): every tile worked out again, mild ----
g.forced_celsius = 15
lib.pl_set(0, 2, 3, 1106)
frame(L3)
check(lib.pl_get(0, 2, 3) == 1106 and u32('crack_forced_on') == 1, 'cracked with a mild forced temperature')
g.forced_celsius = None
frame(L3)
check(lib.pl_get(0, 2, 3) >= 50000 and u32('crack_forced_on') == 0, 'not cracked once the forced temperature went')
# ---- a look that doesn't crack: the ground back ----
frame(L3, 2, cracking=False)
check(lib.pl_get(0, 2, 3) == 1106 and lib.pl_get(1, 6, 3) == 1300, 'cracks left: %d %d' % (lib.pl_get(0, 2, 3), lib.pl_get(1, 6, 3)))
print('%d FAILED' % fails if fails else 'native cracks (frost.lua, colour.lua, looks.lua) against cracks in place (the DLL): all checks pass')
sys.exit(1 if fails else 0)
