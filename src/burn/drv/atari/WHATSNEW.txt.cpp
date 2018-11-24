   -[ atari devnotes ]-

  - good drivers -
badlands: good
shuuz: good.
skullxbo: good.
blastroid: good
thunderj: good.
xybots: good.
toobin: good
eprom: good. (sometimes scratchy/corrupted tms5220 voice synth noise?)
klax: good
atarig1: good-ish, see notes for atarirle below.

  - WIP drivers -
all of the rest!
NOTE: _all_ games need good sync, see d_thunderj, d_blstroid's frame to see how it needs to be.

	- devices -
atarijsa: changed "if (has_tms5220 && tms5220_ready() == 0) result ^= 0x10;" to "if (has_tms5220 && tms5220_ready()) result ^= 0x10;"
atarimo: add states, gfxdata -> GenericGfxData, FIXED CLIPPY BUG (damnit!!) nov.20.2018
atarivad: add states + palette re-sync
tiles_generic.*,tilemap_generic.*: gfxdata -> GenericGfxData ('cuz Barry says so!)
atarirle: this is a nightmare. proper clipping needs to be added, bitmap fills do not respect clipping.  atarig1 should only be displaying the first vram bitmap, but instead its displaying butmap 0 and 1, which basically mixes the previous frame's sprites with the current sprites.  (reason for hydra roz looking funny as it zooms)...
