# 16A: stone-tint.lua with both Detailed Weathers (grungeisdead's copy, with copy_of and shown_look; Poss's,
# with neither), run in lupa (Lua 5.3) with stand-ins: a copy filed as its floor, a stale one not, the
# filing forgotten on a change of look, and the ramp overlays found under Detailed Weather's copies.
# Usage: python3 stone_tint.py path/to/stone-tint.lua
import sys, re
import lupa.lua53 as lupa
src = open(sys.argv[1]).read()
def grab(name):
    m = re.search(r'^local function ' + re.escape(name) + r'\(.*?^end$', src, re.S | re.M)
    assert m, name
    return m.group(0).replace('local function ', 'function ', 1)
fails = 0
for style in ('ours', 'poss'):
    L = lupa.LuaRuntime()
    L.execute('''
    floor_texpos, known_texpos = {[100] = 'FLOOR_A', [101] = 'FLOOR_B'}, {}
    dw_colour, dw_look, dw_filed = nil, nil, {}
    _G.detailed_weather_pointed_from = {[5100] = 100, [6100] = 100, [5101] = 101}
    look_key, copies = 'K1', {K1 = {[100] = {texpos = 5100}, [101] = {texpos = 5101}}, K2 = {[100] = {texpos = 6100}}}
    ''')
    if style == 'ours':
        L.execute('''mod = {copy_of = function(o) local c = copies[look_key] and copies[look_key][o] return c and c.texpos or o end,
                           shown_look = function() return {}, look_key end}''')
    else:
        L.execute('''mod = {state = setmetatable({}, {__index = function(_, k)
                         if k == 'shown' then return look_key end
                         if k == 'copies' then return copies end end})}''')
    L.execute('function reqscript(name) assert(name == "internal/detailed_weather/colour") return mod end')
    for f in ('dw_live_copy', 'dw_shown_key', 'dw_module', 'dw_floor', 'dw_check_look'): L.execute(grab(f))
    r = L.execute('''
    local out = {}
    dw_module()
    dw_check_look()                                     -- (the first look seen: K1)
    out[1] = dw_floor(5100) == 'FLOOR_A' and dw_filed[5100] == 100
    out[2] = dw_floor(6100) == nil                      -- (a copy of another look: stale, not filed)
    dw_check_look()                                     -- (the same look: kept)
    out[2] = out[2] and dw_filed[5100] == 100
    known_texpos[5100] = 'FLOOR_A'
    look_key = 'K2'
    dw_check_look()
    out[3] = known_texpos[5100] == nil and next(dw_filed) == nil   -- (forgotten on the change)
    out[4] = dw_floor(6100) == 'FLOOR_A' and dw_floor(5100) == nil
    look_key = nil                                      -- (the game's own look)
    out[5] = dw_live_copy(mod, 100) == 100
    return out
    ''')
    for i in range(1, 6):
        if not r[i]: fails += 1; print('FAIL', style, 'check', i)
# the ramp overlays under Detailed Weather's copies
L = lupa.LuaRuntime()
L.execute('''
_G.detailed_weather_pointed_from = {[5301] = 301, [5999] = 999}
OVERLAY_PAGES = {RAMP_OVERLAY = true, DARK_RAMP_OVERLAY = true}
TEXTURE_TABLES = {}
for i = 1, 11 do TEXTURE_TABLES[i] = 'texture_indices' .. i end
local function zero(t) local z = {} for i, v in ipairs(t) do z[i - 1] = v end return setmetatable(z, {__len = function() return #t end}) end
df = {global = {texture = {page = setmetatable({[0] = {loaded = true, token = 'RAMP_OVERLAY', texpos = zero({300, 301}), texpos_gs = zero({})},
                                                [1] = {loaded = true, token = 'GRASS', texpos = zero({999}), texpos_gs = zero({})}},
                                               {__len = function() return 2 end})},
                gps = {texture_indices1 = zero({7, 300, 5301, 5999, 301})}}}
''')
L.execute(grab('find_overlay_slots'))
r = L.execute('''local s = find_overlay_slots() local idx = {} for _, e in ipairs(s) do idx[#idx + 1] = e[2] .. ':' .. e[3] end
                 table.sort(idx) return table.concat(idx, ',')''')
if r != '1:300,2:5301,4:301': fails += 1; print('FAIL overlays found:', r)
print('stone-tint.lua 16A: %s' % ('all checks pass' if not fails else '%d FAILED' % fails))
