
enum e132xs_types
{
	TYPE_E116T = 0,
	TYPE_E116XT,
	TYPE_E116XS,
	TYPE_E116XSR,
	TYPE_E132N,
	TYPE_E132T,
	TYPE_E132XN,
	TYPE_E132XT,
	TYPE_E132XS,
	TYPE_E132XSR,
	TYPE_GMS30C2116,
	TYPE_GMS30C2132,
	TYPE_GMS30C2216,
	TYPE_GMS30C2232
};

void E132XSInit(INT32 cpu, INT32 type, INT32 clock);
void E132XSOpen(INT32 cpu);
void E132XSClose();
INT32 E132XSGetActive();
INT32 E132XSRun(INT32);
INT32 E132XSIdle(INT32);
void E132XSReset();
void E132XSExit();
void E132XSNewFrame();
INT32 E132XSScan(INT32 nAction);

void E132XSSetIRQLine(INT32 line, INT32 state);

void E132XSSetWriteByteHandler(void (*handler)(UINT32,UINT8));
void E132XSSetWriteWordHandler(void (*handler)(UINT32,UINT16));
void E132XSSetWriteLongHandler(void (*handler)(UINT32,UINT32));
void E132XSSetReadByteHandler(UINT8 (*handler)(UINT32));
void E132XSSetReadWordHandler(UINT16 (*handler)(UINT32));
void E132XSSetReadLongHandler(UINT32 (*handler)(UINT32));
void E132XSSetIOWriteHandler(void (*handler)(UINT32,UINT32));
void E132XSSetIOReadHandler(UINT32 (*handler)(UINT32));

void E132XSMapMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags);

// for speed hacking!
INT32 E132XSInterruptActive();
void E132XSBurnUntilInt();
UINT32 E132XSGetPC(INT32);
void E132XSRunEnd();
void E132XSRunEndBurnAllCycles();
INT32 E132XSBurnCycles(INT32 cycles);
INT64 E132XSTotalCycles();
INT32 E132XSTotalCycles32();
