libchdr_sources = [
    'libchdr_bitstream.c',
    'libchdr_cdrom.c',
    'libchdr_chd.c',
    'libchdr_codec_cdfl.c',
    'libchdr_codec_cdlz.c',
    'libchdr_codec_cdzl.c',
    'libchdr_codec_cdzs.c',
    'libchdr_codec_flac.c',
    'libchdr_codec_huff.c',
    'libchdr_codec_lzma.c',
    'libchdr_codec_zlib.c',
    'libchdr_codec_zstd.c',
    'libchdr_flac.c',
    'libchdr_huffman.c',
]

libchdr_lib = static_library(
    'libchdr',
    libchdr_sources,
    include_directories: [
        include_directories('.', 'include'),
        include_directories('../zlib'),
        include_directories('../lzma'),
        include_directories('../lzma/include'),
        include_directories('../zstd'),
    ],
    c_args: ['-DCHDR_SYSTEM_ZLIB'],
    dependencies: [zlib_dep],
)

libchdr_dep = declare_dependency(
    link_with: libchdr_lib,
    include_directories: [
        include_directories('.', 'include'),
    ],
    dependencies: [zlib_dep],
)
