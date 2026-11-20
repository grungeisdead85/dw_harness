# place.py: frost.lua's NATIVE CRACKS (its own Lua, through lupa) against the DLL's CRACKS IN PLACE (the
# DLL's own code, built over a fake DF with a map: placelib.cpp), tests 15R to 15U. The views native.lua
# hands frost.lua (crack_view) are made here the same way, over the block: OFF read from native.lua's
# own OFF table and checked against the DLL's struct first. frost.lua's rules (crack_rules) are handed
# to the DLL as native.lua's crack_start does, and the DLL works the tiles out from the map.
#   python3 place.py ../../../lua/frost.lua ../../../lua/native.lua
import ctypes, re, sys
import lupa

frost_path, native_path = sys.argv[1], sys.argv[2]
lib = ctypes.CDLL('./placelib.so')
lib.pl_block.restype = ctypes.c_uint64
lib.pl_off.restype = ctypes.c_int64
lib.pl_init()
base = lib.pl_block()

# ---- OFF and the rings' size, from native.lua ----
native = open(native_path).read()
off_table = native[native.index('local OFF = {'):native.index('\n}', native.index('local OFF = {'))]   # (the OFF table alone)
OFF = {k: int(v) for k, v in re.findall(r'\b((?:crack_\w+|off_temperature)) = (\d+)', off_table)}
ASK_RING = int(re.search(r'local CRACK_ASK_RING = (\d+)', native).group(1))

fails = 0
for k, v in OFF.items():   # native.lua's OFF against the DLL's struct
    real = lib.pl_off(k.encode())
    if real != v: fails += 1; print('FAIL: native.lua OFF.%s = %d, the DLL\'s %d' % (k, v, real))
print('native.lua\'s crack offsets against the DLL\'s: %d checked' % len(OFF))

CT = {'uint32_t': ctypes.c_uint32, 'int32_t': ctypes.c_int32, 'uint8_t': ctypes.c_uint8, 'double': ctypes.c_double,
      'int64_t': ctypes.c_int64}
class Val:       # df.reinterpret_cast(t, a): .value
    def __init__(s, t, a): s.__dict__['p'] = CT[t].from_address(a)
    def __getattr__(s, k): return s.p.value
    def __setattr__(s, k, v): s.p.value = v
    def __getitem__(s, k): return s.p.value      # (lupa indexes a Python object either way)
    def __setitem__(s, k, v): s.p.value = v
class Arr:       # df.reinterpret_cast(t, a)[i]
    def __init__(s, t, a): s.t, s.a, s.n = CT[t], a, ctypes.sizeof(CT[t])
    def __getitem__(s, i): return s.t.from_address(s.a + i * s.n).value
    def __setitem__(s, i, v): s.t.from_address(s.a + i * s.n).value = v
def at(t, name): return Val(t, base + OFF[name])
def arr(t, name): return Arr(t, base + OFF[name])
def u32(name): return ctypes.c_uint32.from_address(base + OFF[name]).value
def i64(name): return ctypes.c_int64.from_address(base + OFF[name]).value
def set_(t, name, v): CT[t].from_address(base + OFF[name]).value = v

