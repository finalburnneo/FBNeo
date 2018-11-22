
#define JSA_CLOCK	1789773

void AtariJSAInterrupt(); // 4x per frame
void AtariJSAInterruptUpdate(INT32 interleave); // same as above, but handles timer

void AtariJSAUpdate(INT16 *output, INT32 length);
UINT16 AtariJSARead();
void AtariJSAWrite(UINT8 data);
void AtariJSAResetWrite(UINT8 data);

void AtariJSAExit();
void AtariJSAInit(UINT8 *rom, void (*int_cb)(), UINT8 *samples0, UINT8 *samples1);
void AtariJSAReset();
void AtariJSAScan(INT32 nAction, INT32 *pnMin);

extern INT32 atarigen_cpu_to_sound_ready;
extern INT32 atarigen_sound_to_cpu_ready;

extern UINT8 atarijsa_input_port;
extern UINT8 atarijsa_test_port;
extern UINT8 atarijsa_test_mask;

extern INT32 atarijsa_int_state;
