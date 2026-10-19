// Intel 80186 support for the NEC core: opcode differences and on-chip peripherals
// (interrupt controller in master mode, timers, dma, chip selects, relocation register)
// Peripheral emulation based on the MAME i186 core by Carl, Phill Harvey-Smith, Aaron Giles and Paul Leaman
// Not supported: iRMX (slave) interrupt mode, interrupt cascade mode

/* external int priority masks */
#define EXTINT_CTRL_PRI_MASK    0x07
#define EXTINT_CTRL_MSK         0x08
#define EXTINT_CTRL_LTM         0x10
#define EXTINT_CTRL_CASCADE     0x20
#define EXTINT_CTRL_SFNM        0x40

/* DMA control register */
#define DEST_MIO                0x8000
#define DEST_DECREMENT          0x4000
#define DEST_INCREMENT          0x2000
#define DEST_NO_CHANGE          (DEST_DECREMENT | DEST_INCREMENT)
#define DEST_INCDEC_MASK        (DEST_DECREMENT | DEST_INCREMENT)
#define SRC_MIO                 0x1000
#define SRC_DECREMENT           0x0800
#define SRC_INCREMENT           0x0400
#define SRC_NO_CHANGE           (SRC_DECREMENT | SRC_INCREMENT)
#define SRC_INCDEC_MASK         (SRC_DECREMENT | SRC_INCREMENT)
#define TERMINATE_ON_ZERO       0x0200
#define INTERRUPT_ON_ZERO       0x0100
#define SYNC_MASK               0x00C0
#define SYNC_SOURCE             0x0040
#define SYNC_DEST               0x0080
#define CHANNEL_PRIORITY        0x0020
#define TIMER_DRQ               0x0010
#define CHG_NOCHG               0x0004
#define ST_STOP                 0x0002
#define BYTE_WORD               0x0001

static void (*i186_instruction[256])(nec_state_t *nec_state);

static inline UINT16 i186_swap16(UINT16 d)
{
	return (d << 8) | (d >> 8);
}

/*************************************
 *
 *  Opcodes that differ from the V30
 *
 *************************************/

static void i186_invalid(nec_state_t *nec_state)
{
	nec_state->icount -= 10;
	nec_interrupt(nec_state, 6, BRK); // 80186 has #UD
}

OP( 0x62, i186_bound ) {
	GetModRM;
	INT16 low = (INT16)GetRMWord(ModRM);
	INT16 high = (INT16)GetnextRMWord;
	INT16 idx = (INT16)RegWord(ModRM);
	if (idx < low || idx > high) {
		nec_interrupt(nec_state, 5, BRK);
	}
	nec_state->icount -= 33;
}

OP( 0x8e, i186_mov_sregw ) { UINT16 src; GetModRM; src = GetRMWord(ModRM); CLKR(15,15,7,15,11,5,2,EA);
	switch (ModRM & 0x38) {
		case 0x00: Sreg(DS1) = src; break; /* mov es,ew */
		case 0x10: Sreg(SS_) = src; break; /* mov ss,ew */
		case 0x18: Sreg(DS0) = src; break; /* mov ds,ew */
		default: nec_interrupt(nec_state, 6, BRK); return;
	}
	nec_state->no_interrupt=1;
}

