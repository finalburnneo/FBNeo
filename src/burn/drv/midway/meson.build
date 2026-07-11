# Midway drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Midway drivers'
group_srcs = [
    'dcs2k.cpp',
    'ide.cpp',
    'midtunit.cpp',
    'midwayic.cpp',
    'midwunit.cpp',
    'narc_sound.cpp',
    'williams_adpcm.cpp',
    'williams_cvsd.cpp',
    'yawdim_sound.cpp',
]
drivers = {
    'd_kinst.cpp': ['6821pia', 'adsp2100', 'dac', 'hc55516', 'm6809', 'mips3', 'msm6295', 'tms34010', 'ym2151', 'z80'],
    'd_tunit.cpp': ['6821pia', 'adsp2100', 'dac', 'hc55516', 'm6809', 'msm6295', 'tms34010', 'ym2151', 'z80'],
    'd_wunit.cpp': ['6821pia', 'adsp2100', 'dac', 'hc55516', 'm6809', 'msm6295', 'tms34010', 'ym2151', 'z80'],
    'd_xunit.cpp': ['6821pia', 'adsp2100', 'dac', 'hc55516', 'm6809', 'msm6295', 'tms34010', 'ym2151', 'z80'],
    'd_yunit.cpp': ['6821pia', 'adsp2100', 'dac', 'hc55516', 'm6809', 'msm6295', 'tms34010', 'ym2151', 'z80'],
}
