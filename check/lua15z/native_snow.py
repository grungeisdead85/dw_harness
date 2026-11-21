# 15Z: native.lua's new and changed parts run in lupa with stubs
import sys   # (argv[1]: the folder with native.lua, colour.lua and snow_ramps.lua)
import re
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/native.lua').read()
def grab(name):
    m = re.search(r'^local function ' + re.escape(name) + r'\(.*?^end$', src, re.S | re.M)
    assert m, name
    return m.group(0).replace('local function ', 'function ', 1)
offs = dict(re.findall(r'(\w+) = (\d+),', src[src.index('local OFF = {'):src.index('\n}', src.index('local OFF = {'))]))
L = lupa.LuaRuntime()
L.execute('''
mem = {}
OFF = {}
TRY_EVERY = 5
next_try = 0
CRACK_BLANKS = 512
BLANK_MS, BLANK_LOW_MS = 1, 4
held = false
running = false
crack_running = false
spat_running_now = false
blank_calls = {}
function crack_blanks(base, ms) blank_calls[#blank_calls + 1] = ms end
function block() return 1000000 end
function peek(t, a) return mem[a] or 0 end
function poke(t, a, v) assert(type(v) == 'number', 'poke of a ' .. type(v) .. ' at ' .. tostring(a)) mem[a] = v end
function addr_of(r) return r.addr end
function fill_common(base) return 'common' end
function check_tables(t) end
function any_event(c) return {addr = 7000, _field = function(self, n) return {addr = 7000 + #n} end} end
function fill_snow_tables(base, t) return {page = 1, edge_ids = 2, singles = 0, kept = 0} end
function reset_counts(base) end
function qerror(m) error(m) end
WATER, SNOWY = 6, 50
_G.detailed_weather_made = {}
sr_stub = {native_tables = function(flag) sr_stub.asked_with = flag return {layout = {}} end,
           serve_native_bakes = function() end, forget = function() end,
           snow_made = {}, snowy_edge_texpos = {}}
function snow_module() return sr_stub end
df = {global = {gps = {main_viewport = {}}, world = {map = {map_blocks = {1}}},
                enabler = {textures = {_field = function(self, n) return {addr = 4242} end}}},
      matter_state = {Powder = 2}, block_square_event_material_spatterst = {}, block_square_event_grassst = {}}
dfhack = {isMapLoaded = function() return true end}
os = {clock = function() return 100 end}
''')
for k, v in offs.items(): L.execute('OFF.%s = %s' % (k, v))
for f in ('snow_bakes_told', 'update_snow', 'hand_back', 'start', 'report_paths'):
    L.execute(grab(f))
L.execute('''
local B = 1000000
-- the start: the DLL told to paint the snow's bakes; snow_ramps.lua asked for no bakes at start
mem[B + OFF.bake_here_head] = 7
update_snow(true)
assert(running, 'not started: ' .. tostring(why_not))
assert(sr_stub.asked_with == true, 'native_tables not asked to leave the bakes')
assert(peek('uint32_t', B + OFF.snow_bake_here) == 1, 'snow_bake_here not set')
assert(peek('uint64_t', B + OFF.col_raws) == 4242, 'col_raws not handed over')
assert(peek('uint32_t', B + OFF.bake_here_tail) == 7, 'bakes told from before taken')
-- two bakes told: filed as the snow's own (a fade and a snowy edge), the blanks topped up
local at = B + OFF.bake_here + (7 % 256) * 16
mem[at] = 63000; mem[at + 4] = 1
mem[at + 16] = 63001; mem[at + 20] = 3
mem[B + OFF.bake_here_head] = 9
mem[B + OFF.crack_b_head] = 500; mem[B + OFF.crack_b_tail] = 0
update_snow(true)
assert(_G.detailed_weather_made[63000] and _G.detailed_weather_made[63001], 'not made')
assert(sr_stub.snow_made[63000] and sr_stub.snow_made[63001], 'not snow_made')
assert(not sr_stub.snowy_edge_texpos[63000] and sr_stub.snowy_edge_texpos[63001], 'snowy edge filing')
assert(peek('uint32_t', B + OFF.bake_here_tail) == 9, 'not taken')
assert(#blank_calls == 1 and blank_calls[1] == BLANK_MS, 'blanks: ' .. #blank_calls)
-- the blanks low: topped up faster; the cracks running: left to them
mem[B + OFF.crack_b_head] = 50
update_snow(true)
assert(#blank_calls == 2 and blank_calls[2] == BLANK_LOW_MS, 'blanks low')
crack_running = true
update_snow(true)
assert(#blank_calls == 2, 'blanks topped up twice')
crack_running = false
-- the ring's count wrapping past 2^32
mem[B + OFF.bake_here_tail] = 0xffffffff
local at2 = B + OFF.bake_here + (0xffffffff % 256) * 16
mem[at2] = 63002; mem[at2 + 4] = 2
mem[B + OFF.bake_here_head] = 0
update_snow(true)
assert(_G.detailed_weather_made[63002] and peek('uint32_t', B + OFF.bake_here_tail) == 0, 'wrap')
-- handed back: snow_ramps.lua bakes again
update_snow(false)
assert(not running and peek('uint32_t', B + OFF.snow_bake_here) == 0, 'hand back')
report_paths(B)
print('native.lua 15Z: all checks pass')
''')
