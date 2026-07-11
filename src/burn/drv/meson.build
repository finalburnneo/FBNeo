# Accumulate burn driver sources from the per-group fragments collected by the
# root meson.build into `burn_drivers` (name -> { desc, drivers, srcs }). Each
# group's file names are relative to its own dir, so re-add the group prefix.
# Enabled/disabled selection already happened during collection in the root.
burn_drv_sources = ['d_parent.cpp']
burn_driver_status = {}
foreach name, g : burn_drivers
    burn_driver_status += {g['desc']: 'ON'}
    foreach src : g['drivers'] + g['srcs']
        burn_drv_sources += name / src
    endforeach
endforeach
summary(burn_driver_status, section: 'Burn drivers', list_sep: '')

ctv_make = executable('ctv_make', 'capcom/ctv_make.cpp', native: true)
pgm_sprite_create = executable(
    'pgm_sprite_create',
    'pgm/pgm_sprite_create.cpp',
    native: true,
)

ctv_h = custom_target('ctv_h', output: 'ctv.h', command: [ctv_make], capture: true)
pgm_sprite_h = custom_target(
    'pgm_sprite_h',
    output: 'pgm_sprite.h',
    command: [pgm_sprite_create],
    capture: true,
)

toa_gp9001_func_h = custom_target(
    'toa_gp9001_func_h',
    output: ['toa_gp9001_func.h', 'toa_gp9001_func_table.h'],
    command: [
        perl,
        meson.current_source_dir() / '..' / '..' / 'dep' / 'scripts' / 'toa_gp9001_func.pl',
        '-o', meson.current_build_dir() / 'toa_gp9001_func.h',
    ],
)
neo_sprite_func_h = custom_target(
    'neo_sprite_func_h',
    output: ['neo_sprite_func.h', 'neo_sprite_func_table.h'],
    command: [
        perl,
        meson.current_source_dir() / '..' / '..' / 'dep' / 'scripts' / 'neo_sprite_func.pl',
        '-o', meson.current_build_dir() / 'neo_sprite_func.h',
    ],
)
cave_tile_func_h = custom_target(
    'cave_tile_func_h',
    output: ['cave_tile_func.h', 'cave_tile_func_table.h'],
    command: [
        perl,
        meson.current_source_dir() / '..' / '..' / 'dep' / 'scripts' / 'cave_tile_func.pl',
        '-o', meson.current_build_dir() / 'cave_tile_func.h',
    ],
)
cave_sprite_func_h = custom_target(
    'cave_sprite_func_h',
    output: ['cave_sprite_func.h', 'cave_sprite_func_table.h'],
    command: [
        perl,
        meson.current_source_dir() / '..' / '..' / 'dep' / 'scripts' / 'cave_sprite_func.pl',
        '-o', meson.current_build_dir() / 'cave_sprite_func.h',
    ],
)
psikyo_tile_func_h = custom_target(
    'psikyo_tile_func_h',
    output: ['psikyo_tile_func.h', 'psikyo_tile_func_table.h'],
    command: [
        perl,
        meson.current_source_dir() / '..' / '..' / 'dep' / 'scripts' / 'psikyo_tile_func.pl',
        '-o', meson.current_build_dir() / 'psikyo_tile_func.h',
    ],
)

burn_drv_sources += [
    ctv_h,
    pgm_sprite_h,
    toa_gp9001_func_h,
    neo_sprite_func_h,
    cave_tile_func_h,
    cave_sprite_func_h,
    psikyo_tile_func_h,
]

burn_drv_defs = []
foreach name : burn_driver_list
    burn_drv_defs += '-DBUILD_' + name.to_upper()
endforeach

if host_machine.system() == 'windows'
    burn_drv_defs += '-D_USE_MATH_DEFINES'
    burn_drv_defs += '-DUSE_SPEEDHACKS'
endif

burn_drv_cpp_warning_args = []
if cpp.get_id() in ['gcc', 'clang']
    burn_drv_cpp_warning_args = [
        '-Wno-sequence-point',
        '-Wno-sign-compare',
        '-Wno-unused-but-set-variable',
        '-Wno-unused-parameter',
        '-Wno-unused-value',
        '-Wno-unused-variable',
    ]
endif

burn_drv_cpp_args = burn_drv_defs + burn_drv_cpp_warning_args

burn_drv_lib = static_library(
    'burn_drv',
    burn_drv_sources,
    # TODO: cd deps split
    include_directories: [
        'capcom',
        'cave',
        'dataeast',
        'galaxian',
        'irem',
        'konami',
        'neogeo',
        'psikyo',
        'sega',
        'taito',
        'toaplan',
        '../../intf/audio',
        '../../intf/cd',
        '../../burner',
    ],
    dependencies: [burn_dep, burn_devices_dep, burn_snd_dep, cpu_dep],
    cpp_args: burn_drv_cpp_args,
)

burn_drv_dep = declare_dependency(
    link_with: burn_drv_lib,
    include_directories: include_directories('.'),
    compile_args: burn_drv_defs,
)
