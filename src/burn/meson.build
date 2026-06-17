burn_sources = [
    'burn.cpp',
    'burn_bitmap.cpp',
    'burn_gun.cpp',
    'burn_led.cpp',
    'burn_memory.cpp',
    'burn_pal.cpp',
    'burn_sha1.cpp',
    'burn_shift.cpp',
    'burn_sound.cpp',
    'burn_sound_c.cpp',
    'cheat.cpp',
    'debug_track.cpp',
    'hiscore.cpp',
    'load.cpp',
    'tilemap_generic.cpp',
    'tiles_generic.cpp',
    'timer.cpp',
    'vector.cpp',
]

burn_inc = ['.', 'devices', '../burner']
if frontend == 'win32'
    # TODO: split dep
    burn_inc += '../burner/win32'
elif frontend == 'sdl2'
    burn_inc += '../burner/sdl'
endif

driver_dirs_abs = []
foreach dir : burn_driver_list
    driver_dirs_abs += meson.current_source_dir() / 'drv' / dir
endforeach

driverlist_h = custom_target(
    'driverlist_h',
    output: 'driverlist.h',
    command: [
        perl,
        meson.current_source_dir() / '..' / 'dep' / 'scripts' / 'gamelist.pl',
        '-o', '@OUTPUT@',
        '-l', meson.current_build_dir() / 'gamelist.txt',
    ]
    + driver_dirs_abs,
)

burn_sources += driverlist_h

burn_defs = ['-DSUPPORT_SPLIT_DRIVER']
if host_machine.endian() == 'little'
    burn_defs += '-DLSB_FIRST'
endif

if get_option('buildtype').contains('debug')
    burn_defs += '-DFBNEO_DEBUG'
endif

if frontend == 'win32'
    burn_defs += '-DBUILD_WIN32'
    burn_defs += '-DUNICODE'
    burn_defs += '-D_UNICODE'
elif frontend == 'macos'
    burn_defs += '-DBUILD_MACOS'
elif frontend == 'sdl2'
    burn_defs += '-DBUILD_SDL2'
elif frontend == 'qt'
    burn_defs += '-DBUILD_QT'
elif frontend == 'pi'
    burn_defs += '-DBUILD_PI'
elif frontend == 'psp'
    burn_defs += '-DBUILD_PSP'
endif

burn_c_args = burn_defs
burn_cpp_args = burn_defs

burn_lib = static_library(
    'burn',
    burn_sources,
    include_directories: include_directories(burn_inc),
    c_args: burn_c_args,
    cpp_args: burn_cpp_args,
)

burn_dep = declare_dependency(
    link_with: burn_lib,
    # devices inc for sh4 serflash interface
    # TODO: refactor the sh4 serflash to remove devices deps
    include_directories: include_directories(burn_inc),
    compile_args: burn_defs,
)
