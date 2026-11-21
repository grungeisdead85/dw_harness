# 15Z: snow_ramps.lua's edge art read once a world, and native_tables(true) baking nothing, in lupa (Lua 5.3)
import sys   # (argv[1]: the folder with native.lua, colour.lua and snow_ramps.lua)
import re
import lupa.lua53 as lupa
src = open(sys.argv[1] + '/snow_ramps.lua').read()
a = src.index('-- EDGE ART ONCE A WORLD')
b = src.index('\nend\n', src.index('function all_edge_art()')) + 5
memo_chunk = src[a:b]
m = re.search(r'^function native_tables\(.*?^end$', src, re.S | re.M)
tables_chunk = m.group(0)
L = lupa.LuaRuntime()
L.execute('''
SC_WORLD_UNLOADED, SC_MAP_UNLOADED, SC_MAP_LOADED = 1, 2, 3
dfhack = {onStateChange = {}, textures = {getTexposByHandle = function(h) return h end}}
GROUND_EDGE_IDS = {DIRT_FLOOR = 2}
finds, opens = 0, 0
grass = {find_edge_art = function() finds = finds + 1 return {[1] = {N = 5}} end,
         graphics_files = function(p) return {'a.txt'} end, GENERIC_PIECE = {N = 1}, page_texpos = function(p, x, y) return 77 end}
io = {open = function(p) opens = opens + 1 return {read = function() return '[TILE_GRAPHICS:P:1:2:DIRT_FLOOR_1]' end, close = function() end} end}
function require(n) return {get_active_mods = function() return {{path = 'm'}} end} end
''')
L.execute(memo_chunk)
L.execute('''
local e1 = all_edge_art()
local e2 = all_edge_art()
assert(e1 == e2 and finds == 1 and opens == 1, 'read again: ' .. finds .. ' ' .. opens)
assert(e1[2].N == 77 and e1[1].N == 5, 'edge art')
edge_art = e1
dfhack.onStateChange.detailed_weather_edge_art(SC_MAP_LOADED)
all_edge_art()
assert(finds == 1, 'let go on a load')
dfhack.onStateChange.detailed_weather_edge_art(SC_WORLD_UNLOADED)
assert(edge_art == nil, 'edge_art kept into another world')
all_edge_art()
assert(finds == 2, 'not read again in a new world')
print('snow_ramps.lua edge art 15Z: all checks pass')
''')
# native_tables(true): the start's bakes and the kept file left alone
L.execute('''
art, cell_of_byte, piece_sides, thin_layout = {}, {}, true, {}
thin_art = {pieces = {handles = {}}, inside = {handles = {}}}
edge_art = {[1] = {N = 5}}
frame_number = 0
function snowy_texpos() return 0 end
SNOW_PAGE = 'SNOW'
df = {global = {texture = {page = {}}, world = {raws = {plants = {all = {}}}}}}
FULL_PIECES = {}
pieces_for = {}
made, snow_made = {}, {}
indexed, baked = 0, 0
function name_edge_pieces() end
function index_snowy_edges() indexed = indexed + 1 return 3 end
function snowy_overhang(key, pieces, state) baked = baked + 1 return 900 end
''')
L.execute(tables_chunk)
L.execute('''
local t = native_tables(true)
assert(indexed == 0 and baked == 0 and t.edges_kept == 0 and next(t.edges_snowy) == nil, 'baked at start with the DLL baking')
local t2 = native_tables()
assert(indexed == 1 and baked == 1 and t2.edges_kept == 3 and t2.edges_snowy[1].N == 900, 'not baked without it')
print('snow_ramps.lua native_tables 15Z: all checks pass')
''')
