
#ifndef _SMSSOUND_H_
#define _SMSSOUND_H_

enum {
    STREAM_PSG_L,               /* PSG left channel */
    STREAM_PSG_R,               /* PSG right channel */
    STREAM_FM_MO,               /* YM2413 melody channel */
    STREAM_FM_RO,               /* YM2413 rhythm channel */
    STREAM_MAX                  /* Total # of sound streams */
};  

/* Sound emulation structure */
typedef struct
{
    void (*mixer_callback)(INT16 **stream, INT16 **output, INT32 length);
    INT16 *output[2];
    INT16 *stream[STREAM_MAX];
    INT32 fm_which;
    INT32 enabled;
    INT32 fps;
    INT32 buffer_size;
    INT32 sample_count;
    INT32 sample_rate;
    INT32 done_so_far;
    UINT32 fm_clock;
    UINT32 psg_clock;
} snd_t;


/* Global data */
extern snd_t snd;

/* Function prototypes */
void psg_write(INT32 data);
void psg_stereo_w(INT32 data);
INT32 fmunit_detect_r(void);
void fmunit_detect_w(INT32 data);
void fmunit_write(INT32 offset, INT32 data);
INT32 sound_init(void);
void sound_shutdown(void);
void sound_reset(void);

#endif /* _SOUND_H_ */
