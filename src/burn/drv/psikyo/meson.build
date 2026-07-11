# Psikyo drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Psikyo drivers'
group_srcs = [
    'psikyo_palette.cpp',
    'psikyo_sprite.cpp',
    'psikyo_tile.cpp',
    'psikyosh_render.cpp',
]
drivers = {
    'd_psikyo.cpp': [
        'm68000',
        'sh2',
        'z80',
        'eeprom',
        'msm6295',
        'ym2151',
        'ym2610',
        'ymf278b',
    ],
    'd_psikyo4.cpp': [
        'm68000',
        'sh2',
        'z80',
        'eeprom',
        'msm6295',
        'ym2151',
        'ym2610',
        'ymf278b',
    ],
    'd_psikyosh.cpp': [
        'm68000',
        'sh2',
        'z80',
        'eeprom',
        'msm6295',
        'ym2151',
        'ym2610',
        'ymf278b',
    ],
}
