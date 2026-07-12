burn_snd_sources = []

all_burn_sounds = {
    'asteroids': {'desc': 'Atari Asteroids discrete sound', 'sources': ['asteroids.cpp']},
    'astrocde': {'desc': 'Bally Astrocade sound', 'sources': ['astrocde_snd.cpp']},
    'ay8910': {'desc': 'AY-3-8910 PSG', 'sources': []},
    'beep': {'desc': 'Beep speaker', 'sources': ['beep.cpp']},
    'burn_md2612': {'desc': 'Mega Drive YM2612 wrapper', 'sources': ['burn_md2612.cpp']},
    'burn_ymf262': {'desc': 'Yamaha YMF262 wrapper', 'sources': ['burn_ymf262.cpp']},
    'burn_ymf271': {'desc': 'Yamaha YMF271 wrapper', 'sources': ['burn_ymf271.cpp']},
    'bzone': {'desc': 'Atari Battlezone discrete sound', 'sources': ['bzone.cpp']},
    'c140': {'desc': 'Namco C140 PCM', 'sources': ['c140.cpp']},
    'c352': {'desc': 'Namco C352 PCM', 'sources': ['c352.cpp']},
    'c6280': {'desc': 'Hudson C6280 PSG', 'sources': ['c6280.cpp']},
    'dac': {'desc': 'Generic DAC', 'sources': ['dac.cpp']},
    'digitalk': {'desc': 'Digitalker speech', 'sources': ['digitalk.cpp']},
    'es5506': {'desc': 'Ensoniq ES5506 wavetable', 'sources': ['es5506.cpp']},
    'es8712': {'desc': 'ES8712 ADPCM', 'sources': ['es8712.cpp']},
    'exidy440': {'desc': 'Exidy 440 sound', 'sources': ['exidy440_snd.cpp']},
    'flower': {'desc': 'Flower custom sound', 'sources': ['flower.cpp']},
    'flt_rc': {'desc': 'RC filter', 'sources': ['flt_rc.cpp']},
    'fmopl': {'desc': 'Yamaha OPL core', 'sources': ['fmopl.c']},
    'gaelco': {'desc': 'Gaelco sound', 'sources': ['gaelco.cpp']},
    'hc55516': {'desc': 'HC55516 CVSD decoder', 'sources': ['hc55516.cpp']},
    'i5000': {'desc': 'I5000 sound device', 'sources': ['i5000.cpp']},
    'ics2115': {'desc': 'ICS2115 wavetable', 'sources': ['ics2115.cpp']},
    'iremga20': {'desc': 'Irem GA20 PCM', 'sources': ['iremga20.cpp']},
    'k005289': {'desc': 'Konami K005289 SCC', 'sources': ['k005289.cpp']},
    'k007232': {'desc': 'Konami K007232 PCM', 'sources': ['k007232.cpp']},
    'k051649': {'desc': 'Konami K051649 SCC', 'sources': ['k051649.cpp']},
    'k053260': {'desc': 'Konami K053260 PCM', 'sources': ['k053260.cpp']},
    'k054539': {'desc': 'Konami K054539 PCM', 'sources': ['k054539.cpp']},
    'llander': {
        'desc': 'Atari Lunar Lander discrete sound',
        'sources': ['llander.cpp'],
    },
    'mpeg_audio': {'desc': 'MPEG audio decoder', 'sources': ['mpeg_audio.cpp']},
    'msm5205': {'desc': 'OKI MSM5205 ADPCM', 'sources': ['msm5205.cpp']},
    'msm5232': {'desc': 'OKI MSM5232 tone generator', 'sources': ['msm5232.cpp']},
    'msm6295': {'desc': 'OKI MSM6295 ADPCM', 'sources': ['msm6295.cpp']},
    'multipcm': {'desc': 'Sega MultiPCM', 'sources': ['multipcm.cpp']},
    'namco_snd': {'desc': 'Namco WSG sound', 'sources': ['namco_snd.cpp']},
    'nes_apu': {'desc': 'NES APU', 'sources': ['nes_apu.cpp']},
    'phoenixsound': {'desc': 'Phoenix sound hardware', 'sources': ['phoenixsound.cpp']},
    'pleiadssound': {'desc': 'Pleiads sound hardware', 'sources': ['pleiadssound.cpp']},
    'pokey': {'desc': 'Atari POKEY', 'sources': ['pokey.cpp']},
    'redbaron': {'desc': 'Atari Red Baron discrete sound', 'sources': ['redbaron.cpp']},
    'rf5c68': {'desc': 'Ricoh RF5C68 PCM', 'sources': ['rf5c68.cpp']},
    's14001a': {'desc': 'TMS/SI S14001A speech', 'sources': ['s14001a.cpp']},
    'saa1099': {'desc': 'Philips SAA1099 PSG', 'sources': ['saa1099.cpp']},
    'samples': {'desc': 'Sample playback', 'sources': ['samples.cpp']},
    'segapcm': {'desc': 'Sega PCM', 'sources': ['segapcm.cpp']},
    'sn76477': {
        'desc': 'TI SN76477 complex sound generator',
        'sources': ['sn76477.cpp'],
    },
    'sn76496': {'desc': 'TI SN76489/SN76496 PSG', 'sources': ['sn76496.cpp']},
    'snk6502_sound': {'desc': 'SNK 6502 sound board', 'sources': ['snk6502_sound.cpp']},
    'sp0250': {'desc': 'GI SP0250 speech', 'sources': ['sp0250.cpp']},
    'sp0256': {'desc': 'GI SP0256 speech', 'sources': ['sp0256.cpp']},
    't6w28': {'desc': 'Toshiba T6W28 PSG', 'sources': ['t6w28.cpp']},
    'tiamc1': {'desc': 'TI AMC1 sound hardware', 'sources': ['tiamc1_snd.cpp']},
    'timeplt_snd': {
        'desc': 'Time Pilot/Pooyan/Rally-X sound board',
        'sources': ['timeplt_snd.cpp'],
    },
    'tms36xx': {'desc': 'TI TMS36xx melody generator', 'sources': ['tms36xx.cpp']},
    'tms5110': {'desc': 'TI TMS5110 speech', 'sources': ['tms5110.cpp']},
    'tms5220': {'desc': 'TI TMS5220 speech', 'sources': ['tms5220.cpp']},
    'upd7759': {'desc': 'NEC uPD7759 ADPCM', 'sources': ['upd7759.cpp']},
    'vlm5030': {'desc': 'Sanyo VLM5030 speech', 'sources': ['vlm5030.cpp']},
    'votrax': {'desc': 'Votrax SC-01 speech', 'sources': ['votrax.cpp']},
    'wiping': {'desc': 'Wiping custom sound', 'sources': ['wiping.cpp']},
    'x1010': {'desc': 'Namco CUS30/X1010 sound', 'sources': ['x1010.cpp']},
    'y8950': {'desc': 'Yamaha Y8950 core', 'sources': []},
    'ym2151': {'desc': 'Yamaha YM2151 core', 'sources': []},
    'ym2203': {'desc': 'Yamaha YM2203 core', 'sources': []},
    'ym2413': {'desc': 'Yamaha YM2413 core', 'sources': []},
    'ym2608': {'desc': 'Yamaha YM2608 core', 'sources': []},
    'ym2610': {'desc': 'Yamaha YM2610 core', 'sources': []},
    'ym2612': {'desc': 'Yamaha YM2612 core', 'sources': []},
    'ym3438': {'desc': 'Yamaha YM3438 core', 'sources': []},
    'ym3526': {'desc': 'Yamaha YM3526 core', 'sources': []},
    'ym3812': {'desc': 'Yamaha YM3812 core', 'sources': []},
    'ymf262': {'desc': 'Yamaha YMF262 core', 'sources': ['ymf262.cpp']},
    'ymf271': {'desc': 'Yamaha YMF271 core', 'sources': ['ymf271.cpp']},
    'ymf278b': {'desc': 'Yamaha YMF278B core', 'sources': []},
    'ymz280b': {'desc': 'Yamaha YMZ280B PCM', 'sources': ['ymz280b.cpp']},
    'ymz770': {'desc': 'Yamaha YMZ770 sound', 'sources': []},
}

