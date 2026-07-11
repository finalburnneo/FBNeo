# IGS PGM drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'IGS PGM drivers'
group_srcs = [
    'pgm_asic25.cpp',
    'pgm_asic27a_type1.cpp',
    'pgm_asic27a_type2.cpp',
    'pgm_asic27a_type3.cpp',
    'pgm_asic3.cpp',
    'pgm_crypt.cpp',
    'pgm_draw.cpp',
    'pgm_run.cpp',
]
drivers = {
    'd_pgm.cpp': ['arm7', 'ics2115', 'm68000', 'v3021', 'z80'],
}