// shift/rotate counts are masked to 5 bits, /6 is an alias of SHL
#define I186_ROTSHFT_BYTE(c) \
	if (c) switch (ModRM & 0x38) { \
		case 0x00: do { ROL_BYTE;  c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break; \
		case 0x08: do { ROR_BYTE;  c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break; \
		case 0x10: do { ROLC_BYTE; c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break; \
		case 0x18: do { RORC_BYTE; c--; CLK(1); } while (c>0); PutbackRMByte(ModRM,(BYTE)dst); break; \
		case 0x20: case 0x30: SHL_BYTE(c); break; \
		case 0x28: SHR_BYTE(c); break; \
		case 0x38: SHRA_BYTE(c); break; \
	}

#define I186_ROTSHFT_WORD(c) \
	if (c) switch (ModRM & 0x38) { \
		case 0x00: do { ROL_WORD;  c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break; \
		case 0x08: do { ROR_WORD;  c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break; \
		case 0x10: do { ROLC_WORD; c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break; \
		case 0x18: do { RORC_WORD; c--; CLK(1); } while (c>0); PutbackRMWord(ModRM,(WORD)dst); break; \
		case 0x20: case 0x30: SHL_WORD(c); break; \
		case 0x28: SHR_WORD(c); break; \
		case 0x38: SHRA_WORD(c); break; \
	}

OP( 0xc0, i186_rotshft_bd8 ) {
	UINT32 src, dst; UINT8 c;
	GetModRM; src = (unsigned)GetRMByte(ModRM); dst=src;
	c=FETCH() & 0x1f;
	CLKM(7,7,2,19,19,6);
	I186_ROTSHFT_BYTE(c)
}

OP( 0xc1, i186_rotshft_wd8 ) {
	UINT32 src, dst; UINT8 c;
	GetModRM; src = (unsigned)GetRMWord(ModRM); dst=src;
	c=FETCH() & 0x1f;
	CLKM(7,7,2,27,19,6);
	I186_ROTSHFT_WORD(c)
}

OP( 0xd0, i186_rotshft_b ) {
	UINT32 src, dst; GetModRM; src = (UINT32)GetRMByte(ModRM); dst=src;
	CLKM(6,6,2,16,16,7);
	switch (ModRM & 0x38) {
		case 0x00: ROL_BYTE;  PutbackRMByte(ModRM,(BYTE)dst); nec_state->OverVal = (src^dst)&0x80; break;
		case 0x08: ROR_BYTE;  PutbackRMByte(ModRM,(BYTE)dst); nec_state->OverVal = (src^dst)&0x80; break;
		case 0x10: ROLC_BYTE; PutbackRMByte(ModRM,(BYTE)dst); nec_state->OverVal = (src^dst)&0x80; break;
		case 0x18: RORC_BYTE; PutbackRMByte(ModRM,(BYTE)dst); nec_state->OverVal = (src^dst)&0x80; break;
		case 0x20: case 0x30: SHL_BYTE(1); nec_state->OverVal = (src^dst)&0x80; break;
		case 0x28: SHR_BYTE(1); nec_state->OverVal = (src^dst)&0x80; break;
		case 0x38: SHRA_BYTE(1); nec_state->OverVal = 0; break;
	}
}

OP( 0xd1, i186_rotshft_w ) {
	UINT32 src, dst; GetModRM; src = (UINT32)GetRMWord(ModRM); dst=src;
	CLKM(6,6,2,24,16,7);
	switch (ModRM & 0x38) {
		case 0x00: ROL_WORD;  PutbackRMWord(ModRM,(WORD)dst); nec_state->OverVal = (src^dst)&0x8000; break;
		case 0x08: ROR_WORD;  PutbackRMWord(ModRM,(WORD)dst); nec_state->OverVal = (src^dst)&0x8000; break;
		case 0x10: ROLC_WORD; PutbackRMWord(ModRM,(WORD)dst); nec_state->OverVal = (src^dst)&0x8000; break;
		case 0x18: RORC_WORD; PutbackRMWord(ModRM,(WORD)dst); nec_state->OverVal = (src^dst)&0x8000; break;
		case 0x20: case 0x30: SHL_WORD(1); nec_state->OverVal = (src^dst)&0x8000; break;
		case 0x28: SHR_WORD(1); nec_state->OverVal = (src^dst)&0x8000; break;
		case 0x38: SHRA_WORD(1); nec_state->OverVal = 0; break;
	}
}

OP( 0xd2, i186_rotshft_bcl ) {
	UINT32 src, dst; UINT8 c; GetModRM; src = (UINT32)GetRMByte(ModRM); dst=src;
	c=Breg(CL) & 0x1f;
	CLKM(7,7,2,19,19,6);
	I186_ROTSHFT_BYTE(c)
}

OP( 0xd3, i186_rotshft_wcl ) {
	UINT32 src, dst; UINT8 c; GetModRM; src = (UINT32)GetRMWord(ModRM); dst=src;
	c=Breg(CL) & 0x1f;
	CLKM(7,7,2,27,19,6);
	I186_ROTSHFT_WORD(c)
}

OP( 0xd8, i186_esc ) {
	if (nec_state->i186.reloc & 0x8000) {
		// ESC trap enabled
		nec_state->ip--;
		nec_interrupt(nec_state, 7, BRK);
		return;
	}
	GetModRM; ModRM = ModRM; nec_state->icount-=2;
}

// OUT with AL writes both AH and AL onto the internal bus
static void i186_out_al(nec_state_t *nec_state, UINT32 port)
{
	port &= 0xffff;

	if (i186_port_match(nec_state, port)) {
		i186_internal_w(nec_state, (port >> 1) & 0x7f, (port & 1) ? i186_swap16(Wreg(AW)) : Wreg(AW));
		return;
	}

	cpu_writeport(port, Breg(AL));
}

// halt doesn't end the timeslice, the timers keep running while halted
OP( 0xf4, i186_hlt ) { nec_state->halted = 1; CLK(2); }

OP( 0xe6, i186_outal   ) { UINT8 port = FETCH(); i186_out_al(nec_state, port); CLKS(8,8,3); }
OP( 0xee, i186_outdxal ) { i186_out_al(nec_state, Wreg(DW)); CLKS(8,8,3); }

static void nec_init_i186_table()
{
	for (INT32 i = 0; i < 0x100; i++)
		i186_instruction[i] = nec_instruction[i];

	// no V-series extensions
	i186_instruction[0x0f] = i186_invalid;
	i186_instruction[0x62] = i186_bound;
	for (INT32 i = 0x63; i < 0x68; i++)
		i186_instruction[i] = i186_invalid;

	i186_instruction[0x8e] = i186_mov_sregw;
	i186_instruction[0xc0] = i186_rotshft_bd8;
	i186_instruction[0xc1] = i186_rotshft_wd8;
	i186_instruction[0xd0] = i186_rotshft_b;
	i186_instruction[0xd1] = i186_rotshft_w;
	i186_instruction[0xd2] = i186_rotshft_bcl;
	i186_instruction[0xd3] = i186_rotshft_wcl;
	i186_instruction[0xd4] = i86_aam;
	i186_instruction[0xd5] = i86_aad;
	for (INT32 i = 0xd8; i < 0xe0; i++)
		i186_instruction[i] = i186_esc;
	i186_instruction[0xe6] = i186_outal;
	i186_instruction[0xee] = i186_outdxal;
	i186_instruction[0xf1] = nec_instruction[0xf0];
	i186_instruction[0xf4] = i186_hlt;
}

/*************************************
 *
 *  Interrupt controller
 *
 *************************************/

static void i186_set_intr_line(nec_state_t *nec_state, INT32 state)
{
	nec_state->irq_state = state;

	if (state) {
		nec_state->pending_irq |= INT_IRQ;
		nec_state->halted = 0;
	} else {
		nec_state->pending_irq &= ~INT_IRQ;
	}
}

static UINT32 i186_inta(nec_state_t *nec_state)
{
	i186_intr_state &intr = nec_state->i186.intr;

	/* clear the interrupt */
	i186_set_intr_line(nec_state, 0);
	intr.pending = 0;

	/* clear the request and set the in-service bit */
	if (intr.ack_mask & 0xf0)
	{
		INT32 i;
		for (i = 0; i < 4; i++)
			if ((intr.ack_mask >> (i + 4)) & 1)
				break;
		if (!(intr.ext[i] & EXTINT_CTRL_LTM))
			intr.request &= ~intr.ack_mask;
	}
	else
		intr.request &= ~intr.ack_mask;

	intr.in_service |= intr.ack_mask;

	if (intr.ack_mask == 0x0001)
	{
		switch (intr.poll_status & 0x1f)
		{
			case 0x08:  intr.status &= ~0x01; break;
			case 0x12:  intr.status &= ~0x02; break;
			case 0x13:  intr.status &= ~0x04; break;
		}
	}

	UINT32 vector = intr.poll_status & 0x1f;

	intr.ack_mask = 0;

	/* a request no longer pending */
	intr.poll_status &= ~0x8000;

	return vector;
}

static void i186_update_interrupt_state(nec_state_t *nec_state)
{
	i186_intr_state &intr = nec_state->i186.intr;
	INT32 new_vector = 0;

	/* loop over priorities */
	for (INT32 priority = 0; priority <= intr.priority_mask; priority++)
	{
		/* note: by checking 4 bits, we also verify that the mask is off */
		if ((intr.timer & 0x0f) == priority)
		{
			/* if we're already servicing something at this level, don't generate anything new */
			if (intr.in_service & 0x01)
				return;

			/* if there's something pending, generate an interrupt */
			if (intr.status & 0x07)
			{
				if (intr.status & 1)
					new_vector = 0x08;
				else if (intr.status & 2)
					new_vector = 0x12;
				else
					new_vector = 0x13;

				intr.ack_mask = 0x01;
				goto generate_int;
			}
		}

		/* check DMA interrupts */
		for (INT32 int_num = 0; int_num < 2; int_num++)
		{
			if ((intr.dma[int_num] & 0x0f) == priority)
			{
				INT32 irq = 0x04 << int_num;

				if (intr.in_service & irq)
					return;

				if (intr.request & irq)
				{
					new_vector = 0x0a + int_num;
					intr.ack_mask = irq;
					goto generate_int;
				}
			}
		}

		/* check external interrupts */
		for (INT32 int_num = 0; int_num < 4; int_num++)
		{
			if ((intr.ext[int_num] & 0x0f) == priority)
			{
				if ((intr.in_service & (0x10 << int_num)) && !(intr.ext[int_num] & EXTINT_CTRL_SFNM))
					return;

				if (intr.request & (0x10 << int_num))
				{
					if ((int_num >= 2) && (intr.ext[int_num - 2] & EXTINT_CTRL_CASCADE))
					{
						intr.request &= ~(0x10 << int_num);
						continue;
					}

					new_vector = 0x0c + int_num;
					intr.ack_mask = 0x0010 << int_num;
					goto generate_int;
				}
				else if ((intr.in_service & (0x10 << int_num)) && (intr.ext[int_num] & EXTINT_CTRL_SFNM))
					return; // if an irq is in service and sfnm is enabled, stop here
			}
		}
	}

	intr.pending = 0;
	i186_set_intr_line(nec_state, 0);
	return;

generate_int:
	intr.poll_status = 0x8000 | new_vector;
	if (!intr.pending)
		i186_set_intr_line(nec_state, 1);
	intr.pending = 1;
}

static void i186_handle_eoi(nec_state_t *nec_state, INT32 data)
{
	i186_intr_state &intr = nec_state->i186.intr;
	INT32 handled = 0;

	/* specific case */
	if (!(data & 0x8000))
	{
		switch (data & 0x1f)
		{
			case 0x08:  intr.in_service &= ~0x01; break;
			case 0x12:  intr.in_service &= ~0x01; break;
			case 0x13:  intr.in_service &= ~0x01; break;
			case 0x0a:  intr.in_service &= ~0x04; break;
			case 0x0b:  intr.in_service &= ~0x08; break;
			case 0x0c:  intr.in_service &= ~0x10; break;
			case 0x0d:  intr.in_service &= ~0x20; break;
			case 0x0e:  intr.in_service &= ~0x40; break;
			case 0x0f:  intr.in_service &= ~0x80; break;
		}
	}
	/* non-specific case */
	else
	{
		for (INT32 priority = 0; priority <= 7 && !handled; priority++)
		{
			if ((intr.timer & 0x07) == priority && (intr.in_service & 0x01))
			{
				intr.in_service &= ~0x01;
				handled = 1;
			}

			for (INT32 int_num = 0; int_num < 2 && !handled; int_num++)
				if ((intr.dma[int_num] & 0x07) == priority && (intr.in_service & (0x04 << int_num)))
				{
					intr.in_service &= ~(0x04 << int_num);
					handled = 1;
				}

			for (INT32 int_num = 0; int_num < 4 && !handled; int_num++)
				if ((intr.ext[int_num] & 0x07) == priority && (intr.in_service & (0x10 << int_num)))
				{
					intr.in_service &= ~(0x10 << int_num);
					handled = 1;
				}
		}
	}

	i186_update_interrupt_state(nec_state);
}

static void i186_external_int(nec_state_t *nec_state, INT32 int_num, INT32 state)
{
	i186_intr_state &intr = nec_state->i186.intr;

	if (!(intr.ext_state & (1 << int_num)) == !state)
		return;

	if (!state)
	{
		intr.request &= ~(0x10 << int_num);
		intr.ack_mask &= ~(0x10 << int_num);
		intr.ext_state &= ~(1 << int_num);
	}
	else
	{
		intr.request |= 0x10 << int_num;
		intr.ext_state |= 1 << int_num;
	}

	i186_update_interrupt_state(nec_state);
}

/*************************************
 *
 *  Timers (clocked at 1/4 of the cpu clock)
 *
 *************************************/

static void i186_drq(nec_state_t *nec_state, INT32 which);
static void i186_inc_timer(nec_state_t *nec_state, INT32 which);

static void i186_timer_out(nec_state_t *nec_state, INT32 which, INT32 state)
{
	if (nec_state->i186_timer_out_cb)
		nec_state->i186_timer_out_cb(which, state);
}

static void i186_restart_timer(nec_state_t *nec_state, INT32 which)
{
	i186_timer_state *t = &nec_state->i186.timer[which];

	/* only run timer 0,1 when not incremented via timer 2 pre-scaler */
	if (which != 2 && (t->control & 0x800c) == 0x8008)
		return;

	INT32 count = (t->control & 0x1000) ? t->maxB : t->maxA;
	if (!(t->control & 4)) {
		t->remain = 4 * (count ? count : 0x10000);
		t->active = 1;
	}
}

static void i186_timer_elapsed(nec_state_t *nec_state, INT32 which)
{
	i186_timer_state *t = &nec_state->i186.timer[which];

	/* set the max count bit */
	t->control |= 0x0020;

	/* request an interrupt */
	if (t->control & 0x2000)
	{
		nec_state->i186.intr.status |= 0x01 << which;
		i186_update_interrupt_state(nec_state);
	}

	if (which == 2)
	{
		if (!nec_state->i186.dma_latency)
		{
			if ((nec_state->i186.dma[0].control & (TIMER_DRQ | ST_STOP)) == (TIMER_DRQ | ST_STOP))
				i186_drq(nec_state, 0);
			if ((nec_state->i186.dma[1].control & (TIMER_DRQ | ST_STOP)) == (TIMER_DRQ | ST_STOP))
				i186_drq(nec_state, 1);
		}
		if ((nec_state->i186.timer[0].control & 0x800c) == 0x8008)
			i186_inc_timer(nec_state, 0);
		if ((nec_state->i186.timer[1].control & 0x800c) == 0x8008)
			i186_inc_timer(nec_state, 1);
	}
	else
	{
		if (!(t->control & 2))
			i186_timer_out(nec_state, which, 1);
		else
			i186_timer_out(nec_state, which, (t->control & 0x1000) ? 1 : 0);
	}

	/* if we're continuous or altcounting, reset */
	if ((t->control & 1) || ((t->control & 2) && !(t->control & 0x1000)))
	{
		if ((t->control & 2) && !(t->control & 0x1000))
			t->control |= 0x1000;
		else
			t->control &= ~0x1000;

		t->active = 0;
		i186_restart_timer(nec_state, which);
	}
	else
	{
		t->active = 0;
		t->control &= ~0x9000;
	}
	t->count = 0;
}

static void i186_internal_timer_sync(nec_state_t *nec_state, INT32 which)
{
	i186_timer_state *t = &nec_state->i186.timer[which];

	/* if we have a timing timer running, adjust the count */
	if ((t->control & 0x8000) && !(t->control & 0x0c) && t->active)
		t->count = ((t->control & 0x1000) ? t->maxB : t->maxA) - t->remain / 4;
}

static void i186_inc_timer(nec_state_t *nec_state, INT32 which)
{
	i186_timer_state *t = &nec_state->i186.timer[which];

	t->count++;
	if (t->control & 2)
	{
		if (t->count == ((t->control & 0x1000) ? t->maxB : t->maxA))
			i186_timer_elapsed(nec_state, which);
	}
	else if (t->count == t->maxA)
		i186_timer_elapsed(nec_state, which);
}

static void i186_internal_timer_update(nec_state_t *nec_state, INT32 which, INT32 new_count, INT32 new_maxA, INT32 new_maxB, INT32 new_control)
{
	i186_timer_state *t = &nec_state->i186.timer[which];
	INT32 update_int_timer = 0;

	/* if we have a new count and we're on, update things */
	if (new_count != -1)
	{
		if (t->control & 0x8000)
		{
			i186_internal_timer_sync(nec_state, which);
			update_int_timer = 1;
		}
		t->count = new_count;
	}

	/* if we have a new max and we're on, update things */
	if (new_maxA != -1 && new_maxA != t->maxA)
	{
		if (t->control & 0x8000)
		{
			i186_internal_timer_sync(nec_state, which);
			update_int_timer = 1;
		}
		t->maxA = new_maxA;
	}

	/* if we have a new max and we're on, update things */
	if (new_maxB != -1 && new_maxB != t->maxB)
	{
		if (t->control & 0x8000)
		{
			i186_internal_timer_sync(nec_state, which);
			update_int_timer = 1;
		}
		t->maxB = new_maxB;
	}

	/* handle control changes */
	if (new_control != -1)
	{
		UINT16 resbits = (which == 2) ? 0x1fde : 0x1fc0;

		/* merge back in the bits we don't modify */
		new_control = (new_control & ~resbits) | (t->control & resbits);

		/* handle the /INH bit */
		if (!(new_control & 0x4000))
			new_control = (new_control & ~0x8000) | (t->control & 0x8000);
		new_control &= ~0x4000;

		/* if we have real changes, update things */
		UINT16 diff = new_control ^ t->control;
		if (diff != 0)
		{
			/* if we're going off, make sure our timers are gone */
			if ((diff & 0x8000) && !(new_control & 0x8000))
			{
				i186_internal_timer_sync(nec_state, which);
				update_int_timer = 1;
			}
			/* if we're going on, start the timers running except with external clock or prescale */
			else if ((diff & 0x8000) && (new_control & 0x8000) && !(new_control & 0xc))
			{
				update_int_timer = 1;
			}

			/* if something about the interrupt timer changed, force an update */
			if (!(diff & 0x8000) && (diff & 0x2000))
			{
				i186_internal_timer_sync(nec_state, which);
				update_int_timer = 1;
			}
		}

		/* RIU is cleared whenever ALT = 0 */
		if (!(new_control & 0x0002))
			new_control &= ~0x1000;

		t->control = new_control;
	}

	/* update the interrupt timer */
	if (update_int_timer)
	{
		if ((t->control & 0x8004) == 0x8000 && (!(t->control & 0x10) || t->active))
		{
			INT32 diff = ((t->control & 0x1000) ? t->maxB : t->maxA) - t->count;
			if (diff <= 0)
				diff += 0x10000;
			t->remain = 4 * diff;
			t->active = 1;
		}
		else
		{
			t->active = 0;
		}
	}
}

static void i186_external_tmrin(nec_state_t *nec_state, INT32 which, INT32 state)
{
	i186_timer_state *t = &nec_state->i186.timer[which];

	if (state)
	{
		if ((t->control & 0x8004) == 0x8004)
			i186_inc_timer(nec_state, which);
		else if ((t->control & 0x8014) == 0x8010)
		{
			t->count = 0;
			i186_restart_timer(nec_state, which);
		}
	}
}

static void i186_advance_timers(nec_state_t *nec_state, INT32 cycles)
{
	for (INT32 i = 0; i < 3; i++)
	{
		i186_timer_state *t = &nec_state->i186.timer[i];

		if (!t->active) continue;

		t->remain -= cycles;

		while (t->active && t->remain <= 0)
		{
			INT32 late = t->remain;
			i186_timer_elapsed(nec_state, i);
			if (t->active) t->remain += late;
		}
	}
}

static INT32 i186_next_timer_event(nec_state_t *nec_state)
{
	INT32 next = 0x7fffffff;

	for (INT32 i = 0; i < 3; i++)
	{
		i186_timer_state *t = &nec_state->i186.timer[i];

		if (t->active && t->remain < next)
			next = t->remain;
	}

	return next;
}

/*************************************
 *
 *  DMA
 *
 *************************************/

static void i186_update_dma_control(nec_state_t *nec_state, INT32 which, INT32 new_control)
{
	i186_dma_state *d = &nec_state->i186.dma[which];

	/* handle the CHG bit */
	if (!(new_control & CHG_NOCHG))
		new_control = (new_control & ~ST_STOP) | (d->control & ST_STOP);
	new_control &= ~CHG_NOCHG;

	d->control = new_control;
}

static void i186_drq(nec_state_t *nec_state, INT32 which)
{
	i186_dma_state *dma = &nec_state->i186.dma[which];
	UINT32 incdec_size;

	if (!(dma->control & ST_STOP))
		return;

	if (dma->control & BYTE_WORD)
	{
		UINT16 data = (dma->control & SRC_MIO) ? cpu_readmem20_word(dma->source & 0xfffff) : cpu_readport_word(dma->source & 0xffff);

		if (dma->control & DEST_MIO)
			cpu_writemem20_word(dma->dest & 0xfffff, data);
		else
			cpu_writeport_word(dma->dest & 0xffff, data);

		incdec_size = 2;
	}
	else
	{
		UINT8 data = (dma->control & SRC_MIO) ? cpu_readmem20(dma->source & 0xfffff) : cpu_readport(dma->source & 0xffff);

		if (dma->control & DEST_MIO)
			cpu_writemem20(dma->dest & 0xfffff, data);
		else
			cpu_writeport(dma->dest & 0xffff, data);

		incdec_size = 1;
	}

	switch (dma->control & DEST_INCDEC_MASK)
	{
		case DEST_DECREMENT: dma->dest -= incdec_size; break;
		case DEST_INCREMENT: dma->dest += incdec_size; break;
	}

	switch (dma->control & SRC_INCDEC_MASK)
	{
		case SRC_DECREMENT: dma->source -= incdec_size; break;
		case SRC_INCREMENT: dma->source += incdec_size; break;
	}

	dma->count -= 1;

	/* terminate if count is zero, and terminate flag set */
	if (((dma->control & TERMINATE_ON_ZERO) || !(dma->control & SYNC_MASK)) && dma->count == 0)
	{
		dma->control &= ~ST_STOP;
	}

	/* interrupt if count is zero, and interrupt flag set */
	if ((dma->control & INTERRUPT_ON_ZERO) && dma->count == 0)
	{
		nec_state->i186.intr.request |= 0x04 << which;
		i186_update_interrupt_state(nec_state);
	}
}

/*************************************
 *
 *  Peripheral control block
 *
 *************************************/

static UINT16 i186_internal_r(nec_state_t *nec_state, UINT32 offset, INT32 low_byte)
{
	i186_state &s = nec_state->i186;
	INT32 temp, which;

	switch (offset)
	{
		case 0x10:
			return s.intr.vector;

		case 0x12:
			if (s.intr.poll_status & 0x8000)
				i186_inta(nec_state);
			return s.intr.poll_status;

		case 0x13:
			return s.intr.poll_status;

		case 0x14:
			temp  = (s.intr.timer  >> 3) & 0x01;
			temp |= (s.intr.dma[0] >> 1) & 0x04;
			temp |= (s.intr.dma[1] >> 0) & 0x08;
			temp |= (s.intr.ext[0] << 1) & 0x10;
			temp |= (s.intr.ext[1] << 2) & 0x20;
			temp |= (s.intr.ext[2] << 3) & 0x40;
			temp |= (s.intr.ext[3] << 4) & 0x80;
			return temp;

		case 0x15:
			return s.intr.priority_mask;

		case 0x16:
			return s.intr.in_service;

		case 0x17:
			temp = s.intr.request & ~0x0001;
			if (s.intr.status & 0x0007)
				temp |= 1;
			return temp;

		case 0x18:
			return s.intr.status;

		case 0x19:
			return s.intr.timer;

		case 0x1a:
			return s.intr.dma[0];

		case 0x1b:
			return s.intr.dma[1];

		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
			return s.intr.ext[offset - 0x1c];

		case 0x28:
		case 0x2c:
		case 0x30:
			which = (offset - 0x28) / 4;
			if (low_byte)
				i186_internal_timer_sync(nec_state, which);
			return s.timer[which].count;

		case 0x29:
		case 0x2d:
		case 0x31:
			which = (offset - 0x29) / 4;
			return s.timer[which].maxA;

		case 0x2a:
		case 0x2e:
			which = (offset - 0x2a) / 4;
			return s.timer[which].maxB;

		case 0x2b:
		case 0x2f:
		case 0x33:
			which = (offset - 0x2b) / 4;
			return s.timer[which].control;

		case 0x50:
			return s.mem.upper;

		case 0x51:
			return s.mem.lower;

		case 0x52:
			return s.mem.peripheral;

		case 0x53:
			return s.mem.middle;

		case 0x54:
			return s.mem.middle_size;

		case 0x60:
		case 0x68:
			which = (offset - 0x60) / 8;
			return s.dma[which].source;

		case 0x61:
		case 0x69:
			which = (offset - 0x61) / 8;
			return s.dma[which].source >> 16;

		case 0x62:
		case 0x6a:
			which = (offset - 0x62) / 8;
			return s.dma[which].dest;

		case 0x63:
		case 0x6b:
			which = (offset - 0x63) / 8;
			return s.dma[which].dest >> 16;

		case 0x64:
		case 0x6c:
			which = (offset - 0x64) / 8;
			return s.dma[which].count;

		case 0x65:
		case 0x6d:
			which = (offset - 0x65) / 8;
			return s.dma[which].control;

		case 0x7f:
			return s.reloc;
	}

	return 0x0000;
}

static void i186_chip_select(nec_state_t *nec_state, INT32 which, UINT16 data)
{
	if (nec_state->i186_chip_select_cb)
		nec_state->i186_chip_select_cb(which, data);
}

static void i186_internal_w(nec_state_t *nec_state, UINT32 offset, UINT16 data)
{
	i186_state &s = nec_state->i186;
	INT32 which;

	switch (offset)
	{
		case 0x10:
			s.intr.vector = data & 0xf8;
			break;

		case 0x11:
			i186_handle_eoi(nec_state, data);
			i186_update_interrupt_state(nec_state);
			break;

		case 0x14:
			s.intr.timer  = (s.intr.timer  & ~0x08) | ((data << 3) & 0x08);
			s.intr.dma[0] = (s.intr.dma[0] & ~0x08) | ((data << 1) & 0x08);
			s.intr.dma[1] = (s.intr.dma[1] & ~0x08) | ((data << 0) & 0x08);
			s.intr.ext[0] = (s.intr.ext[0] & ~0x08) | ((data >> 1) & 0x08);
			s.intr.ext[1] = (s.intr.ext[1] & ~0x08) | ((data >> 2) & 0x08);
			s.intr.ext[2] = (s.intr.ext[2] & ~0x08) | ((data >> 3) & 0x08);
			s.intr.ext[3] = (s.intr.ext[3] & ~0x08) | ((data >> 4) & 0x08);
			i186_update_interrupt_state(nec_state);
			break;

		case 0x15:
			s.intr.priority_mask = data & 0x0007;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x16:
			s.intr.in_service = data & 0x00ff;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x17:
			s.intr.request = (s.intr.request & ~0x000c) | (data & 0x000c);
			i186_update_interrupt_state(nec_state);
			break;

		case 0x18:
			s.intr.status = (s.intr.status & ~0x8007) | (data & 0x8007);
			i186_update_interrupt_state(nec_state);
			break;

		case 0x19:
			s.intr.timer = data & 0x000f;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x1a:
			s.intr.dma[0] = data & 0x000f;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x1b:
			s.intr.dma[1] = data & 0x000f;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x1c:
			s.intr.ext[0] = data & 0x007f;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x1d:
			s.intr.ext[1] = data & 0x007f;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x1e:
			s.intr.ext[2] = data & 0x001f;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x1f:
			s.intr.ext[3] = data & 0x001f;
			i186_update_interrupt_state(nec_state);
			break;

		case 0x28:
		case 0x2c:
		case 0x30:
			which = (offset - 0x28) / 4;
			i186_internal_timer_update(nec_state, which, data, -1, -1, -1);
			break;

		case 0x29:
		case 0x2d:
		case 0x31:
			which = (offset - 0x29) / 4;
			i186_internal_timer_update(nec_state, which, -1, data, -1, -1);
			break;

		case 0x2a:
		case 0x2e:
			which = (offset - 0x2a) / 4;
			i186_internal_timer_update(nec_state, which, -1, -1, data, -1);
			break;

		case 0x2b:
		case 0x2f:
		case 0x33:
			which = (offset - 0x2b) / 4;
			i186_internal_timer_update(nec_state, which, -1, -1, -1, data);
			break;

		case 0x50:
			s.mem.upper = data | 0xc038;
			i186_chip_select(nec_state, 0, s.mem.upper);
			break;

		case 0x51:
			s.mem.lower = (data & 0x3fff) | 0x0038;
			i186_chip_select(nec_state, 1, s.mem.lower);
			break;

		case 0x52:
			s.mem.peripheral = data | 0x0038;
			i186_chip_select(nec_state, 2, s.mem.peripheral);
			break;

		case 0x53:
			s.mem.middle = data | 0x01f8;
			i186_chip_select(nec_state, 3, s.mem.middle);
			break;

		case 0x54:
			s.mem.middle_size = data | 0x8038;
			i186_chip_select(nec_state, 4, s.mem.middle_size);
			break;

		case 0x60:
		case 0x68:
			which = (offset - 0x60) / 8;
			s.dma[which].source = (s.dma[which].source & ~0x0ffff) | (data & 0x0ffff);
			s.dma_latency = 8;
			break;

		case 0x61:
		case 0x69:
			which = (offset - 0x61) / 8;
			s.dma[which].source = (s.dma[which].source & ~0xf0000) | ((data << 16) & 0xf0000);
			s.dma_latency = 8;
			break;

		case 0x62:
		case 0x6a:
			which = (offset - 0x62) / 8;
			s.dma[which].dest = (s.dma[which].dest & ~0x0ffff) | (data & 0x0ffff);
			s.dma_latency = 8;
			break;

		case 0x63:
		case 0x6b:
			which = (offset - 0x63) / 8;
			s.dma[which].dest = (s.dma[which].dest & ~0xf0000) | ((data << 16) & 0xf0000);
			s.dma_latency = 8;
			break;

		case 0x64:
		case 0x6c:
			which = (offset - 0x64) / 8;
			s.dma[which].count = data;
			s.dma_latency = 8;
			break;

		case 0x65:
		case 0x6d:
			which = (offset - 0x65) / 8;
			i186_update_dma_control(nec_state, which, data);
			if ((s.dma[which].control & (SYNC_MASK | ST_STOP | TIMER_DRQ)) == ST_STOP)
			{
				// unsynchronized transfer, done all at once
				while (s.dma[which].control & ST_STOP)
					i186_drq(nec_state, which);
			}
			break;

		case 0x7f:
			s.reloc = data;
			nec_state->i186_mem_base = (data & 0x1000) ? ((data & 0xfff) << 8) : ~0;
			break;
	}
}

/*************************************
 *
 *  Reset & execution
 *
 *************************************/

static void i186_reset(nec_state_t *nec_state)
{
	i186_state &s = nec_state->i186;

	s.intr.priority_mask = 0x0007;
	s.intr.timer         = 0x000f;
	s.intr.dma[0]        = 0x000f;
	s.intr.dma[1]        = 0x000f;
	s.intr.ext[0]        = 0x000f;
	s.intr.ext[1]        = 0x000f;
	s.intr.ext[2]        = 0x000f;
	s.intr.ext[3]        = 0x000f;
	s.intr.in_service    = 0x0000;

	s.intr.pending       = 0x0000;
	s.intr.ack_mask      = 0x0000;
	s.intr.request       = 0x0000;
	s.intr.status        = 0x0000;
	s.intr.poll_status   = 0x0000;
	s.intr.ext_state     = 0x00;

	s.reloc = 0x20ff;
	s.mem.upper = 0xfffb;

	for (INT32 i = 0; i < 2; i++)
	{
		s.dma[i].drq_state = 0;
		s.dma[i].control = 0;
	}

	for (INT32 i = 0; i < 3; i++)
	{
		s.timer[i].control = 0;
		s.timer[i].maxA = 0;
		s.timer[i].maxB = 0;
		s.timer[i].count = 0;
		s.timer[i].remain = 0;
		s.timer[i].active = 0;
	}

	s.last_dma = 0;
	s.dma_latency = 0;
}

static int i186_execute(nec_state_t *nec_state, int cycles)
{
	i186_state &s = nec_state->i186;
	int prev_ICount;

	nec_state->icount = cycles;
	nec_state->cycles_remaining = cycles;
	nec_state->stop_run = 0;

	while ((nec_state->icount > 0) && (!nec_state->stop_run))
	{
		if ((s.dma[0].drq_state && (s.dma[0].control & ST_STOP)) || (s.dma[1].drq_state && (s.dma[1].control & ST_STOP)))
		{
			// a real 80186 has some latency after writing to the DMA registers before a transfer will fire
			if (!s.dma_latency)
			{
				INT32 channel = s.last_dma ? 0 : 1;
				s.last_dma = !s.last_dma;
				if (!(s.dma[1].drq_state && (s.dma[1].control & ST_STOP)))
					channel = 0;
				else if (!(s.dma[0].drq_state && (s.dma[0].control & ST_STOP)))
					channel = 1;
				else if ((s.dma[0].control & CHANNEL_PRIORITY) && !(s.dma[1].control & CHANNEL_PRIORITY))
					channel = 0;
				else if ((s.dma[1].control & CHANNEL_PRIORITY) && !(s.dma[0].control & CHANNEL_PRIORITY))
					channel = 1;
				nec_state->icount--;
				i186_drq(nec_state, channel);
				i186_advance_timers(nec_state, 1);
				continue;
			}
		}

		if (s.dma_latency > 0)
			s.dma_latency--;

		if (nec_state->halted)
		{
			// timers keep running and can wake up the cpu
			INT32 burn = i186_next_timer_event(nec_state);
			if (burn > nec_state->icount) burn = nec_state->icount;
			if (burn < 1) burn = 1;
			nec_state->icount -= burn;
			i186_advance_timers(nec_state, burn);
			continue;
		}

		/* Dispatch IRQ */
		if (nec_state->pending_irq && nec_state->no_interrupt==0)
		{
			if (nec_state->pending_irq & NMI_IRQ)
				external_int(nec_state);
			else if (nec_state->IF)
				external_int(nec_state);
		}

		/* No interrupt allowed between last instruction and this one */
		if (nec_state->no_interrupt)
			nec_state->no_interrupt--;

		prev_ICount = nec_state->icount;
		NEC_OPTABLE[fetchop(nec_state)](nec_state);
		do_prefetch(nec_state, prev_ICount);

		i186_advance_timers(nec_state, prev_ICount - nec_state->icount);
	}

	cycles = cycles - nec_state->icount;
	nec_state->cycles_total += cycles;
	nec_state->cycles_remaining = nec_state->icount = 0;

	return cycles;
}

/*************************************
 *
 *  Driver interface
 *
 *************************************/

void VezI186SetChipSelectCallback(void (*cb)(INT32 which, UINT16 data))
{
	sChipsPtr->i186_chip_select_cb = cb;
}

void VezI186SetTimerOutCallback(void (*cb)(INT32 which, INT32 state))
{
	sChipsPtr->i186_timer_out_cb = cb;
}
