// Sega 315-5292 tile generator (System 24, Model 1, Model 2)

#ifndef SEGA_315_5292_H
#define SEGA_315_5292_H

// tileram: 0x10000 bytes, charram: charram_size bytes (16-bit words, host order), tile_mask: char number bits
// of a tile ram entry (0xfff on System 24, 0x3fff on Model 1/2); the layers are drawn at width x height
void sega_315_5292_init(UINT8 *tileram, UINT8 *charram, INT32 charram_size, UINT16 tile_mask, INT32 width, INT32 height);
void sega_315_5292_exit();
void sega_315_5292_reset();
void sega_315_5292_recalc();
void sega_315_5292_invalidate();
void sega_315_5292_char_written(UINT32 offset);	// byte offset in the char ram, even
void sega_315_5292_update();					// once a frame, before drawing the layers
// layer 0-7: tilemap (layer >> 1), category (layer & 1); opaque: the lower layers are drawn opaque
// prio (same pitch as dst): primask is or-ed in where the layer draws
void sega_315_5292_draw_layer(INT32 layer, INT32 opaque, UINT16 *dst, UINT8 *prio = NULL, UINT8 primask = 0);

#endif
