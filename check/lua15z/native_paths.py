import re
import sys   # (argv[1]: the folder with native.lua, colour.lua and snow_ramps.lua)
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/native.lua').read()
def grab(name):
    m = re.search(r'^local function ' + re.escape(name) + r'\(.*?^end$', src, re.S | re.M)
    assert m, name
    return m.group(0).replace('local function ', 'function ', 1)
offs = dict(re.findall(r'(\w+) = (\d+),', src[src.index('local OFF = {'):src.index('\n}', src.index('local OFF = {'))]))
L = lupa.LuaRuntime()
L.execute('''
mem, OFF = {}, {}
MURKY_MAKE_MS, MURKY_MAKE_BUSY_MS = 1, 8
PATHS_RING = 256
paths_no_blank_seen, murky_waits_seen = 0, 0
paths_soil_asked, paths_soil_next = false, 0
paths_running, murky_running, held = true, false, false
fills = {}
function murky_fill_blanks(base, ms) fills[#fills + 1] = ms end
function block() return 1000000 end
function peek(t, a) return mem[a] or 0 end
function poke(t, a, v) mem[a] = v end
pm = {mode = 'on', native_soil_wanted = function() return false end, native_base = function(t) return 0 end}
function paths_module() return pm end
function murky_module() return {mode = 'on'} end
function colour_module() return nil end
function murky_hand_back() end
function paths_hand_back() end
os = {clock = function() return 100 end}
dfhack = {isMapLoaded = function() return true end, getTickCount = function() return 0 end}
''')
for k, v in offs.items(): L.execute('OFF.%s = %s' % (k, v))
L.execute(grab('update_paths'))
L.execute('''
local B = 1000000
update_paths()
assert(#fills == 1 and fills[1] == MURKY_MAKE_MS, 'paths fill ' .. tostring(fills[1]))
mem[B + OFF.paths_no_blank] = 5
update_paths()
assert(#fills == 2 and fills[2] == MURKY_MAKE_BUSY_MS, 'paths busy fill')
update_paths()
assert(#fills == 3 and fills[3] == MURKY_MAKE_MS, 'paths fill after')
murky_running = true
update_paths()
assert(#fills == 3, 'filled with murky running')
print('native.lua 15Z paths refill: all checks pass')
''')
