# Cave drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Cave drivers'
group_srcs = [
    'cave.cpp',
    'cave_palette.cpp',
    'cave_sprite.cpp',
    'cave_tile.cpp',
]
drivers = {
    'd_cv1k.cpp': ['epic12', 'm68000', 'rtc9701', 'serflash', 'sh4', 'ymz770'],
    'd_dodonpachi.cpp': ['eeprom', 'm68000', 'ymz280b'],
    'd_donpachi.cpp': ['eeprom', 'm68000', 'msm6295', 'nmk112', 'samples'],
    'd_esprade.cpp': ['eeprom', 'm68000', 'ymz280b'],
    'd_feversos.cpp': ['eeprom', 'm68000', 'ymz280b'],
    'd_gaia.cpp': ['m68000', 'ymz280b'],
    'd_guwange.cpp': ['eeprom', 'm68000', 'ymz280b'],
    'd_hotdogst.cpp': ['ay8910', 'eeprom', 'm68000', 'msm6295', 'ym2203', 'z80'],
    'd_korokoro.cpp': ['eeprom', 'm68000', 'ymz280b'],
    'd_mazinger.cpp': ['ay8910', 'eeprom', 'm68000', 'msm6295', 'watchdog', 'ym2203', 'z80'],
    'd_metmqstr.cpp': ['eeprom', 'm68000', 'msm6295', 'ym2151', 'z80'],
    'd_pwrinst2.cpp': ['ay8910', 'eeprom', 'm68000', 'msm6295', 'nmk112', 'ym2203', 'z80'],
    'd_sailormn.cpp': ['eeprom', 'm68000', 'msm6295', 'ym2151', 'z80'],
    'd_tjumpman.cpp': ['eeprom', 'm68000', 'msm6295'],
    'd_uopoko.cpp': ['eeprom', 'm68000', 'ymz280b'],
}
