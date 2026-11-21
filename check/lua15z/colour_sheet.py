# 15Z: colour.lua's sheet loop (the partly see-through textures remade) in lupa (Lua 5.3)
import sys   # (argv[1]: the folder with native.lua, colour.lua and snow_ramps.lua)
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/colour.lua').read()
a = src.index("    -- the sheet's partly see-through textures, remade, before the look shows")
b = src.index("    if remaking and #remaking > 0 then", a)
loop = src[a:b]
L = lupa.LuaRuntime()
L.execute('''
clock = 0
dfhack = {getTickCount = function() return clock end}
state = {remaking = {}}
sent, lua_made, busy_after = {}, 0, nil
function native_pixels(kind, t)
    if busy_after and #sent >= busy_after then return nil, true end
    if t == 66 then return nil end   -- (unreadable: made in Lua)
    sent[#sent + 1] = t
    return {texpos = 5000 + t, native = true}
end
function unpremultiplied(t) lua_made = lua_made + 1 return {texpos = 7000 + t} end
''')
L.execute('function run(key, set, started, budget)\n' + loop + '\n return state.remaking[key] end')
L.execute('''
native_colour = true
state.remaking.K = {{1, 61}, {2, 62}, {3, 66}, {4, 64}}
local set = {}
busy_after = 2
local left = run('K', set, 0, 3)
assert(#left == 1 and set[4].texpos == 5064 and set[3].texpos == 7066 and set[2].texpos == 5062 and set[1] == nil, 'busy: ' .. #left)
busy_after = nil
left = run('K', set, 0, 3)
assert(#left == 0 and set[1].texpos == 5061 and lua_made == 1, 'rest')
native_colour = false
state.remaking.J = {{9, 69}}
local set2 = {}
run('J', set2, 0, 3)
assert(set2[9].texpos == 7069 and lua_made == 2, 'Lua path')
print('colour.lua sheet loop 15Z: all checks pass')
''')