L = lupa.LuaRuntime()
cv = L.table_from({
    'on': at('uint32_t', 'crack_on'), 'gen': at('uint32_t', 'crack_gen'), 'gen_prev': at('uint32_t', 'crack_gen_prev'),
    'depth': at('uint32_t', 'crack_depth'),
    'forced_on': at('uint32_t', 'crack_forced_on'), 'forced_c': at('double', 'crack_forced_c'),
    'tables': at('uint32_t', 'crack_tables'),
    'gen_next': at('uint32_t', 'crack_gen_next'), 'next_walked': at('uint32_t', 'crack_next_walked'),
    'next_missing': at('uint32_t', 'crack_next_missing_last'),
    'q_head': at('uint32_t', 'crack_q_head'), 'q_tail': at('uint32_t', 'crack_q_tail'),
    'q32': arr('int32_t', 'crack_q'), 'q8': arr('uint8_t', 'crack_q'), 'qu32': arr('uint32_t', 'crack_q'),
    'a_head': at('uint32_t', 'crack_a_head'), 'a_tail': at('uint32_t', 'crack_a_tail'),
    'a32': arr('int32_t', 'crack_a'), 'a8': arr('uint8_t', 'crack_a'), 'au32': arr('uint32_t', 'crack_a'),
    'ask_ring': ASK_RING})

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
local MOST_DEPTH, BAND_C, COOL_FROM, WARM_FROM = 8, 3, 10, 20
local EMPTY, RAMP_TOP, RAMP, WALL, FLOOR = 'EMPTY', 'RAMP_TOP', 'RAMP', 'WALL', 'FLOOR'
local SHAPES = {[1] = WALL, [20] = FLOOR, [21] = RAMP, [22] = EMPTY, [23] = RAMP, [24] = RAMP, [25] = RAMP_TOP}
local floor_like, class_of_tiletype = {}, {}
local function shape(tt)
    local s = SHAPES[tt] or WALL
    floor_like[tt] = s == FLOOR
    class_of_tiletype[tt] = tt == 23 and 'grass' or tt == 21 and 'dirt' or 'stone'
    return s
