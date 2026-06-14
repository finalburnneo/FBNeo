frontend_sources = files(
    '../inputbuf.cpp',
    'main_pi.cpp',
    'run_pi.cpp',
    '../sdl/bzip.cpp',
    '../sdl/config.cpp',
    '../sdl/drv.cpp',
    '../sdl/inpdipsw.cpp',
    '../sdl/ips_manager.cpp',
    '../sdl/media.cpp',
    '../sdl/replay.cpp',
    '../sdl/romdata.cpp',
    '../sdl/scrn.cpp',
    '../sdl/stated.cpp',
    '../sdl/stringset.cpp',
    '../sdl/support_paths.cpp',
    '../../dep/pi/gles/pigl_drm.c',
    '../../intf/audio/sdl/aud_sdl.cpp',
    '../../intf/cd/sdl/cd_sdl2.cpp',
    '../../intf/input/pi/inp_pi.cpp',
    '../../intf/video/pi/vid_pi.cpp',
)

frontend_dep = declare_dependency(
    include_directories: include_directories(
        '.',
        '../sdl',
        '../../dep/pi/gles',
        '../../intf/input/pi',
        '../../intf/video/pi',
    ),
    compile_args: ['-DBUILD_SDL', '-DBUILD_PI'],
)

frontend_link_args = []
