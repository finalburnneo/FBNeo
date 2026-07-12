# Irem drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Irem drivers'
group_srcs = [
    'irem_cpu.cpp',
    'irem_sound.cpp',
]
drivers = {
    'd_m107.cpp': ['ay8910', 'iremga20', 'm6800', 'msm5205', 'nec', 'ym2151', 'z80'],
    'd_m52.cpp': ['ay8910', 'm6800', 'msm5205', 'resnet', 'z80'],
    'd_m57.cpp': ['ay8910', 'm6800', 'msm5205', 'z80'],
    'd_m58.cpp': ['ay8910', 'm6800', 'msm5205', 'z80'],
    'd_m62.cpp': ['ay8910', 'm6800', 'msm5205', 'samples', 'z80'],
    'd_m63.cpp': ['ay8910', 'i8039', 'm6800', 'msm5205', 'z80'],
    'd_m72.cpp': ['ay8910', 'dac', 'i8051', 'm6800', 'msm5205', 'nec', 'ym2151', 'z80'],
    'd_m90.cpp': ['ay8910', 'dac', 'm6800', 'msm5205', 'nec', 'ym2151', 'z80'],
    'd_m92.cpp': ['ay8910', 'iremga20', 'm6800', 'msm5205', 'msm6295', 'nec', 'pic8259', 'ym2151', 'z80'],
    'd_travrusa.cpp': ['ay8910', 'm6800', 'msm5205', 'z80'],
    'd_vigilant.cpp': ['ay8910', 'dac', 'm6800', 'msm5205', 'ym2151', 'ym2203', 'z80'],
}
