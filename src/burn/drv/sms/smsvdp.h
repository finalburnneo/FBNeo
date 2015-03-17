
#ifndef _SMSVDP_H_
#define _SMSVDP_H_

/*
    vdp1

    mode 4 when m4 set and m1 reset

    vdp2

    mode 4 when m4 set and m2,m1 != 1,0


*/

/* Display timing (NTSC) */

#define MASTER_CLOCK        3579545
#define LINES_PER_FRAME     262
#define FRAMES_PER_SECOND   60
#define CYCLES_PER_LINE     ((MASTER_CLOCK / FRAMES_PER_SECOND) / LINES_PER_FRAME)

/* VDP context */
typedef struct
{
    UINT8 vram[0x4000];
    UINT8 cram[0x40]; 
    UINT8 reg[0x10];
    UINT8 status;     
    UINT8 latch;      
    UINT8 pending;    
    UINT8 buffer;     
    UINT8 code;       
    UINT16 addr;
    INT32 pn, ct, pg, sa, sg;
    INT32 ntab;        
    INT32 satb;
    INT32 line;
    INT32 left;
    UINT8 height;
    UINT8 extended;
    UINT8 mode;
    UINT8 vint_pending;
    UINT8 hint_pending;
    UINT16 cram_latch;
    UINT8 bd;
} vdp_t;

/* Global data */
extern vdp_t vdp;
extern UINT32 smsvdp_tmsmode;

/* Function prototypes */
void vdp_init(void);
void vdp_shutdown(void);
void vdp_reset(void);
UINT8 vdp_counter_r(INT32 offset);
UINT8 vdp_read(INT32 offset);
void vdp_write(INT32 offset, UINT8 data);
void gg_vdp_write(INT32 offset, UINT8 data);
void md_vdp_write(INT32 offset, UINT8 data);
void tms_write(INT32 offset, INT32 data);
void viewport_check(void);

#endif /* _VDP_H_ */