# Enablement is derived from the parent's flat `enabled_components` set
# (sound names are unique across cpu/device/sound namespaces).
burn_sound_status = {}
burn_snd_defs = []
foreach name, sound : all_burn_sounds
    enabled = enabled_components.contains(name)
    set_variable('snd_@0@_enable'.format(name), enabled)
    burn_sound_status += {sound['desc']: enabled ? 'ON' : 'OFF'}
    if enabled
        burn_snd_sources += sound['sources']
    endif
endforeach

summary(burn_sound_status, section: 'Burn sound devices', list_sep: '')

snd_fm_enable = false
snd_fmopl_enable = false
snd_mpeg_audio_enable = false

# ---------------------------------------------------------------------------
# Yamaha Y8950
# ---------------------------------------------------------------------------
if snd_y8950_enable
    burn_snd_sources += 'burn_y8950.cpp'
    snd_fmopl_enable = true
    burn_snd_defs += '-DHAS_Y8950=1'
else
    burn_snd_defs += '-DHAS_Y8950=0'
endif

# ---------------------------------------------------------------------------
# Yamaha YM2151
# ---------------------------------------------------------------------------
if snd_ym2151_enable
    burn_snd_sources += ['burn_ym2151.cpp', 'ym2151.c']
endif

# ---------------------------------------------------------------------------
# Yamaha YM2203
# ---------------------------------------------------------------------------
if snd_ym2203_enable
    burn_snd_sources += 'burn_ym2203.cpp'
    snd_fm_enable = true
    burn_snd_defs += '-DHAS_YM2203=1'
