# MSX drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'MSX drivers'
group_srcs = []
drivers = {
    'd_msx.cpp': ['8255ppi', 'ay8910', 'dac', 'k051649', 'tms9928a', 'z80'],
}
