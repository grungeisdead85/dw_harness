# place.py: frost.lua's NATIVE CRACKS (its own Lua, through lupa) against the DLL's CRACKS IN PLACE (the
# DLL's own code, built over a fake DF: placelib.cpp), test 15R. The views native.lua hands frost.lua
# (crack_view) are made here the same way, over the block: OFF read from native.lua itself.
#   python3 place.py ../../../lua/frost.lua ../../../lua/native.lua
import ctypes, re, sys
import lupa

frost_path, native_path = sys.argv[1], sys.argv[2]
lib = ctypes.CDLL('./placelib.so')
lib.pl_block.restype = ctypes.c_uint64
lib.pl_init()
base = lib.pl_block()

# ---- OFF and the rings' sizes, from native.lua ----
native = open(native_path).read()
off_table = native[native.index('local OFF = {'):native.index('\n}', native.index('local OFF = {'))]   # (the OFF table alone)
OFF = {k: int(v) for k, v in re.findall(r'\b(crack_\w+) = (\d+)', off_table)}
RING = int(re.search(r'local CRACK_RING = (\d+)', native).group(1))
ASK_RING = int(re.search(r'local CRACK_ASK_RING = (\d+)', native).group(1))

lib.pl_off.restype = ctypes.c_int64
fails = 0
for k, v in OFF.items():   # native.lua's OFF against the DLL's struct
    real = lib.pl_off(k.encode())
    if real != v: fails += 1; print('FAIL: native.lua OFF.%s = %d, the DLL\'s %d' % (k, v, real))
print('native.lua\'s crack offsets against the DLL\'s: %d checked' % len(OFF))

CT = {'uint32_t': ctypes.c_uint32, 'int32_t': ctypes.c_int32, 'uint8_t': ctypes.c_uint8}
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

L = lupa.LuaRuntime()
def at(t, name): return Val(t, base + OFF[name])
def arr(t, name): return Arr(t, base + OFF[name])
cv = L.table_from({
    'on': at('uint32_t', 'crack_on'), 'gen': at('uint32_t', 'crack_gen'), 'gen_prev': at('uint32_t', 'crack_gen_prev'),
    'depth': at('uint32_t', 'crack_depth'), 'reset': at('uint32_t', 'crack_reset'),
    'gen_next': at('uint32_t', 'crack_gen_next'), 'next_walked': at('uint32_t', 'crack_next_walked'),
    'next_missing': at('uint32_t', 'crack_next_missing_last'),
    'e_head': at('uint32_t', 'crack_e_head'), 'e_tail': at('uint32_t', 'crack_e_tail'),
    'e32': arr('int32_t', 'crack_e'), 'e8': arr('uint8_t', 'crack_e'), 'ring': RING,
    'q_head': at('uint32_t', 'crack_q_head'), 'q_tail': at('uint32_t', 'crack_q_tail'),
    'q32': arr('int32_t', 'crack_q'), 'q8': arr('uint8_t', 'crack_q'), 'qu32': arr('uint32_t', 'crack_q'),
    'a_head': at('uint32_t', 'crack_a_head'), 'a_tail': at('uint32_t', 'crack_a_tail'),
    'a32': arr('int32_t', 'crack_a'), 'a8': arr('uint8_t', 'crack_a'), 'au32': arr('uint32_t', 'crack_a'),
    'ask_ring': ASK_RING})
def u32(name): return ctypes.c_uint32.from_address(base + OFF[name]).value
def i64(name): return ctypes.c_int64.from_address(base + OFF[name]).value

