project(
    'fbneo',
    ['c', 'cpp'],
    version: '1.0.0.3',
    default_options: ['c_std=c99', 'cpp_std=c++11'],
)

perl = find_program('perl', required: true)
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if cc.get_id() in ['gcc', 'clang']
    add_project_arguments('-D_GNU_SOURCE', language: 'c')
endif
if cpp.get_id() in ['gcc', 'clang']
    add_project_arguments('-D_GNU_SOURCE', language: 'cpp')
endif

if cpp.get_id() in ['gcc', 'clang']
    add_project_arguments('-Wno-write-strings', language: 'cpp')
elif cpp.get_id() == 'msvc'
    add_project_arguments('/Zc:strictStrings-', language: 'cpp')
endif

include_7z = get_option('include_7z')
frontend = get_option('frontend')
if frontend == 'auto'
    if host_machine.system() == 'windows'
        frontend = 'win32'
    elif host_machine.system() == 'darwin'
        frontend = 'macos'
    else
        frontend = 'sdl2'
    endif
endif

message('FBNeo frontend: ' + frontend)
force_pulse_audio = get_option('force_pulse_audio')
rom_verify = get_option('rom_verify')
fastcall_support = get_option('fastcall_support')

if fastcall_support
    fastcall_test = []
    warn_as_error_args = []

    if cc.get_id() in ['gcc', 'clang']
        fastcall_test = '''
        int __attribute__((fastcall)) test_fastcall(int a) { return a; }
        int main(void) { return test_fastcall(0); }
        '''

        warn_as_error_args = '-Werror'
    elif cc.get_id() == 'msvc'
        fastcall_test = '''
        int __fastcall test_fastcall(int a) { return a; }
        int main(void) { return test_fastcall(0); }
        '''

        warn_as_error_args = ['/WX']
    endif

    if not cc.compiles(
        fastcall_test,
        name: '__fastcall keyword (C)',
        args: warn_as_error_args,
    )
        add_project_arguments('-D__fastcall=', language: 'c')
    else
        add_project_arguments('-DFASTCALL', language: 'c')
    endif

    if not cpp.compiles(
        fastcall_test,
        name: '__fastcall keyword (C++)',
        args: warn_as_error_args,
    )
        add_project_arguments('-D__fastcall=', language: 'cpp')
    else
        add_project_arguments('-DFASTCALL', language: 'cpp')
    endif
endif

burn_driver_list = get_option('driver_list')

fs = import('fs')

# Collect each enabled hardware group's self-describing fragment.
# Each src/burn/drv/<group>/meson.build sets (data only, no targets):
#   group_desc / group_use (flat component names) / group_drivers (d_*.cpp) / group_srcs
driver_disable = get_option('driver_disable')
enabled_components = []
burn_drivers = {}                 # name -> { 'desc', 'drivers', 'srcs' }
driver_files_abs = []             # explicit d_*.cpp paths fed to gamelist.pl
seen_drivers = []                 # for driver_disable validation
foreach name : burn_driver_list
    assert(
        fs.is_dir('src/burn/drv' / name),
        'Unknown driver group in driver_list: ' + name,
    )
    subdir('src/burn/drv' / name)
    enabled_drivers = []
    foreach d : group_drivers
        seen_drivers += name / d
        if not driver_disable.contains(name / d)
            enabled_drivers += d
            driver_files_abs += meson.current_source_dir() / 'src/burn/drv' / name / d
        endif
    endforeach
    burn_drivers += {
        name: {'desc': group_desc, 'drivers': enabled_drivers, 'srcs': group_srcs},
    }
    foreach c : group_use
        if not enabled_components.contains(c)
            enabled_components += c
        endif
    endforeach
endforeach

foreach dd : driver_disable
    assert(seen_drivers.contains(dd), 'Unknown driver in driver_disable: ' + dd)
endforeach

# win32 always needs the 68000 core (matches original build)
if frontend == 'win32' and not enabled_components.contains('m68000')
    enabled_components += 'm68000'
endif

