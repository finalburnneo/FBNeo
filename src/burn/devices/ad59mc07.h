
void AD59MC07Init(UINT8 *rom); // pass rom data
void AD59MC07Reset();
void AD59MC07Command(UINT8 data);
void AD59MC07Exit();
INT32 AD59MC07Scan(INT32 nAction, INT32 *pnMin);

void AD59MC07NewFrame();
INT32 AD59MC07Run(INT32 cycles); // run like a cpu
void AD59MC07Update(INT16 *output, INT32 length);

static struct BurnSampleInfo mc07SampleDesc[] = {
	{ "bongo1", 			SAMPLE_NOLOOP },
	{ "bongo2", 			SAMPLE_NOLOOP },
	{ "bongo3", 			SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(mc07)
STD_SAMPLE_FN(mc07)