else
    burn_snd_defs += '-DHAS_YM2203=0'
endif

# ---------------------------------------------------------------------------
# Yamaha YM2413
# ---------------------------------------------------------------------------
if snd_ym2413_enable
    burn_snd_sources += ['burn_ym2413.cpp', 'ym2413.c']
endif

# ---------------------------------------------------------------------------
# Yamaha YM2608
# ---------------------------------------------------------------------------
if snd_ym2608_enable
    burn_snd_sources += 'burn_ym2608.cpp'
    snd_fm_enable = true
    burn_snd_defs += '-DHAS_YM2608=1'
else
    burn_snd_defs += '-DHAS_YM2608=0'
endif

# ---------------------------------------------------------------------------
# Yamaha YM2610
# ---------------------------------------------------------------------------
if snd_ym2610_enable
    burn_snd_sources += 'burn_ym2610.cpp'
    snd_fm_enable = true
    burn_snd_defs += ['-DHAS_YM2610=1', '-DHAS_YM2610B=1']
else
    burn_snd_defs += ['-DHAS_YM2610=0', '-DHAS_YM2610B=0']
endif

# ---------------------------------------------------------------------------
# Yamaha YM2612
# ---------------------------------------------------------------------------
if snd_ym2612_enable or snd_ym3438_enable
    burn_snd_sources += ['ym2612.c', 'burn_ym2612.cpp']
    snd_fm_enable = true
    burn_snd_defs += '-DHAS_YM2612=1'
    burn_snd_defs += '-DHAS_YM3438=1'
else
    burn_snd_defs += '-DHAS_YM2612=0'
    burn_snd_defs += '-DHAS_YM3438=0'
endif

# ---------------------------------------------------------------------------
# Yamaha YM3526
# ---------------------------------------------------------------------------
if snd_ym3526_enable
    burn_snd_sources += 'burn_ym3526.cpp'
    snd_fmopl_enable = true
    burn_snd_defs += '-DHAS_YM3526=1'
else
    burn_snd_defs += '-DHAS_YM3526=0'
endif

# ---------------------------------------------------------------------------
# Yamaha YM3812
# ---------------------------------------------------------------------------
if snd_ym3812_enable
    burn_snd_sources += 'burn_ym3812.cpp'
    snd_fmopl_enable = true
    burn_snd_defs += '-DHAS_YM3812=1'
else
    burn_snd_defs += '-DHAS_YM3812=0'
endif

# ---------------------------------------------------------------------------
# Yamaha YMF278B
# ---------------------------------------------------------------------------
if snd_ymf278b_enable
    burn_snd_sources += ['burn_ymf278b.cpp', 'ymf278b.c']
endif

# ---------------------------------------------------------------------------
# Yamaha YMZ770
# ---------------------------------------------------------------------------
if snd_ymz770_enable
    burn_snd_sources += 'ymz770.cpp'
    snd_mpeg_audio_enable = true
endif

# ---------------------------------------------------------------------------
# AY-3-8910 PSG
# ---------------------------------------------------------------------------
if snd_ay8910_enable or snd_fm_enable
    burn_snd_sources += 'ay8910.c'
    snd_fm_enable = true
endif

# ---------------------------------------------------------------------------
# Yamaha FM core
# ---------------------------------------------------------------------------
if snd_fm_enable
    burn_snd_sources += 'fm.c'
endif

# ---------------------------------------------------------------------------
# Yamaha OPL core
# ---------------------------------------------------------------------------
if snd_fmopl_enable
    burn_snd_sources += 'fmopl.c'
endif

# ---------------------------------------------------------------------------
# Yamaha Delta-T ADPCM core
# ---------------------------------------------------------------------------
if snd_fm_enable or snd_fmopl_enable
    burn_snd_sources += 'ymdeltat.c'
endif

# ---------------------------------------------------------------------------
# MPEG audio decoder
# ---------------------------------------------------------------------------
if snd_mpeg_audio_enable
    burn_snd_sources += 'mpeg_audio.cpp'
endif

burn_snd_inc = ['../../dep/libs/dr_libs']

burn_snd_defs += platform_math_defs

burn_snd_c_args = burn_snd_defs
burn_snd_cpp_args = burn_snd_defs

burn_snd_lib = static_library(
    'burn_snd',
    burn_snd_sources,
    dependencies: [burn_dep, cpu_dep],
    include_directories: [include_directories(burn_snd_inc), global_inc],
    c_args: burn_snd_c_args,
    cpp_args: burn_snd_cpp_args,
)

burn_snd_dep = declare_dependency(
    link_with: burn_snd_lib,
    include_directories: include_directories('.'),
)
