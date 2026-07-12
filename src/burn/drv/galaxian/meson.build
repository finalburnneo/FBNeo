# Galaxian hardware drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Galaxian hardware drivers'
group_srcs = [
    'cclimber_audio.cpp',
    'gal_gfx.cpp',
    'gal_run.cpp',
    'gal_sound.cpp',
    'gal_stars.cpp',
]
drivers = {
    'd_119.cpp': ['8255ppi', 'ay8910', 'dac', 'digitalk', 'flt_rc', 'resnet', 's2650', 'sn76496', 'z80'],
    'd_galaxian.cpp': ['8255ppi', 'ay8910', 'dac', 'digitalk', 'flt_rc', 'resnet', 's2650', 'sn76496', 'z80'],
}
