# SNK Neo Geo drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'SNK Neo Geo drivers'
group_srcs = [
    'neo_decrypt.cpp',
    'neo_palette.cpp',
    'neo_run.cpp',
    'neo_sprite.cpp',
    'neo_text.cpp',
    'neo_upd4990a.cpp',
    'neogeo.cpp',
]
drivers = {
    'd_neogeo.cpp': ['ay8910', 'm68000', 'resnet', 'ym2610', 'z80'],
}