subdir('src/burn')
subdir('src/cpu')
subdir('src/burn/devices')
subdir('src/burn/snd')

# Every component named in a group_use must resolve to a known cpu/device/sound.
known_components = all_cpu.keys() + all_burn_devices.keys() + all_burn_sounds.keys()
foreach c : enabled_components
    assert(known_components.contains(c), 'Unknown component in a group_use: ' + c)
endforeach

subdir('src/burn/drv')

subdir('src/dep/libs/zlib')
subdir('src/dep/libs/lua')
subdir('src/dep/libs/libchdr')
if include_7z
    subdir('src/dep/libs/lib7z')
endif

subdir('src/burner')

fbneo_sources = []
build_details_exe = executable(
    'build_details',
    'src/dep/scripts/build_details.cpp',
    cpp_args: ['-D_UNICODE']
    + (host_machine.cpu_family() == 'x86_64' ? ['-DBUILD_X64_EXE'] : []),
    native: true,
)
build_details_h = custom_target(
    'build_details_h',
    output: 'build_details.h',
    command: [build_details_exe],
    capture: true,
)
fbneo_sources += build_details_h

if frontend == 'sdl2'
    subdir('src/burner/sdl')
elif frontend == 'win32'
    subdir('src/burner/win32')
elif frontend == 'qt'
    subdir('src/burner/qt')
elif frontend == 'macos'
    subdir('src/burner/macos')
elif frontend == 'pi'
    subdir('src/burner/pi')
elif frontend == 'psp'
    subdir('src/burner/psp')
endif

fbneo_sources += frontend_sources

defs = []
extra_deps = []
if include_7z
    fbneo_sources += ['src/burner/un7z.cpp']
    defs += ['-DINCLUDE_7Z_SUPPORT']
    extra_deps += lib7z_dep
endif
if force_pulse_audio
    defs += ['-DFORCE_PULSE_AUDIO']
endif
if rom_verify
    defs += ['-DROM_VERIFY']
endif
fbneo_includes = include_directories(
    [
        'src/intf',
        'src/intf/video',
        'src/intf/video/scalers',
        'src/intf/audio',
        'src/intf/input',
        'src/intf/cd',
        'src/intf/perfcount',
    ],
)

deps = [
    dependency('threads'),
]
if force_pulse_audio
    deps += [dependency('pulse-simple')]
endif

c_warning_args = []
cpp_warning_args = []
if cc.get_id() in ['gcc', 'clang']
    c_warning_args = [
        '-Wno-long-long',
        '-Wno-sign-compare',
        '-Wno-uninitialized',
        '-Wno-unused',
        '-Wno-conversion',
        '-Wno-attributes',
        '-Wno-unused-parameter',
        '-Wno-unused-value',
    ]
endif
if cpp.get_id() in ['gcc', 'clang']
    cpp_warning_args = [
        '-Wconversion',
        '-Winline',
        '-Wno-attributes',
        '-Wno-conversion',
        '-Wno-long-long',
        '-Wno-missing-braces',
        '-Wno-multichar',
        '-Wno-narrowing',
        '-Wno-sign-compare',
        '-Wno-unused-parameter',
        '-Wno-unused-value',
        '-Wno-write-strings',
        '-Wpointer-arith',
        '-Wundef',
        '-Wuninitialized',
        '-Wunknown-pragmas',
    ]
endif

link_args = []
if host_machine.system() != 'windows'
    link_args += ['-lm']
endif
link_args += frontend_link_args

executable(
    'fbneo',
    fbneo_sources,
    include_directories: [fbneo_includes],
    dependencies: deps
    + [
        cpu_dep,
        burn_dep,
        burn_devices_dep,
        burn_snd_dep,
        burn_drv_dep,
        burner_dep,
        frontend_dep,
    ]
    + extra_deps,
    c_args: defs + c_warning_args,
    cpp_args: defs + cpp_warning_args,
    link_args: link_args,
    win_subsystem: frontend == 'win32' ? 'windows' : 'console',
)
