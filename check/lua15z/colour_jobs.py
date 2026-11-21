# 15Z: colour.lua's native_pixels and native_job (its kind and blank number) run in lupa (Lua 5.3) with stubs
import sys   # (argv[1]: the folder with native.lua, colour.lua and snow_ramps.lua)
import re
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/colour.lua').read()
a = src.index('local NATIVE_LEANS_PER_FRAME')
b = src.index('\nend\n', src.index('function native_pixels(')) + 5
pixels_chunk = src[a:b]
m = re.search(r'^local function native_job\(.*?^end$', src, re.S | re.M)
job_chunk = m.group(0).replace('local function native_job', 'function native_job', 1)
L = lupa.LuaRuntime()
L.execute('''
MEM = {}
local SIZES = {uint64_t = 8, uint32_t = 4, double = 8, int32_t = 4}
df = {reinterpret_cast = function(t, a)
    local n = SIZES[t]
    return setmetatable({}, {__index = function(_, k) if k == 'value' then k = 0 end return MEM[a + k * n] end,
                             __newindex = function(_, k, v) if k == 'value' then k = 0 end MEM[a + k * n] = {t, v} end})
end}
function W(t, a) local e = MEM[a] assert(e, 'nothing at ' .. a) assert(e[1] == t, 'type at ' .. a .. ': ' .. e[1]) return e[2] end
grass = {original_address = function(t) if t == 13 then return nil end return t * 4096 end}
native_counts = {}
native_pool = {}
for i = 1, 600 do native_pool[i] = {handle = i, texpos = 50000 + i, address = 9000000 + i * 4096} end
PAINT_JOB_BYTES = 256
NATIVE_FULL_PER_FRAME = 16
frame_number = 1
native_view = {paint_head = {value = 0}, paint_tail = {value = 0}, paint_base = 100000, paint_ring = 256}
function paint_numbers(class, look) return {cls = 0} end
function write_numbers(job, p) end
''')
L.execute(pixels_chunk)
L.execute(job_chunk)
L.execute('''
local nv = native_view
-- a lean frame: the job's fields where the DLL reads them
local f, busy = native_pixels(2, 7, 4.2, 0.7071, -0.7071)
assert(f and not busy and f.texpos == 50600 and f.native, 'lean job')
assert(W('uint64_t', 100000) == 7 * 4096 and W('uint64_t', 100008) == 9000000 + 600 * 4096, 'src, dst')
assert(W('double', 100016) == 4.2 and W('double', 100024) == 0.7071 and W('double', 100032) == -0.7071, 'amount, way')
assert(W('uint32_t', 100248) == 2 and W('uint32_t', 100252) == 0, 'kind, dst_tex')
assert(nv.paint_head.value == 1, 'head')
-- an unreadable texture: nil, not busy, nothing taken
local g, b2 = native_pixels(1, 13)
assert(g == nil and not b2 and nv.paint_head.value == 1 and #native_pool == 599, 'unreadable')
-- this frame's share: 24 lean frames, then busy; 128 alpha jobs, apart
for k = 2, 24 do assert(native_pixels(2, 7, 1, 1, 0), 'lean ' .. k) end
local h, b3 = native_pixels(2, 7, 1, 1, 0)
assert(h == nil and b3, 'lean share not kept')
for k = 1, 128 do assert(native_pixels(1, 8), 'alpha ' .. k) end
local h2, b4 = native_pixels(1, 8)
assert(h2 == nil and b4, 'alpha share not kept')
local at = 100000 + ((nv.paint_head.value - 1) % 256) * 256
assert(W('uint32_t', at + 248) == 1 and W('double', at + 16) == 0, 'alpha job')
-- the next frame: sent again
frame_number = 2
assert(native_pixels(2, 7, 1, 1, 0), 'next frame')
-- the ring full: busy
nv.paint_tail.value = nv.paint_head.value - 256
local h3, b5 = native_pixels(1, 9)
assert(h3 == nil and b5, 'ring full')
nv.paint_tail.value = nv.paint_head.value
-- a look's redraw: kind 0, and the blank's number only for a screen copy
local c = native_job(21, 'grass', {}, nil, true)
local at2 = 100000 + ((nv.paint_head.value - 1) % 256) * 256
assert(c and W('uint32_t', at2 + 248) == 0 and W('uint32_t', at2 + 252) == c.texpos, 'screen copy job')
local c2 = native_job(22, 'grass', {}, nil, nil)
local at3 = 100000 + ((nv.paint_head.value - 1) % 256) * 256
assert(c2 and W('uint32_t', at3 + 248) == 0 and W('uint32_t', at3 + 252) == 0, 'table copy job')
print('colour.lua 15Z: all checks pass')
''')
