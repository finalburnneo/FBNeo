// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    Intel 8255(A) Programmable Peripheral Interface emulation

**********************************************************************
                            _____   _____
                   PA3   1 |*    \_/     | 40  PA4
                   PA2   2 |             | 39  PA5
                   PA1   3 |             | 38  PA6
                   PA0   4 |             | 37  PA7
                   _RD   5 |             | 36  WR
                   _CS   6 |             | 35  RESET
                   GND   7 |             | 34  D0
                    A1   8 |             | 33  D1
                    A0   9 |             | 32  D2
                   PC7  10 |    8255     | 31  D3
                   PC6  11 |    8255A    | 30  D4
                   PC5  12 |             | 29  D5
                   PC4  13 |             | 28  D6
                   PC0  14 |             | 27  D7
                   PC1  15 |             | 26  Vcc
                   PC2  16 |             | 25  PB7
                   PC3  17 |             | 24  PB6
                   PB0  18 |             | 23  PB5
                   PB1  19 |             | 22  PB4
                   PB2  20 |_____________| 21  PB3

**********************************************************************/
void i8255_init();
void i8255_exit();
void i8255_scan();
void i8255_reset();

void i8255_set_ports_read(UINT8 (*in_pa)(), UINT8 (*in_pb)(), UINT8 (*in_pc)());
void i8255_set_ports_write(void (*out_pa)(UINT8), void (*out_pb)(UINT8), void (*out_pc)(UINT8));
void i8255_set_ports_tri_read(UINT8 (*tri_pa)(), UINT8 (*tri_pb)(), UINT8 (*tri_pc)());

UINT8 i8255_read(INT32 offset);
void i8255_write(INT32 offset, UINT8 data);

UINT8 i8255_pa_read();
UINT8 i8255_acka_read();

UINT8 i8255_pb_read();
UINT8 i8255_ackb_read();

void i8255_pc2_write(int state);
void i8255_pc4_write(int state);
void i8255_pc6_write(int state);

