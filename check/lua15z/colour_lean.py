# 15Z: colour.lua's build_lean, native and not, in lupa (Lua 5.3) with stubs
import sys   # (argv[1]: the folder with native.lua, colour.lua and snow_ramps.lua)
import re
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/colour.lua').read()
def grab(name):
    m = re.search(r'^local function ' + re.escape(name) + r'\(.*?^end$', src, re.S | re.M)
    assert m, name
    return m.group(0).replace('local function ', 'function ', 1)
def const(name): return re.search(r'^local ' + name + r' = [^\n]*', src, re.M).group(0).replace('local ', '', 1)
L = lupa.LuaRuntime()
L.execute('''
TILE = 32
clock = 0
dfhack = {getTickCount = function() return clock end,
          textures = {createTile = function(px, w, h) made = made + 1 return 700 + made end, getTexposByHandle = function(h) return h end}}
made = 0
state = {copies = {L = {[11] = {texpos = 101}, [12] = {texpos = 102}}}}
grass = {pixels_of = function(t) local px = {} for i = 1, 1024 do px[i] = {10, 20, 30, 255} end return px end}
sent = {}
busy_at = nil
function native_pixels(kind, t, amount, ux, uy)
    if busy_at and #sent + 1 == busy_at then busy_at = nil return nil, true end
    sent[#sent + 1] = {kind, t, amount, ux, uy}
    return {handle = 900 + #sent, texpos = 900 + #sent, native = true}
end
''')
for c in ('LEAN_STEPS', 'LEAN_MOST_PX', 'LEAN_REACH_MOST'): L.execute(const(c))
for f in ('clump_offset', 'leaning', 'build_lean'): L.execute(grab(f))
L.execute('''
-- native: every step of every grass texture sent as a job, from its copy, in order; a busy frame resumes there
native_colour = true
local l = {grass = {11, 12, 13}, position = 1, frames = {}, original = {}, look = 'L', ux = 0.7071, uy = 0.7071}
busy_at = 5
assert(build_lean(l, 0, 3) == false and #sent == 4, 'busy: ' .. #sent)
assert(build_lean(l, 0, 3) == true, 'not done')
assert(#sent == 20, 'jobs ' .. #sent)
for k = 1, 10 do
    local s = sent[k]
    assert(s[1] == 2 and s[2] == 101 and math.abs(s[3] - LEAN_MOST_PX * k / LEAN_STEPS) < 1e-12 and s[4] == 0.7071, 'job ' .. k)
    assert(l.frames[11][k].texpos == 900 + k and l.original[900 + k] == 11, 'frame ' .. k)
end
assert(sent[11][2] == 102 and l.frames[12][10].texpos == 920, 'second texture')
assert(l.frames[13] == nil, 'a texture with no copy given frames')
-- without the DLL: made here as before, from the copy's pixels
native_colour = false
sent = {}
local l2 = {grass = {11}, position = 1, frames = {}, original = {}, look = 'L', ux = 1, uy = 0}
assert(build_lean(l2, 0, 3) == true and #sent == 0 and made == 10 and l2.frames[11][10].texpos == 710, 'Lua path')
print('colour.lua build_lean 15Z: all checks pass')
''')
