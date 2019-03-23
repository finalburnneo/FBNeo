/***************************************************************************

    Z80 PIO (Z8420) implementation

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************
                            _____   _____
                    D2   1 |*    \_/     | 40  D3
                    D7   2 |             | 39  D4
                    D6   3 |             | 38  D5
                   _CE   4 |             | 37  _M1
                  C/_D   5 |             | 36  _IORQ
                  B/_A   6 |             | 35  RD
                   PA7   7 |             | 34  PB7
                   PA6   8 |             | 33  PB6
                   PA5   9 |             | 32  PB5
                   PA4  10 |   Z80-PIO   | 31  PB4
                   GND  11 |             | 30  PB3
                   PA3  12 |             | 29  PB2
                   PA2  13 |             | 28  PB1
                   PA1  14 |             | 27  PB0
                   PA0  15 |             | 26  +5V
                 _ASTB  16 |             | 25  CLK
                 _BSTB  17 |             | 24  IEI
                  ARDY  18 |             | 23  _INT
                    D0  19 |             | 22  IEO
                    D1  20 |_____________| 21  BRDY

***************************************************************************/

#ifndef __Z80PIO_H__
#define __Z80PIO_H__

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

// for the daisy chain:
int   z80pio_irq_state();
int   z80pio_irq_ack();
void  z80pio_irq_reti();
void  z80pio_exit();
void  z80pio_reset();
void  z80pio_scan(INT32 nAction);

// driver stuff:
void  z80pio_init(void (*intr)(int), UINT8 (*portAread)(int), UINT8 (*portBread)(int), void (*portAwrite)(int, UINT8), void (*portBwrite)(int, UINT8), void (*rdyA)(int), void (*rdyB)(int));
void  z80pio_strobeA(int state);
void  z80pio_strobeB(int state);
UINT8 z80pio_read(int offset);
void  z80pio_write(int offset, UINT8 data);
UINT8 z80pio_read_alt(int offset);
void  z80pio_write_alt(int offset, UINT8 data);

void  z80pio_c_w(int offset, UINT8 data);
UINT8 z80pio_c_r(int offset);
void  z80pio_d_w(int offset, UINT8 data);
UINT8 z80pio_d_r(int offset);
void  z80pio_port_write(int offset, UINT8 data);
UINT8 z80pio_port_read(int offset, UINT8 data);

#endif
