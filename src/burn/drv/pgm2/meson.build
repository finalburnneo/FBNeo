# IGS PGM2 drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'IGS PGM2 drivers'
group_srcs = [
    'pgm2_crypt.cpp',
    'pgm2_draw.cpp',
    'pgm2_run.cpp',
]
drivers = {
    'd_pgm2.cpp': ['arm9', 'ymz770'],
}
