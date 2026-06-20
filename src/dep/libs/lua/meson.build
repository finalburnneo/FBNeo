lua_sources = [
    'lapi.c',
    'lauxlib.c',
    'lbaselib.c',
    'lcode.c',
    'ldblib.c',
    'ldebug.c',
    'ldo.c',
    'ldump.c',
    'lfunc.c',
    'lgc.c',
    'linit.c',
    'liolib.c',
    'llex.c',
    'lmathlib.c',
    'lmem.c',
    'loadlib.c',
    'lobject.c',
    'lopcodes.c',
    'loslib.c',
    'lparser.c',
    'lstate.c',
    'lstring.c',
    'lstrlib.c',
    'ltable.c',
    'ltablib.c',
    'ltm.c',
    'lundump.c',
    'lvm.c',
    'lzio.c',
    'print.c',
]

lua_lib = static_library('lua', lua_sources)

lua_dep = declare_dependency(
    link_with: lua_lib,
    include_directories: include_directories('.'),
)
