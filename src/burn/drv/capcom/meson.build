# Capcom drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Capcom drivers'
group_srcs = [
    'cps.cpp',
    'cps2_crpt.cpp',
    'cps_config.cpp',
    'cps_draw.cpp',
    'cps_mem.cpp',
    'cps_obj.cpp',
    'cps_pal.cpp',
    'cps_run.cpp',
    'cps_rw.cpp',
    'cps_scr.cpp',
    'cpsr.cpp',
    'cpsrd.cpp',
    'cpst.cpp',
    'ctv.cpp',
    'fcrash_snd.cpp',
    'kabuki.cpp',
    'ps.cpp',
    'ps_m.cpp',
    'ps_z.cpp',
    'qs.cpp',
    'qs_c.cpp',
    'qs_z.cpp',
    'sf2mdt_snd.cpp',
]
drivers = {
    'd_cps1.cpp': ['ay8910', 'eeprom', 'm68000', 'msm5205', 'msm6295', 'samples', 'timekpr', 'ym2151', 'ym2203', 'z80'],
    'd_cps2.cpp': ['ay8910', 'eeprom', 'm68000', 'msm5205', 'msm6295', 'samples', 'ym2151', 'ym2203', 'z80'],
    'd_mitchell.cpp': ['ay8910', 'eeprom', 'm68000', 'msm5205', 'msm6295', 'samples', 'ym2151', 'ym2203', 'ym2413', 'z80'],
}
