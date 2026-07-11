# Capcom CPS-3 drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Capcom CPS-3 drivers'
group_srcs = [
    'cps3run.cpp',
    'cps3snd.cpp',
]
drivers = {
    'd_cps3.cpp': ['sh2'],
}
