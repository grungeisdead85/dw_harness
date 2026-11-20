# Compares the DLL's painting with looks.lua's colour change (recolourer's closure and colour_only's
# rounding, run in Lua 5.3), on random pixels and colour changes. Doubles printed with %.17g.
import random, subprocess, lupa.lua53 as L
lua = L.LuaRuntime()
f = lua.eval(r'''
function(m, lean, has_lean, dark, lighten, px)
    local function lum_of(r, g, b) return r * 0.299 + g * 0.587 + b * 0.114 end
    local function balance(r, g, b, gains)
        local before = lum_of(r, g, b)
        local nr, ng, nb = r * gains[1], g * gains[2], b * gains[3]
        local after = lum_of(nr, ng, nb)
        local k = after > 0 and before / after or 1
        return nr * k, ng * k, nb * k
    end
    local leanv = has_lean and lean or nil
    local function recolour(r, g, b)
        local nr, ng, nb = m[1] * r + m[2] * g + m[3] * b, m[4] * r + m[5] * g + m[6] * b, m[7] * r + m[8] * g + m[9] * b
        if leanv then nr, ng, nb = balance(nr, ng, nb, leanv) end
        nr, ng, nb = nr * dark, ng * dark, nb * dark
        if lighten > 0 then
            return nr + (255 - nr) * lighten, ng + (255 - ng) * lighten, nb + (255 - nb) * lighten
        end
        return nr * (1 + lighten), ng * (1 + lighten), nb * (1 + lighten)
    end
    local out = {}
    for n = 1, #px do
        local v = px[n]
        local p1, p2, p3, p4 = v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24
        local nr, ng, nb = recolour(p1, p2, p3)
        nr = nr < 0 and 0 or nr > 255 and 255 or math.floor(nr + 0.5)
        ng = ng < 0 and 0 or ng > 255 and 255 or math.floor(ng + 0.5)
        nb = nb < 0 and 0 or nb > 255 and 255 or math.floor(nb + 0.5)
        out[n] = (p4 << 24) | (nb << 16) | (ng << 8) | nr
    end
    return out
end''')
random.seed(5)
cases, inp = [], []
for c in range(60):
    m = [random.uniform(-0.4, 1.4) for _ in range(9)]
    lean = [random.uniform(0.85, 1.15) for _ in range(3)]
    has = random.random() < 0.6
    dark = random.uniform(0.7, 1.0)
    lighten = random.uniform(-0.2, 0.2) if c % 3 else 0.0
    px = [random.getrandbits(32) for _ in range(1024)]
    if c == 0: px = [ (a << 24) | (b << 16) | (g << 8) | r for r in (0, 255) for g in (0, 128, 255) for b in (0, 255) for a in (0, 255)] * 43
    px = px[:1024]
    cases.append((m, lean, has, dark, lighten, px))
    inp.append(' '.join('%.17g' % v for v in m + lean) + ' %d %.17g %.17g ' % (1 if has else 0, dark, lighten) + ' '.join(map(str, px)))
out = subprocess.run(['./paintcheck'], input='\n'.join(inp), capture_output=True, text=True).stdout.split('\n')
got = [int(x) for x in out if x and not x.startswith('bad')]
bad = [x for x in out if x.startswith('bad')]
diff = 0
for c, (m, lean, has, dark, lighten, px) in enumerate(cases):
    want = list(f(lua.table(*m), lua.table(*lean), has, dark, lighten, lua.table(*px)).values())
    for n in range(1024):
        if want[n] != got[c * 1024 + n]: diff += 1
print('cases', len(cases), 'pixels differing', diff, bad)
