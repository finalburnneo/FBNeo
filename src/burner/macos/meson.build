add_languages('objc', 'objcpp')

frontend_sources = files(
    'FBImporter.m',
    'FBMainThread+Etc.mm',
    'FBMainThread.mm',
    'FBScanner.mm',
    'NSString+Etc.m',
    'main.cpp',
    'misc.cpp',
)

frontend_dep = declare_dependency(
    include_directories: include_directories('.'),
    dependencies: [
        dependency('appleframeworks', modules: ['Cocoa', 'OpenGL']),
    ],
    compile_args: ['-DBUILD_MACOS'],
)

frontend_link_args = []
