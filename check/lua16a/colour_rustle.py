# 16A: colour.lua's rustle sent to the DLL (update_sway's trees, grass_wave's grass, the canopy listing's
# wood), run in lupa (Lua 5.3) with stand-ins. Usage: python3 colour_rustle.py <folder with colour.lua>
import sys, re
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/colour.lua').read()
def grab(name):
    m = re.search(r'^local function ' + re.escape(name) + r'\(.*?^end$', src, re.S | re.M)
    assert m, name
    return m.group(0).replace('local function ', 'function ', 1)
def const(name): return re.search(r'^local ' + name + r' = [^\n]*', src, re.M).group(0).replace('local ', '', 1)
L = lupa.LuaRuntime()
L.execute('''
clock = 0
dfhack = {getTickCount = function() return clock end}
df = {global = {pause_state = false}}
wind = {get = function() return nil, nil, wind_strength end}
wind_strength = 0.5
function lean_now() return 0.02, 1, 0, 'E' end
screen_allowance, frame_number = 1, 1
native_colour = true
function native_sway_waiting() return false end
function forget_sway() state.sway = nil end
room = true
function native_room(n, kind) assert(kind == 3, 'kind ' .. tostring(kind)) return room end
sent = {}
function native_rustle(t, moves, all) sent[#sent + 1] = {t, moves, all} return {handle = 1, texpos = 800 + #sent, native = true} end
told = {}
function native_sway_made(original, frames) told[original] = frames end
function rustle_moves(phase, kind, sway) return {phase = phase, kind = kind} end
read = 0
grass = {pixels_of = function(t) read = read + 1 return nil end}
state = {screen_copies = {K = {[11] = {texpos = 111}, [12] = {texpos = 112}, [13] = {texpos = 113}}}, ours = {}, screen_original = {}}
native_canopy = {[11] = true, [12] = 'wood'}
''')
for c in ('RUSTLE_PHASES', 'SWAY_MS', 'LEAN_BREEZE_FROM', 'LEAN_STRONG_FROM', 'LEAN_GALE_FROM', 'LEAN_BREEZE_MOST', 'SEEN_CANOPY', 'SEEN_WOOD'):
    L.execute(const(c))
L.execute(re.search(r'^local RUSTLE = \{.*?^\}', src, re.S | re.M).group(0).replace('local ', '', 1))
L.execute(grab('update_sway'))
# the canopy listing's decode, cut from the seen loop
dec = re.search(r'\n(\s*)local wood = v & SEEN_WOOD ~= 0\n\s*native_canopy\[[^\n]*\n', src).group(0)
L.execute('function decode(v)\n' + dec + '\nend')
L.execute('''
-- trees: a leafy one sent as 16 jobs from its copy, leaves only; wood none; a picture not listed none
update_sway('K')
assert(#sent == 16, 'jobs ' .. #sent)
for p = 1, 16 do
    assert(sent[p][1] == 111 and sent[p][3] == false and sent[p][2].phase == p - 1 and sent[p][2].kind == 'breeze', 'job ' .. p)
end
assert(told[11] and told[11][16].texpos == 816 and state.ours[816] and state.screen_original[816] == 11, 'frames told')
assert(told[12] and told[12][1] == false and told[12][16] == false, 'wood given frames')
assert(told[13] == nil and read == 0, 'pixels read, or an unlisted picture made')
-- no room: nothing sent, tried again next frame
state.sway, told, sent, room = nil, {}, {}, false
update_sway('K')
assert(#sent == 0 and told[11] == nil and told[12] ~= nil, 'sent without room')
room = true
update_sway('K')
assert(#sent == 16 and told[11], 'not sent once room')
-- the listing: wood, leaves
native_canopy = {}
decode(SEEN_CANOPY | SEEN_WOOD | 4242)
decode(SEEN_CANOPY | 4343)
assert(native_canopy[4242] == 'wood' and native_canopy[4343] == true, 'decode')
print('colour.lua 16A rustle (trees): all checks pass')
''')
# the grass's frames: grass_wave's loop over the frames wanted, cut out and run alone
a = src.index('    -- the rustle frames still wanted, a few a frame')
b = src.index('\nend\n', a)
L.execute('function wanted_loop(wanted, l, copies, kind, rustle, step_of)\n' + src[a:b] + '\nend')
L.execute('''
GRASS_RUSTLE_MS = 1.5
sent, room = {}, true
local l = {frames = {[21] = {[2] = {texpos = 2102}}}, original = {}, ux = 1, uy = 0}
local copies = {[21] = {texpos = 2100}}
local wanted = {[(21 * 16 + 2) * 64 + 5] = true, [(21 * 16 + 0) * 64 + 7] = true}
local rustle, step_of = {}, {}
wanted_loop(wanted, l, copies, 'strong', rustle, step_of)
assert(#sent == 2 and next(wanted) == nil, 'grass jobs ' .. #sent)
local by = {}
for _, s in ipairs(sent) do by[s[1]] = s end
assert(by[2102] and by[2102][3] == true and by[2102][2].phase == 5, 'from the lean frame, every pixel')
assert(by[2100] and by[2100][2].phase == 7, 'from the copy')
assert(rustle[21][2][5].texpos and rustle[21][0][7].texpos and step_of[rustle[21][2][5].texpos][2] == 2, 'filed')
-- no room: left wanted
sent, room = {}, false
local w2 = {[(21 * 16 + 4) * 64 + 1] = true}
wanted_loop(w2, l, copies, 'strong', rustle, step_of)
assert(#sent == 0 and next(w2) ~= nil, 'sent without room, or dropped')
print('colour.lua 16A rustle (grass): all checks pass')
''')
