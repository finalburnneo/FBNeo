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

# devices / burner / active-frontend headers are reached via the global
# include set, so this library only needs its own directory.
burn_inc = ['.']

# driver_files_abs is the explicit list of enabled d_*.cpp files collected by
# the root meson.build (respecting driver_list + driver_disable). gamelist.pl
# reads them from a manifest file (-f) rather than as command-line arguments,
# both to stay under the OS command-line length limit and to avoid any
# directory globbing.
driverlist_manifest_cd = configuration_data()
driverlist_manifest_cd.set('DRIVER_FILES', '\n'.join(driver_files_abs))
driverlist_manifest = configure_file(
    input: 'driverlist_manifest.txt.in',
    output: 'driverlist_manifest.txt',
    configuration: driverlist_manifest_cd,
)

driverlist_h = custom_target(
    'driverlist_h',
    output: 'driverlist.h',
    command: [
        perl,
        meson.current_source_dir() / '..' / 'dep' / 'scripts' / 'gamelist.pl',
        '-o', '@OUTPUT@',
        '-l', meson.current_build_dir() / 'gamelist.txt',
        '-f', driverlist_manifest,
    ],
)

burn_sources += driverlist_h

burn_defs = ['-DSUPPORT_SPLIT_DRIVER']
if host_machine.endian() == 'little'
    burn_defs += '-DLSB_FIRST'
endif

if get_option('buildtype').contains('debug')
    burn_defs += '-DFBNEO_DEBUG'
endif

# Active-frontend build macro(s), computed once in the root meson.build.
burn_defs += frontend_defs

if frontend == 'win32'
    burn_defs += [
        '-DWIN32',
        '-D_WIN32',
    ]
endif

burn_c_args = burn_defs
burn_cpp_args = burn_defs

burn_lib = static_library(
    'burn',
    burn_sources,
    include_directories: [include_directories(burn_inc), global_inc],
    c_args: burn_c_args,
    cpp_args: burn_cpp_args,
)

burn_dep = declare_dependency(
    link_with: burn_lib,
    include_directories: include_directories(burn_inc),
    compile_args: burn_defs,
)
