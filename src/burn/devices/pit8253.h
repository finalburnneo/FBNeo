/***************************************************************************

    Intel 8253/8254
    Programmable Interval Timer

    As uPD71054C (8MHz), uPD71054C-10 (10MHz) - it is a clone of Intel 82C54
    also available in 28-pin QFP and 44-pin PLCC (many pins NC)

                            _____   _____
                    D7   1 |*    \_/     | 24  VCC
                    D6   2 |             | 23  _WR
                    D5   3 |             | 22  _RD
                    D4   4 |             | 21  _CS
                    D3   5 |             | 20  A1
                    D2   6 |    8253     | 19  A0
                    D1   7 |    8254     | 18  CLK2
                    D0   8 |             | 17  OUT2
                  CLK0   9 |             | 16  GATE2
                  OUT0  10 |             | 15  CLK1
                 GATE0  11 |             | 14  GATE1
                   GND  12 |_____________| 13  OUT1


***************************************************************************/

void pit8253_init(INT32 clock0, INT32 clock1, INT32 clock2, void (*cb0)(INT32),void (*cb1)(INT32),void (*cb2)(INT32));
void pit8253_exit();
void pit8253_reset();
void pit8253_scan();

void pit8253NewFrame();
INT32 pit8253TotalCycles();
INT32 pit8253Idle(INT32 cycles);
INT32 pit8253Run(INT32 cycles);


UINT8 pit8253_read(INT32 offset);
void pit8253_write(INT32 offset, UINT8 data);
void pit8253_gate_write(INT32 gate, INT32 state);

void pit8253_set_clockin(INT32 timerno, double new_clockin);
void pit8253_set_clock_signal(INT32 timerno, INT32 state);

#if 0
	// static configuration helpers
	static void set_clk0(device_t &device, double clk0) { downcast<pit8253_device &>(device).m_clk0 = clk0; }
	static void set_clk1(device_t &device, double clk1) { downcast<pit8253_device &>(device).m_clk1 = clk1; }
	static void set_clk2(device_t &device, double clk2) { downcast<pit8253_device &>(device).m_clk2 = clk2; }
	template<class _Object> static devcb_base &set_out0_handler(device_t &device, _Object object) { return downcast<pit8253_device &>(device).m_out0_handler.set_callback(object); }
	template<class _Object> static devcb_base &set_out1_handler(device_t &device, _Object object) { return downcast<pit8253_device &>(device).m_out1_handler.set_callback(object); }
	template<class _Object> static devcb_base &set_out2_handler(device_t &device, _Object object) { return downcast<pit8253_device &>(device).m_out2_handler.set_callback(object); }

	DECLARE_READ8_MEMBER(read);
	DECLARE_WRITE8_MEMBER(write);

	WRITE_LINE_MEMBER(write_gate0);
	WRITE_LINE_MEMBER(write_gate1);
	WRITE_LINE_MEMBER(write_gate2);

	/* In the 8253/8254 the CLKx input lines can be attached to a regular clock
	 signal. Another option is to use the output from one timer as the input
	 clock to another timer.

	 The functions below should supply both functionalities. If the signal is
	 a regular clock signal, use the pit8253_set_clockin function. If the
	 CLKx input signal is the output of the different source, set the new_clockin
	 to 0 with pit8253_set_clockin and call pit8253_clkX_w to change
	 the state of the input CLKx signal.
	 */
	WRITE_LINE_MEMBER(write_clk0);
	WRITE_LINE_MEMBER(write_clk1);
	WRITE_LINE_MEMBER(write_clk2);

	void set_clockin(int timer, double new_clockin);
#endif
