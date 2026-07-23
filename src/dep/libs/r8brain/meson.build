r8brain_sources = [
    'fft/pffft.c',
    'fft/pffft_double.c',
]

r8brain_inc = [
    '.',
    'fft',
    'fft/simd',
]

r8brain_defs = []

if host_machine.system() == 'windows'
    r8brain_defs += '-D_USE_MATH_DEFINES'
endif

r8brain_lib = static_library(
    'r8brain',
    r8brain_sources,
    c_args: r8brain_defs,
    cpp_args: r8brain_defs,
)

r8brain_dep = declare_dependency(
    link_with: r8brain_lib,
    include_directories: include_directories(r8brain_inc),
)