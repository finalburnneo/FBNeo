// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    Intel 8155/8156 - 2048-Bit Static MOS RAM with I/O Ports and Timer

**********************************************************************
                            _____   _____
                   PC3   1 |*    \_/     | 40  Vcc
                   PC4   2 |             | 39  PC2
              TIMER IN   3 |             | 38  PC1
                 RESET   4 |             | 37  PC0
                   PC5   5 |             | 36  PB7
            _TIMER OUT   6 |             | 35  PB6
                 IO/_M   7 |             | 34  PB5
             CE or _CE   8 |             | 33  PB4
                   _RD   9 |             | 32  PB3
                   _WR  10 |    8155     | 31  PB2
                   ALE  11 |    8156     | 30  PB1
                   AD0  12 |             | 29  PB0
                   AD1  13 |             | 28  PA7
                   AD2  14 |             | 27  PA6
                   AD3  15 |             | 26  PA5
                   AD4  16 |             | 25  PA4
                   AD5  17 |             | 24  PA3
                   AD6  18 |             | 23  PA2
                   AD7  19 |             | 22  PA1
                   Vss  20 |_____________| 21  PA0

**********************************************************************/

void i8155_init();
void i8155_exit();
void i8155_reset();
void i8155_scan();

// use cpu hook or run cycles in frame
INT32 i8155_timers_run(INT32 cycles);

void i8155_set_pa_read_cb(UINT8 (*cb)(INT32));
void i8155_set_pb_read_cb(UINT8 (*cb)(INT32));
void i8155_set_pc_read_cb(UINT8 (*cb)(INT32));
void i8155_set_pa_write_cb(void (*cb)(INT32, UINT8));
void i8155_set_pb_write_cb(void (*cb)(INT32, UINT8));
void i8155_set_pc_write_cb(void (*cb)(INT32, UINT8));
void i8155_set_to_write_cb(void (*cb)(INT32));

void i8155_data_write(UINT8 data);
UINT8 i8155_data_read();

void i8155_memory_write(INT32 offset, UINT8 data);
UINT8 i8155_memory_read(INT32 offset);

void i8155_io_write(INT32 offset, UINT8 data);
UINT8 i8155_io_read(INT32 offset);

void i8155_ale_write(INT32 offset, UINT8 data);
