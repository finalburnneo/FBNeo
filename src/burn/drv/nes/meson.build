# Nintendo NES drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Nintendo NES drivers'
group_srcs = [
    'nes.cpp',
]
drivers = {
    'd_nes.cpp': ['ay8910', 'm6502', 'nes_apu', 'ym2413'],
}