# ---- frost.lua's NATIVE CRACKS section, with what it uses from the rest of frost.lua stubbed ----
frost = open(frost_path).read()
a = frost.index('-- NATIVE CRACKS (grungeisdead\'s copy, 2026-11-20, test 15R)\n')
a = frost.rindex('\n---', 0, a) + 1
b = frost.index('\nlocal function resting()')
b = frost.rindex('\n-- ', 0, b)
section = frost[a:b]
section = section[:section.index('local SLICE_MS')] if 'local SLICE_MS' in section else section
stub = r'''
local CRACK_KIND_CODE = {dirt = 1, stone = 2, pebbles = 3}
local CRACK_KINDS = {dirt = true, stone = true, pebbles = true}
local MOST_DEPTH = 8
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
plan_view = 'x'
plan_complete = true
local crack_forget
function forget() if crack_forget then crack_forget() end end
'''
tail = r'''
return {crack_code = crack_code, crack_send = crack_send, crack_flush = crack_flush,
        frame = native_crack_frame, begin = native_crack_begin, ['end'] = native_crack_end,
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

# ---- started: tiles handed over as plan() works them out ----
F.begin(cv)
tile = lambda **k: L.table_from(k)
check(F.crack_code(tile(band=1, ramp=False)) == 0x40, 'a floor\'s code')
check(F.crack_code(tile(band=1, ramp=True, **{'class': 'dirt'})) == 0x81, 'a dirt ramp\'s code')
check(F.crack_code(tile(band=1, ramp=True)) == 0x82, 'a ramp with no class is stone\'s')
check(F.crack_code(tile(band=1, ramp=True, **{'class': 'grass'})) == 0, 'a grass ramp cracks')
check(F.crack_code(tile(ramp=False)) == 0, 'a mild floor cracks')
F.crack_send(2, 3, 2, 0x40)             # a dirt floor on the view's level
F.crack_send(4, 3, 2, 0x81)             # a dirt ramp
F.crack_send(6, 3, 1, 0x40)             # a floor a level below, seen through open space
F.crack_send(1, 1, 2, 0x40)             # a grass floor: no copy (ground_kind)
F.crack_send(2, 3, 2, 0x40)             # (the same again: not handed twice)
lib.pl_set(0, 2, 3, 1100); lib.pl_set(0, 1, 1, 1500); lib.pl_set(1, 6, 3, 1150)
lib.pl_publish(1200, 21200); lib.pl_set(0, 4, 3, 21200)     # the ramp shows its copy in the look
frame('L1', 0.5)
check(u32('crack_e_head') == 4, 'tiles handed: %d' % u32('crack_e_head'))
check(i64('crack_asked') == 4, 'copies asked: %d' % i64('crack_asked'))
frame('L1', 0.5)
keys = list(F['keys']().values())   # (F['keys']: a Lua table's .keys is lupa's own)
check(sorted(keys) == sorted(['c1100:L1:14', 'rp1200:L1:dirt:12', 'c1150:L1:14']), 'keys made: %s' % keys)
t_floor, t_ramp, t_below = F.baked('c1100:L1:14'), F.baked('rp1200:L1:dirt:12'), F.baked('c1150:L1:14')
check(lib.pl_get(0, 2, 3) == t_floor and lib.pl_get(0, 4, 3) == t_ramp and lib.pl_get(1, 6, 3) == t_below,
      'copies placed: %d %d %d' % (lib.pl_get(0, 2, 3), lib.pl_get(0, 4, 3), lib.pl_get(1, 6, 3)))
check(lib.pl_get(0, 1, 1) == 1500, 'the grass floor changed')
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
# ---- a tile that can no longer crack (snow came) ----
frame('L1', 0.5)
F.crack_send(2, 3, 2, 0)
lib.pl_set(0, 2, 3, 1100)
frame('L1', 0.5)
check(lib.pl_get(0, 2, 3) == 1100, 'a tile cracked after its code went to 0')
# ---- frost.lua forgets (REMEMBER_MOST, a setting changed): the DLL's tiles go too, and are handed again ----
lua_g.forget()
lib.pl_set(0, 2, 3, 1100)
frame('L1', 0.5)
check(lib.pl_get(0, 2, 3) == 1100 and u32('crack_tiles_kept') > 0, 'the DLL\'s tiles cleared by frost.lua forgetting (15T: kept)')
F.crack_send(2, 3, 2, 0)   # (handed again as it's worked out: crack_sent was emptied, so even a 0 goes)
check(len(list(lua_g.crack_pending.values())) == 4, 'not handed again after forgetting')
frame('L1', 0.5)
F.crack_send(2, 3, 2, 0x40)
frame('L1', 0.5)
check(lua_g.native_crack_counts.forgets == 2, 'forgets counted: %s' % lua_g.native_crack_counts.forgets)
# ---- THE NEXT LOOK AHEAD (15T): while L4 is made, its copies asked for and made; shown at once ----
frame('L1', 0.5)
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
      'ahead from a dry-less look: cell %d, left %d' % (lib.pl_get(0, 2, 3), F.ahead_left()))
F.set_pending(None)
frame('L5', 0.5)
check(lib.pl_get(0, 2, 3) == F.baked('c1100:L5:14'), 'L5 not shown at once')
# ---- the ring full: tiles wait their turn ----
F.begin(cv)
for k in range(RING + 100): F.crack_send(k % 32, k // 32, 0, 0x40)
F.crack_flush()
check(u32('crack_e_head') - u32('crack_e_tail') == RING and len(list(lua_g.crack_pending.values())) == 400,
      'the ring: %d in, %d waiting' % (u32('crack_e_head') - u32('crack_e_tail'), len(list(lua_g.crack_pending.values()))))
lib.pl_frame(); F.crack_flush()
check(len(list(lua_g.crack_pending.values())) == 0, 'the rest not handed')
print('%d FAILED' % fails if fails else 'native cracks (frost.lua) against cracks in place (the DLL): all checks pass')
sys.exit(1 if fails else 0)
