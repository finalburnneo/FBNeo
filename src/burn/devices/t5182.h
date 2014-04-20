
#define T5182_CLOCK     (14318180/4)

enum
{
	VECTOR_INIT,
	YM2151_ASSERT,
	YM2151_CLEAR,
	YM2151_ACK,
	CPU_ASSERT,
	CPU_CLEAR
};

extern UINT8 *t5182SharedRAM;	// allocate in driver
extern UINT8 *t5182ROM;		// allocate in driver
extern UINT8 t5182_semaphore_snd;
extern UINT8 t5182_semaphore_main;
extern UINT8 t5182_coin_input;	// coin input in driver

void t5182_setirq_callback(INT32 param);

void t5182Reset();
void t5182Init(INT32 nZ80CPU, INT32 clock);
void t5182Exit();
INT32 t5182Scan(INT32 nAction);