end
df = {tiletype = setmetatable({_last_item = 30}, {__index = function(_, k) return type(k) == 'number' and 'tt' or nil end})}
local counts = {}
local function count(w) counts[w] = (counts[w] or 0) + 1 end
local function picture_id(t) return 'p' .. t end
local function ground_kind(t) return t < 1200 and 'dirt' or 'grass' end
local baked, made_n, keys_asked = {}, 0, {}
local budget = 1000
local function baked_texpos(key, bake, state)
    if baked[key] == nil then
        if budget <= 0 then state.waited = true return nil end
        budget = budget - 1
        keys_asked[#keys_asked + 1] = key
        made_n = made_n + 1
        baked[key] = 50000 + made_n
    end
    return baked[key]
end
local colour = {}
local pending = nil
function colour.pending_look() if pending then return {k = pending}, pending end return nil, nil end
local function crack_amount(look) return look and (look.k ~= 'L3' and 0.5 or nil) or nil end
local looks = {}
local function crack_spec() return nil end
local function crack_packed() return nil end
local function bake_cracks() return nil end
cracked_from = {}
function forget() end
'''
tail = r'''
return {frame = native_crack_frame, begin = native_crack_begin, rules = crack_rules,
        set_budget = function(n) budget = n end, set_pending = function(k) pending = k end,
        ahead_left = cracks_ahead_left, keys = function() return keys_asked end,
        baked = function(k) return baked[k] end, counts = counts}
'''
F = L.execute(stub + section + tail)
lua_g = L.globals()

def check(c, what):
    global fails
    if not c: fails += 1; print('FAIL:', what)

def frame(look, cracking, n=1):
    for _ in range(n):
        F.frame(L.table_from({'k': look}), look, cracking, L.table_from({'baked': 0}))
        lib.pl_frame()

# ---- started: frost.lua's rules handed over as native.lua's crack_start does ----
F.begin(cv)
rules = F.rules()
tt = rules['tiletype']
check(tt[20] == 0x40 and tt[21] == 0x81 and tt[24] == 0x82 and tt[23] == 0 and tt[22] == 0x20 and tt[25] == 0x20 and tt[1] == 0,
      'the rules by tiletype: %s' % [tt[k] for k in (1, 20, 21, 22, 23, 24, 25)])
for k in range(31): ctypes.c_uint8.from_address(base + OFF['crack_tt'] + k).value = tt[k] or 0
set_('double', 'crack_band_c', rules['band_c']); set_('double', 'crack_cool_from', rules['cool_from'])
set_('double', 'crack_warm_from', rules['warm_from']); set_('uint32_t', 'crack_depth', rules['most_depth'])
set_('uint32_t', 'crack_recheck', 600); set_('uint32_t', 'crack_tables', u32('crack_tables') + 1)
OUT, SUB = 1 << 16, 1 << 15
def units(c): return int(10000 + c * 9 / 5)
lib.pl_tile(2, 3, 2, 20, 0, units(30))        # a dirt floor (its picture 1100), hot
lib.pl_tile(4, 3, 2, 21, 0, units(30))        # a dirt ramp
lib.pl_tile(6, 3, 2, 22, 0, units(30))        # open space, and under it
lib.pl_tile(6, 3, 1, 20, OUT, units(-10))     #   a floor outdoors, cold
lib.pl_tile(1, 1, 2, 20, 0, units(30))        # a grass floor (its picture 1500): asked, none
lib.pl_tile(3, 3, 2, 20, 0, units(15))        # mild
lib.pl_tile(5, 3, 1, 20, OUT, units(30))      # under a wall (0 at 5,3,2): not seen
lib.pl_tile(7, 3, 2, 20, 2, units(30))        # water on it
lib.pl_tile(1, 4, 1, 20, SUB, units(30)); lib.pl_tile(1, 4, 2, 22, 0, units(30))   # underground, open above
lib.pl_set(0, 2, 3, 1100); lib.pl_set(0, 1, 1, 1500); lib.pl_set(1, 6, 3, 1150)
lib.pl_set(0, 3, 3, 1101); lib.pl_set(1, 5, 3, 1102); lib.pl_set(0, 7, 3, 1103); lib.pl_set(1, 1, 4, 1104)
lib.pl_publish(1200, 21200); lib.pl_set(0, 4, 3, 21200)     # the ramp shows its copy in the look
frame('L1', 0.5)
check(i64('crack_asked') == 4 and i64('crack_blocks_worked') > 0, 'copies asked: %d, blocks worked %d' % (i64('crack_asked'), i64('crack_blocks_worked')))
frame('L1', 0.5)
keys = list(F['keys']().values())   # (F['keys']: a Lua table's .keys is lupa's own)
check(sorted(keys) == sorted(['c1100:L1:14', 'rp1200:L1:dirt:12', 'c1150:L1:14']), 'keys made: %s' % keys)
t_floor, t_ramp, t_below = F.baked('c1100:L1:14'), F.baked('rp1200:L1:dirt:12'), F.baked('c1150:L1:14')
check(lib.pl_get(0, 2, 3) == t_floor and lib.pl_get(0, 4, 3) == t_ramp and lib.pl_get(1, 6, 3) == t_below,
      'copies placed: %d %d %d' % (lib.pl_get(0, 2, 3), lib.pl_get(0, 4, 3), lib.pl_get(1, 6, 3)))
check([lib.pl_get(0, 1, 1), lib.pl_get(0, 3, 3), lib.pl_get(1, 5, 3), lib.pl_get(0, 7, 3), lib.pl_get(1, 1, 4)] == [1500, 1101, 1102, 1103, 1104],
      'a tile that doesn\'t crack changed')
check(lib.pl_link(t_ramp) == 1200 and lib.pl_mark(t_ramp), 'the ramp\'s copy not a copy of its own picture')
# ---- a change of look: the look before's held until the new look's are made ----
F.set_budget(1)
lib.pl_set(0, 2, 3, 1100); lib.pl_set(0, 4, 3, 21200)   # (the game drew them again)
frame('L2', 0.8)
check(lib.pl_get(0, 2, 3) == t_floor and lib.pl_get(0, 4, 3) == t_ramp and u32('crack_held_last') == 3,
      'the look before\'s not held: %d %d (held %d)' % (lib.pl_get(0, 2, 3), lib.pl_get(0, 4, 3), u32('crack_held_last')))
frame('L2', 0.8)    # one made a frame (budget): one placed, the rest still held
check(u32('crack_written_last') == 1 and u32('crack_held_last') == 2,
      'a trickle: written %d, held %d' % (u32('crack_written_last'), u32('crack_held_last')))
F.set_budget(1000)
frame('L2', 0.8, 2)
check(lib.pl_get(0, 2, 3) == F.baked('c1100:L2:14') and lib.pl_get(0, 4, 3) == F.baked('rp1200:L2:dirt:12'),
      'the new look\'s not placed')
# ---- back to the first look: its copies are still the DLL's, shown at once with nothing asked ----
asked = i64('crack_asked')
frame('L1', 0.5)
check(lib.pl_get(0, 2, 3) == t_floor and i64('crack_asked') == asked, 'the first look\'s not shown at once')
# ---- a look that doesn't crack: the ground back ----
frame('L3', None, 2)
check(lib.pl_get(0, 2, 3) == 1100 and lib.pl_get(1, 6, 3) == 1150, 'cracks left: %d %d' % (lib.pl_get(0, 2, 3), lib.pl_get(1, 6, 3)))
# ---- a forced temperature (`detailed_weather temperature 15`): every tile worked out again, mild ----
frame('L1', 0.5)
check(lib.pl_get(0, 2, 3) == t_floor, 'not cracked again')
lua_g.forced_celsius = 15
lib.pl_set(0, 2, 3, 1100)
frame('L1', 0.5)
check(lib.pl_get(0, 2, 3) == 1100 and u32('crack_forced_on') == 1, 'cracked with a mild forced temperature: %d' % lib.pl_get(0, 2, 3))
lua_g.forced_celsius = None
frame('L1', 0.5)
check(lib.pl_get(0, 2, 3) == t_floor and u32('crack_forced_on') == 0, 'not cracked once the forced temperature went')
# ---- THE NEXT LOOK AHEAD (15T): while L4 is made, its copies asked for and made; shown at once ----
F.set_pending('L4')
frame('L1', 0.5)
check(F.ahead_left() == -1 or F.ahead_left() > 0, 'ahead not waiting at first: %d' % F.ahead_left())
frame('L1', 0.5, 3)
check(F.ahead_left() == 0, 'ahead still waiting: %d' % F.ahead_left())
check(F.baked('c1100:L4:14') is not None and F.baked('rp1200:L4:dirt:12') is not None, 'the next look\'s copies not made')
check(lib.pl_get(0, 2, 3) == t_floor, 'the look showing changed while the next was made')
F.set_pending(None)
asked = i64('crack_asked') + i64('crack_ahead_asked')
lib.pl_set(0, 2, 3, 1100); lib.pl_set(0, 4, 3, 21200)
frame('L4', 0.5)
check(lib.pl_get(0, 2, 3) == F.baked('c1100:L4:14') and lib.pl_get(0, 4, 3) == F.baked('rp1200:L4:dirt:12')
      and i64('crack_asked') + i64('crack_ahead_asked') == asked, 'L4 not shown at once')
# from a look that doesn't crack (L3), ahead of L5: asked with the cracks off, the cells left as drawn
frame('L3', None, 3)
F.set_pending('L5')
frame('L3', None, 4)
check(lib.pl_get(0, 2, 3) == 1100 and F.ahead_left() == 0 and F.baked('c1100:L5:14') is not None,
      'ahead from a look that doesn\'t crack: cell %d, left %d' % (lib.pl_get(0, 2, 3), F.ahead_left()))
F.set_pending(None)
frame('L5', 0.5)
check(lib.pl_get(0, 2, 3) == F.baked('c1100:L5:14'), 'L5 not shown at once')
print('%d FAILED' % fails if fails else 'native cracks (frost.lua) against cracks in place (the DLL): all checks pass')
sys.exit(1 if fails else 0)
