# Sega Master System drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Sega Master System drivers'
group_srcs = [
    'sms.cpp',
    'smsfmintf.cpp',
    'smspio.cpp',
    'smsrender.cpp',
    'smssound.cpp',
    'smssystem.cpp',
    'smstms.cpp',
    'smsvdp.cpp',
]
drivers = {
    'd_sms.cpp': ['sn76496', 'ym2413', 'z80'],
}
