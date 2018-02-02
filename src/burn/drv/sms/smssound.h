
#ifndef _SMSSOUND_H_
#define _SMSSOUND_H_

/* Sound emulation structure */
typedef struct
{
    INT32 enabled;
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
