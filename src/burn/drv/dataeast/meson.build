# Data East drivers - self-describing driver group fragment.
# group_srcs: shared support compiled whenever any driver here is selected.
# drivers: per-driver component lists (seeded from the old group set;
#          refine individually, then verify with test_per_driver.py).
# Add a driver: drop d_<name>.cpp here and add a 'd_<name>.cpp': [...] entry.

group_desc = 'Data East drivers'
group_srcs = [
    'deco146.cpp',
    'deco16ic.cpp',
    'decobac06.cpp',
]
drivers = {
    'd_actfancr.cpp': ['ay8910', 'h6280', 'm6502', 'msm6295', 'ym2151', 'ym2203', 'ym3812'],
    'd_backfire.cpp': ['arm', 'ay8910', 'eeprom', 'h6280', 'msm6295', 'ym2151', 'ym2203', 'ymz280b'],
    'd_boogwing.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_brkthru.cpp': ['ay8910', 'h6280', 'm6809', 'msm6295', 'ym2151', 'ym2203', 'ym3526'],
    'd_bwing.cpp': ['ay8910', 'dac', 'h6280', 'm6502', 'm6809', 'msm6295', 'ym2151', 'ym2203'],
    'd_cbuster.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_chanbara.cpp': ['ay8910', 'h6280', 'm6809', 'msm6295', 'ym2151', 'ym2203'],
    'd_cninja.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203', 'z80'],
    'd_compgolf.cpp': ['ay8910', 'h6280', 'm6809', 'msm6295', 'ym2151', 'ym2203'],
    'd_darkseal.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_dassault.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_dblewing.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203', 'z80'],
    'd_dec0.cpp': ['ay8910', 'h6280', 'i8051', 'm6502', 'm68000', 'msm6295', 'ym2151', 'ym2203', 'ym3812'],
    'd_dec8.cpp': ['ay8910', 'h6280', 'hd6309', 'i8051', 'm6502', 'm6809', 'msm5205', 'msm6295', 'ym2151', 'ym2203', 'ym3526', 'ym3812'],
    'd_deco156.cpp': ['arm', 'ay8910', 'eeprom', 'h6280', 'msm6295', 'ym2151', 'ym2203', 'ymz280b'],
    'd_deco32.cpp': ['arm', 'ay8910', 'bsmt2000', 'dac', 'decobsmt', 'eeprom', 'h6280', 'm6809', 'msm6295', 'tms32010', 'ym2151', 'ym2203', 'z80'],
    'd_decocass.cpp': ['ay8910', 'h6280', 'i8x41', 'm6502', 'msm6295', 'ym2151', 'ym2203'],
    'd_decomlc.cpp': ['arm', 'ay8910', 'eeprom', 'h6280', 'msm6295', 'sh2', 'ym2151', 'ym2203', 'ymz280b'],
    'd_dietgogo.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_dreambal.cpp': ['ay8910', 'eeprom', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_funkyjet.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_karnov.cpp': ['ay8910', 'h6280', 'i8051', 'm6502', 'm68000', 'msm6295', 'ym2151', 'ym2203', 'ym3526'],
    'd_kchamp.cpp': ['ay8910', 'dac', 'h6280', 'msm5205', 'msm6295', 'ym2151', 'ym2203', 'z80'],
    'd_lemmings.cpp': ['ay8910', 'h6280', 'm68000', 'm6809', 'msm6295', 'ym2151', 'ym2203'],
    'd_liberate.cpp': ['ay8910', 'h6280', 'm6502', 'msm6295', 'ym2151', 'ym2203'],
    'd_madmotor.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_metlclsh.cpp': ['ay8910', 'h6280', 'm6809', 'msm6295', 'ym2151', 'ym2203', 'ym3526'],
    'd_mirage.cpp': ['ay8910', 'eeprom', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_pcktgal.cpp': ['ay8910', 'h6280', 'm6502', 'msm5205', 'msm6295', 'ym2151', 'ym2203', 'ym3812'],
    'd_pktgaldx.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_progolf.cpp': ['ay8910', 'h6280', 'm6502', 'msm6295', 'ym2151', 'ym2203'],
    'd_rohga.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_shootout.cpp': ['ay8910', 'h6280', 'm6502', 'msm6295', 'ym2151', 'ym2203'],
    'd_sidepckt.cpp': ['ay8910', 'h6280', 'i8051', 'm6502', 'm6809', 'msm6295', 'ym2151', 'ym2203', 'ym3526'],
    'd_simpl156.cpp': ['arm', 'ay8910', 'eeprom', 'h6280', 'msm6295', 'ym2151', 'ym2203'],
    'd_sshangha.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203', 'z80'],
    'd_stadhero.cpp': ['ay8910', 'h6280', 'm6502', 'm68000', 'msm6295', 'ym2151', 'ym2203', 'ym3812'],
    'd_supbtime.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_tumblep.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
    'd_vaportra.cpp': ['ay8910', 'h6280', 'm68000', 'msm6295', 'ym2151', 'ym2203'],
}
