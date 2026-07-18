# Nintendo SNES drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Nintendo SNES drivers'
group_srcs = [
    'apu.cpp',
    'cart.cpp',
    'cpu.cpp',
    'cpu_sa1.cpp',
    'cx4.cpp',
    'dma.cpp',
    'dsp.cpp',
    'epsonrtc.cpp',
    'gsu.cpp',
    'input.cpp',
    'ppu.cpp',
    'sa1.cpp',
    'sdd1.cpp',
    'spc7110.cpp',
    'snes.cpp',
    'snes_other.cpp',
    'spc.cpp',
    'statehandler.cpp',
]
drivers = {
    'd_snes.cpp': ['upd7725'],
}
