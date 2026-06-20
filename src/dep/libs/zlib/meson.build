zlib_sources = [
    'adler32.c',
    'compress.c',
    'crc32.c',
    'deflate.c',
    'gzclose.c',
    'gzlib.c',
    'gzread.c',
    'gzwrite.c',
    'infback.c',
    'inffast.c',
    'inflate.c',
    'inftrees.c',
    'trees.c',
    'uncompr.c',
    'zutil.c',
]

zlib_lib = static_library('zlib', zlib_sources)

zlib_dep = declare_dependency(
    link_with: zlib_lib,
    include_directories: include_directories('.'),
)
