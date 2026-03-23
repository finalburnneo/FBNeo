// pgm2_crypt.h - IGS036 (PGM2) ROM decryption
// Based on MAME igs036crypt / pgm2.cpp research

#pragma once

void pgm2_decrypt_orleg2();
void pgm2_decrypt_kov2nl();
void pgm2_decrypt_kov3();

// shared helper used by all three
// offset = word_address addend (MAME romboard_ram.bytes(), e.g. 0x200000 for rom-board games)
void pgm2_igs036_decrypt(UINT8 *rom, UINT32 romlen, const UINT8 *key, INT32 offset = 0);

// sprite data preprocessing used by PGM2 games
void pgm2_decode_sprite_data(INT32 maskOffset, INT32 maskLen, INT32 colourOffset, INT32 colourLen);
