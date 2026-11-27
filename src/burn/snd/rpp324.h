// Roland RPP-324 RDAC phrase player (System Board Y2)

void RPP324Init(UINT8 *rom, UINT32 rom_size, UINT64 (*pCycles)(), UINT32 nCpuClock);
void RPP324Exit();
void RPP324Reset();
void RPP324Scan(INT32 nAction, INT32 *pnMin);
void RPP324SetVolume(double vol);

// 16-bit registers: host port (offset 0: command bytes, 1: handshake), board control latch, status ports
UINT16 RPP324HostRead(INT32 offset);
void RPP324HostWrite(INT32 offset, UINT16 data);
UINT16 RPP324ControlRead();
void RPP324ControlWrite(UINT16 data, UINT16 mask);
UINT16 RPP324StatusRead();

// call once per frame, after the cpu: brings the chip up to date and resamples the frame (buffer may be NULL)
void RPP324Update(INT16 *buffer, INT32 length);
