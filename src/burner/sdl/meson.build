frontend_sources = files(
    'bzip.cpp',
    'config.cpp',
    'drv.cpp',
    'inpdipsw.cpp',
    'input_sdl2.cpp',
    'ips_manager.cpp',
    'localise_gamelist.cpp',
    'main.cpp',
    'media.cpp',
    'neocdlist.cpp',
    'replay.cpp',
    'romdata.cpp',
    'run.cpp',
    'scrn.cpp',
    'sdl2_gui.cpp',
    'sdl2_gui_common.cpp',
    'sdl2_gui_ingame.cpp',
    'sdl2_inprint.cpp',
    'stated.cpp',
    'stringset.cpp',
    'support_paths.cpp',
    '../../intf/audio/sdl/aud_sdl.cpp',
    '../../intf/cd/sdl/cd_sdl2.cpp',
    '../../intf/input/sdl/inp_sdl2.cpp',
    '../../intf/video/sdl/vid_sdl2.cpp',
    '../../intf/video/sdl/vid_sdl2opengl.cpp',
)

sdl2_defs = ['-DBUILD_SDL2', '-DUSE_FILE32API', '-DHAVE_UNISTD_H']
if host_machine.system() == 'windows'
    sdl2_defs += ['-DSDL_WINDOWS']
endif

frontend_dep = declare_dependency(
    include_directories: include_directories(
        '.',
        '../../intf/input/sdl',
        '../../intf/audio/sdl',
        '../../intf/video/sdl',
        '../../intf/cd/sdl',
    ),
    dependencies: [
        dependency('SDL2_image'),
        dependency('opengl'),
    ],
    compile_args: sdl2_defs,
)

frontend_link_args = []
