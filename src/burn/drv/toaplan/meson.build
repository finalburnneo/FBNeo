# Toaplan drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Toaplan drivers'
group_srcs = [
    'toa_bcu2.cpp',
    'toa_extratext.cpp',
    'toa_gp9001.cpp',
    'toa_palette.cpp',
    'toaplan.cpp',
    'toaplan1.cpp',
]
drivers = {
    'd_batrider.cpp': ['m68000', 'msm6295', 'nmk112', 'ym2151', 'z80'],
    'd_batsugun.cpp': ['m68000', 'msm6295', 'nec', 'ym2151', 'z80'],
    'd_battleg.cpp': ['m68000', 'msm6295', 'nmk112', 'ym2151', 'z80'],
    'd_bbakraid.cpp': ['eeprom', 'm68000', 'ymz280b', 'z80'],
    'd_dogyuun.cpp': ['m68000', 'msm6295', 'nec', 'ym2151', 'z80'],
    'd_enmadaio.cpp': ['m68000', 'msm6295', 'ym2151', 'z80'],
    'd_fixeight.cpp': ['eeprom', 'm68000', 'msm6295', 'nec', 'ym2151', 'z80'],
    'd_ghox.cpp': ['m68000', 'ym2151', 'z180', 'z80'],
    'd_kbash.cpp': ['m68000', 'msm6295', 'nec', 'ym2151', 'z80'],
    'd_kbash2.cpp': ['m68000', 'msm6295', 'z80'],
    'd_mahoudai.cpp': ['m68000', 'msm6295', 'ym2151', 'z80'],
    'd_pipibibs.cpp': ['m68000', 'ym3812', 'z80'],
    'd_shippumd.cpp': ['m68000', 'msm6295', 'ym2151', 'z80'],
    'd_slapfght.cpp': ['ay8910', 'm68000', 'm6805', 'taito_m68705', 'z80'],
    'd_snowbro2.cpp': ['m68000', 'msm6295', 'ym2151', 'z80'],
    'd_tekipaki.cpp': ['m68000', 'ym3812', 'z80'],
    'd_toaplan1.cpp': ['m68000', 'tms32010', 'ym3812', 'z80'],
    'd_truxton2.cpp': ['m68000', 'msm6295', 'ym2151', 'z80'],
    'd_twincobr.cpp': ['m68000', 'tms32010', 'ym3812', 'z80'],
    'd_vfive.cpp': ['m68000', 'nec', 'ym2151', 'z80'],
    'd_wardner.cpp': ['m68000', 'tms32010', 'ym3812', 'z80'],
}
