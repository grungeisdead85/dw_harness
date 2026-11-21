# 16A: install.lua (THE HOOK DLL'S INSTALLER) run in lupa (Lua 5.3) over a real scratch folder: the mod's
# folder and DF's. Usage: python3 install.py <folder with install.lua>
import sys, os, tempfile
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/install.lua').read()
root = tempfile.mkdtemp()
mod = os.path.join(root, 'mod'); df = os.path.join(root, 'df')
os.makedirs(os.path.join(mod, 'scripts_modactive/internal/detailed_weather')); os.makedirs(df)
def put(path, data): open(path, 'wb').write(data)
def get(path): return open(path, 'rb').read() if os.path.exists(path) else None
L = lupa.LuaRuntime()
g = L.globals()
g.MOD, g.DF = mod, df
L.execute('''
prompts = {}
timeouts = {}
COLOR_WHITE, SC_MAP_LOADED = 15, 7
local real_require = require
function require(name)
    if name == 'gui.dialogs' then return {showMessage = function(title, text) prompts[#prompts + 1] = text end} end
    return real_require(name)
end
local function exists(p) local f = io.open(p, 'rb') if f then f:close() return true end return false end
dfhack = {
    findScript = function(name) return MOD .. '/scripts_modactive/' .. name .. '.lua' end,
    filesystem = {getInstallDir = function() return DF end, exists = exists, isfile = exists},
    onStateChange = {},
    timeout = function(n, unit, fn) timeouts[#timeouts + 1] = fn end,
    isMapLoaded = function() return false end,
}
dfhack_flags = {module = true}
io_open_real, os_remove_real = io.open, os.remove
''')
L.execute(src)
dll = os.path.join(df, 'dfhooks_detailed_weather.dll'); src_dll = os.path.join(mod, 'dfhooks_detailed_weather.dll')
ver = os.path.join(df, 'detailed_weather_hook_version.txt')
fails = 0
def check(cond, what):
    global fails
    if not cond: fails += 1; print('FAIL:', what)
def run(popup=True):
    r = g.check(popup)
    return tuple(r) if isinstance(r, tuple) else (r, None)
# no DLL in the mod's folder
what, why = run()
check(what is None and why == 'no DLL in the mod folder', 'missing source: %r %r' % (what, why))
# installed: copied, version noted, the prompt once
put(src_dll, b'DLL-A' * 1000)
what, why = run()
check(what == 'installed' and get(dll) == get(src_dll) and get(ver).strip() == b'37' and len(g.prompts) == 1, 'install: %r' % (what,))
# the same: current, no prompt
what, _ = run()
check(what == 'current' and len(g.prompts) == 1, 'current: %r' % (what,))
# a new build in the mod's folder: the old one set aside, the new one written
put(src_dll, b'DLL-B' * 900)
what, _ = run()
check(what == 'updated' and get(dll) == get(src_dll) and get(dll + '.old') == b'DLL-A' * 1000 and len(g.prompts) == 2, 'update: %r' % (what,))
# the next check clears the set-aside copy (nothing holds it here)
what, _ = run()
check(what == 'current' and get(dll + '.old') is None, 'set-aside left: %r' % (what,))
# a set-aside name still held (it can't be removed): the next free one is used
put(dll + '.old', b'held')
os.chmod(root, 0o755)
L.execute('os.remove = function(p) if p:sub(-4) == ".old" then return nil, "held" end return os_remove_real(p) end')
put(src_dll, b'DLL-C' * 800)
what, _ = run(False)
check(what == 'updated' and get(dll + '.old1') == b'DLL-B' * 900 and get(dll + '.old') == b'held' and get(dll) == get(src_dll), 'held name: %r' % (what,))
L.execute('os.remove = os_remove_real')
# a newer version in DF's folder: left alone
put(ver, b'99\n')
put(src_dll, b'DLL-D' * 700)
what, _ = run()
check(what == 'newer' and get(dll) == b'DLL-C' * 800, 'newer: %r' % (what,))
put(ver, b'37\n')
# a failed write: the old one put back, nothing half written
L.execute('io.open = function(p, m) if p:sub(-4) == ".new" then return nil, "no room" end return io_open_real(p, m) end')
what, why = run()
check(what is None and why == 'install folder not writable' and get(dll) == b'DLL-C' * 800 and get(dll + '.new') is None, 'failed write: %r %r' % (what, why))
L.execute('io.open = io_open_real')
# uninstall: moved aside, the version gone; again: not installed
os.remove(dll + '.old') if os.path.exists(dll + '.old') else None
os.remove(dll + '.old1') if os.path.exists(dll + '.old1') else None
ok = g.uninstall()
check(ok is True and get(dll) is None and get(ver) is None and get(dll + '.old') is not None, 'uninstall: %r' % (ok,))
r = g.uninstall()
check(tuple(r) == (False, 'not installed'), 'uninstall twice: %r' % (r,))
# the map load: checked once a session
L.execute('timeouts = {}')
g.dfhack.onStateChange['detailed_weather:install'](7)
check(len(g.timeouts) == 1, 'not scheduled at a map load')
L.execute('timeouts[1]()')
g.dfhack.onStateChange['detailed_weather:install'](7)
check(len(g.timeouts) == 1 and g._G.detailed_weather_install.checked is True, 'checked twice in a session')
check(get(os.path.join(df, 'detailed_weather_install.log')) is not None, 'no log')
print('install.lua: %s' % ('all checks pass' if not fails else '%d FAILED' % fails))
