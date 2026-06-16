burn_snd_sources = []

all_burn_sounds = {
    'asteroids': {'desc': 'Atari Asteroids discrete sound', 'sources': ['asteroids.cpp']},
    'astrocde': {'desc': 'Bally Astrocade sound', 'sources': ['astrocde_snd.cpp']},
    'ay8910': {'desc': 'AY-3-8910 PSG', 'sources': ['ay8910.c']},
    'beep': {'desc': 'Beep speaker', 'sources': ['beep.cpp']},
    'burn_md2612': {'desc': 'Mega Drive YM2612 wrapper', 'sources': ['burn_md2612.cpp']},
    'burn_y8950': {'desc': 'Yamaha Y8950 wrapper', 'sources': ['burn_y8950.cpp']},
    'burn_ym2151': {'desc': 'Yamaha YM2151 wrapper', 'sources': ['burn_ym2151.cpp']},
    'burn_ym2203': {'desc': 'Yamaha YM2203 wrapper', 'sources': ['burn_ym2203.cpp']},
    'burn_ym2413': {'desc': 'Yamaha YM2413 wrapper', 'sources': ['burn_ym2413.cpp']},
    'burn_ym2608': {'desc': 'Yamaha YM2608 wrapper', 'sources': ['burn_ym2608.cpp']},
    'burn_ym2610': {'desc': 'Yamaha YM2610 wrapper', 'sources': ['burn_ym2610.cpp']},
    'burn_ym2612': {'desc': 'Yamaha YM2612 wrapper', 'sources': ['burn_ym2612.cpp']},
    'burn_ym3526': {'desc': 'Yamaha YM3526 wrapper', 'sources': ['burn_ym3526.cpp']},
    'burn_ym3812': {'desc': 'Yamaha YM3812 wrapper', 'sources': ['burn_ym3812.cpp']},
    'burn_ymf262': {'desc': 'Yamaha YMF262 wrapper', 'sources': ['burn_ymf262.cpp']},
    'burn_ymf271': {'desc': 'Yamaha YMF271 wrapper', 'sources': ['burn_ymf271.cpp']},
    'burn_ymf278b': {'desc': 'Yamaha YMF278B wrapper', 'sources': ['burn_ymf278b.cpp']},
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
    'fm': {'desc': 'Yamaha FM core', 'sources': ['fm.c']},
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
    'tms36xx': {'desc': 'TI TMS36xx melody generator', 'sources': ['tms36xx.cpp']},
    'tms5110': {'desc': 'TI TMS5110 speech', 'sources': ['tms5110.cpp']},
    'tms5220': {'desc': 'TI TMS5220 speech', 'sources': ['tms5220.cpp']},
    'upd7759': {'desc': 'NEC uPD7759 ADPCM', 'sources': ['upd7759.cpp']},
    'vlm5030': {'desc': 'Sanyo VLM5030 speech', 'sources': ['vlm5030.cpp']},
    'votrax': {'desc': 'Votrax SC-01 speech', 'sources': ['votrax.cpp']},
    'wiping': {'desc': 'Wiping custom sound', 'sources': ['wiping.cpp']},
    'x1010': {'desc': 'Namco CUS30/X1010 sound', 'sources': ['x1010.cpp']},
    'ym2151': {'desc': 'Yamaha YM2151 core', 'sources': ['ym2151.c']},
    'ym2413': {'desc': 'Yamaha YM2413 core', 'sources': ['ym2413.c']},
    'ym2612': {'desc': 'Yamaha YM2612 core', 'sources': ['ym2612.c']},
    'ymdeltat': {'desc': 'Yamaha Delta-T ADPCM core', 'sources': ['ymdeltat.c']},
    'ymf262': {'desc': 'Yamaha YMF262 core', 'sources': ['ymf262.cpp']},
    'ymf271': {'desc': 'Yamaha YMF271 core', 'sources': ['ymf271.cpp']},
    'ymf278b': {'desc': 'Yamaha YMF278B core', 'sources': ['ymf278b.c']},
    'ymz280b': {'desc': 'Yamaha YMZ280B PCM', 'sources': ['ymz280b.cpp']},
    'ymz770': {'desc': 'Yamaha YMZ770 sound', 'sources': ['ymz770.cpp']},
}

burn_sound_status = {}
all_burn_sound_names = all_burn_sounds.keys()
foreach name : burn_sound_list
    assert(
        all_burn_sound_names.contains(name),
        'Unknown burn sound in burn_sound_list: ' + name,
    )
endforeach

foreach name, sound : all_burn_sounds
    enabled = burn_sound_list.contains(name)
    burn_sound_status += {sound['desc']: enabled ? 'ON' : 'OFF'}
    if enabled
        burn_snd_sources += sound['sources']
    endif
endforeach
summary(burn_sound_status, section: 'Burn sound devices', list_sep: '')

burn_snd_inc = ['../../dep/libs/dr_libs']

burn_snd_defs = []
if host_machine.system() == 'windows'
    burn_snd_defs += '-D_USE_MATH_DEFINES'
endif

burn_snd_cpp_args = burn_snd_defs

burn_snd_lib = static_library(
    'burn_snd',
    burn_snd_sources,
    dependencies: [burn_dep, cpu_dep],
    include_directories: include_directories(burn_snd_inc),
    cpp_args: burn_snd_cpp_args,
)

burn_snd_dep = declare_dependency(
    link_with: burn_snd_lib,
    include_directories: include_directories('.'),
)
