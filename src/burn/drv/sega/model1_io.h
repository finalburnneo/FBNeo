// Sega Model 1 I/O boards (also on the original Model 2)

#ifndef MODEL1_IO_H
#define MODEL1_IO_H

#define M1IO_CLOCK		4000000
#define M1IO2_CLOCK		9830400

// the board's z80 is ZetInit(0); it shares dpram (0x800 bytes) with the main board
// inputs: 3 ports, dips: 3 (the control switch selects them on the 837-8950), analog: 8 channels (4-7 secondary)
void M1IOInit(INT32 board2, UINT8 *rom, UINT8 *ram, UINT8 *dpram, UINT8 *inputs, UINT8 *dips, UINT8 *analog);
// port_in: return -1 for the default value; port_out: called for every port write
void M1IOSetHooks(INT32 (*port_in)(INT32), void (*port_out)(INT32, UINT8));
// model1io2 lightgun fpga (virtua cop), offset 0-15 once the game has uploaded it
void M1IOSetFpgaRead(UINT8 (*read)(INT32 offset));
void M1IOReset();
void M1IOScan(INT32 nAction);
UINT8 M1IOGetOutputs();
INT32 M1IOGetSecondary();

#endif
