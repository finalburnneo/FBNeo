# Sega Mega Drive drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Sega Mega Drive drivers'
group_srcs = [
    'megadrive.cpp',
    'stm95.cpp',
]
drivers = {
    'd_mdarcbl.cpp': ['burn_md2612', 'i2ceeprom', 'm68000', 'samples', 'sn76496', 'ym2612', 'z80'],
    'd_megadrive.cpp': ['burn_md2612', 'i2ceeprom', 'm68000', 'samples', 'sn76496', 'ym2612', 'z80'],
}
