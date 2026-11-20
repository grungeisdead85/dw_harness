# Runs the real looks.lua (test 9's, with native_numbers) in Lua 5.3 with stand-ins for DFHack, and
# compares its redraw with the DLL's THE FULL REDRAW, pixel for pixel, for every class, look and
# transparency case. Textures: smooth noise (like ground), some with clear pixels.
import random, subprocess, math, lupa.lua53 as L
lua = L.LuaRuntime()
textures = {}
lua.globals().py_pixels = lambda t: (lua.table(*textures[t]) if t in textures else None)
lua.execute(r'''
dfhack = {textures = {loadTileset = function() return {1} end, getTexposByHandle = function() return 'grain' end}}
local grass_stub = {
    pixels_of = function(t)
        local words = py_pixels(t)
        if not words then return nil end
        local px = {}
        for n = 1, 1024 do local v = words[n] px[n] = {v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24} end
        return px
    end,
    original_address = function(t) return t == 'grain' and 1 or nil end,
}
function reqscript() return grass_stub end
''')
src = open('../../looks.lua').read()
env = lua.eval('function(src) local env = setmetatable({}, {__index = _G}); local f = assert(load(src, "looks", "t", env)); f(); return env end')(src)
random.seed(11)
def noise_texture(seed, clear):
    rnd = random.Random(seed)
    base = [rnd.randint(40, 200) for _ in range(3)]
    words = []
    for y in range(32):
        for x in range(32):
            v = [min(255, max(0, int(base[c] + 30 * math.sin((x + seed) * 0.4 + c) + 25 * math.cos((y * 0.3 + x * 0.1) + c * 2) + rnd.randint(-20, 20)))) for c in range(3)]
            a = 255
            if clear and (x + y * 2 + seed) % 7 == 0: a = 0
            if clear and (x * y + seed) % 11 == 0: a = rnd.randint(1, 254)
            words.append((a << 24) | (v[2] << 16) | (v[1] << 8) | v[0])
    return words
textures['grain'] = noise_texture(999, False)
cases, inp = [], []
classes = ['grass', 'dirt', 'sand', 'stone', 'pebbles', 'ice']
k = 0
for cls in classes:
    for temp in (-5, -3, -1, 0, 2, 4):
        for wet in (0, 1, 2, 3, 4):
            for clear in (False, True):
                k += 1
                tex = 'tex%d' % k
                textures[tex] = noise_texture(k, clear)
                look = lua.table_from({'temperature': temp, 'wet': wet})
                nn = env.native_numbers(cls, look)
                want = env.redraw(tex, cls, look, True)
                want = [want[n] for n in range(1, 1025)]
                m = [nn.m[i] for i in range(1, 10)]
                lean = nn.lean; soil = nn.soil
                lv = [lean[1], lean[2], lean[3]] if lean else [1, 1, 1]
                sv = [soil[1], soil[2], soil[3]] if soil else [1, 1, 1]
                ice_k = nn.ice_k or 0; ice_dark = nn.ice_dark or 1
                has_grain = 1 if nn.grain else 0
                fields = m + lv + [1 if lean else 0, nn.dark, nn.lighten, nn.cls] + sv + [1 if soil else 0, nn.weather, nn.dryness, nn.wet, nn.thaw, ice_k, ice_dark, has_grain]
                line = ' '.join(('%.17g' % f) if isinstance(f, float) else str(int(f)) for f in fields)
                line += ' ' + ' '.join(map(str, textures['grain'])) + ' ' + ' '.join(map(str, textures[tex]))
                inp.append(line)
                cases.append((cls, temp, wet, clear, want))
r = subprocess.run(['./fullcheck'], input='\n'.join(inp), capture_output=True, text=True)
out = r.stdout.split('\n')
got = [int(x) for x in out if x and not x.startswith('bad')]
times = [float(x) for x in r.stderr.split()]
bad_cases, diff_px = [], 0
for c, (cls, temp, wet, clear, want) in enumerate(cases):
    d = sum(1 for n in range(1024) if want[n] != got[c * 1024 + n])
    if d:
        diff_px += d
        bad_cases.append((cls, temp, wet, clear, d))
print('cases', len(cases), 'cases differing', len(bad_cases), 'pixels differing', diff_px, [x for x in out if x.startswith('bad')])
print('first differing:', bad_cases[:8])
print('C time per copy: mean %.1f us, worst %.1f us' % (sum(times) / len(times), max(times)))
