/**********************************************************************

    Motorola 6821 PIA interface and emulation

    This function emulates all the functionality of up to 8 M6821
    peripheral interface adapters.

    MAME sources by Nathan Woods

**********************************************************************/

#include <string.h>
#include <stdio.h>
#include "burnint.h"
#include "6821pia.h"

#ifdef __LP64__
#define FPTR unsigned long   /* 64bit: sizeof(void *) is sizeof(long)  */
#else
#define FPTR unsigned int
#endif

#define VERBOSE 0

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif


/******************* internal PIA data structure *******************/

struct pia6821
{
	const struct pia6821_interface *intf;
	UINT8 addr;

	UINT8 in_a;
	UINT8 in_ca1;
	UINT8 in_ca2;
	UINT8 out_a;
	UINT8 out_ca2;
	UINT8 ddr_a;
	UINT8 ctl_a;
	UINT8 irq_a1;
	UINT8 irq_a2;
	UINT8 irq_a_state;

	UINT8 in_b;
	UINT8 in_cb1;
	UINT8 in_cb2;
	UINT8 out_b;
	UINT8 out_cb2;
	UINT8 ddr_b;
	UINT8 ctl_b;
	UINT8 irq_b1;
	UINT8 irq_b2;
	UINT8 irq_b_state;
	UINT8 in_set; // which input ports are set
};


/******************* convenince macros and defines *******************/

#define PIA_IRQ1				0x80
#define PIA_IRQ2				0x40

#define IRQ1_ENABLED(c)			(c & 0x01)
#define IRQ1_DISABLED(c)		(!(c & 0x01))
#define C1_LOW_TO_HIGH(c)		(c & 0x02)
#define C1_HIGH_TO_LOW(c)		(!(c & 0x02))
#define OUTPUT_SELECTED(c)		(c & 0x04)
#define DDR_SELECTED(c)			(!(c & 0x04))
#define IRQ2_ENABLED(c)			(c & 0x08)
#define IRQ2_DISABLED(c)		(!(c & 0x08))
#define STROBE_E_RESET(c)		(c & 0x08)
#define STROBE_C1_RESET(c)		(!(c & 0x08))
#define SET_C2(c)				(c & 0x08)
#define RESET_C2(c)				(!(c & 0x08))
#define C2_LOW_TO_HIGH(c)		(c & 0x10)
#define C2_HIGH_TO_LOW(c)		(!(c & 0x10))
#define C2_SET_MODE(c)			(c & 0x10)
#define C2_STROBE_MODE(c)		(!(c & 0x10))
#define C2_OUTPUT(c)			(c & 0x20)
#define C2_INPUT(c)				(!(c & 0x20))

#define PIA_IN_SET_A   0x01
#define PIA_IN_SET_CA1 0x02
#define PIA_IN_SET_CA2 0x04
#define PIA_IN_SET_B   0x08
#define PIA_IN_SET_CB1 0x10
#define PIA_IN_SET_CB2 0x20

/******************* static variables *******************/

static struct pia6821 pia[MAX_PIA];

static const UINT8 swizzle_address[4] = { 0, 2, 1, 3 };



/******************* stave state *******************/
#if 0
static void update_6821_interrupts(struct pia6821 *p);

static void pia_postload(int which)
{
	struct pia6821 *p = pia + which;
	update_6821_interrupts(p);
	if (p->intf->out_a_func && p->ddr_a) p->intf->out_a_func(0, p->out_a & p->ddr_a);
	if (p->intf->out_b_func && p->ddr_b) p->intf->out_b_func(0, p->out_b & p->ddr_b);
	if (p->intf->out_ca2_func) p->intf->out_ca2_func(0, p->out_ca2);
	if (p->intf->out_cb2_func) p->intf->out_cb2_func(0, p->out_cb2);
}

static void pia_postload_0(void)
{
	pia_postload(0);
}

static void pia_postload_1(void)
{
	pia_postload(1);
}

static void pia_postload_2(void)
{
	pia_postload(2);
}

static void pia_postload_3(void)
{
	pia_postload(3);
}

static void pia_postload_4(void)
{
	pia_postload(4);
}

static void pia_postload_5(void)
{
	pia_postload(5);
}

static void pia_postload_6(void)
{
	pia_postload(6);
}

static void pia_postload_7(void)
{
	pia_postload(7);
}

static void (*pia_postload_funcs[MAX_PIA])(void) =
{
	pia_postload_0,
	pia_postload_1,
	pia_postload_2,
	pia_postload_3,
	pia_postload_4,
	pia_postload_5,
	pia_postload_6,
	pia_postload_7
};

void pia_init(int count)
{
	int i;
	for (i = 0; i < count; i++)
	{
		state_save_register_UINT8("6821pia", i, "in_a",		&pia[i].in_a, 1);
		state_save_register_UINT8("6821pia", i, "in_ca1",	&pia[i].in_ca1, 1);
		state_save_register_UINT8("6821pia", i, "in_ca2",	&pia[i].in_ca2, 1);
		state_save_register_UINT8("6821pia", i, "out_a",	&pia[i].out_a, 1);
		state_save_register_UINT8("6821pia", i, "out_ca2",	&pia[i].out_ca2, 1);
		state_save_register_UINT8("6821pia", i, "ddr_a",	&pia[i].ddr_a, 1);
		state_save_register_UINT8("6821pia", i, "ctl_a",	&pia[i].ctl_a, 1);
		state_save_register_UINT8("6821pia", i, "irq_a1",	&pia[i].irq_a1, 1);
		state_save_register_UINT8("6821pia", i, "irq_a2",	&pia[i].irq_a2, 1);
		state_save_register_UINT8("6821pia", i, "in_b",		&pia[i].in_b, 1);
		state_save_register_UINT8("6821pia", i, "in_cb1",	&pia[i].in_cb1, 1);
		state_save_register_UINT8("6821pia", i, "in_cb2",	&pia[i].in_cb2, 1);
		state_save_register_UINT8("6821pia", i, "out_b",	&pia[i].out_b, 1);
		state_save_register_UINT8("6821pia", i, "out_cb2",	&pia[i].out_cb2, 1);
		state_save_register_UINT8("6821pia", i, "ddr_b",	&pia[i].ddr_b, 1);
		state_save_register_UINT8("6821pia", i, "ctl_b",	&pia[i].ctl_b, 1);
		state_save_register_UINT8("6821pia", i, "irq_b1",	&pia[i].irq_b1, 1);
		state_save_register_UINT8("6821pia", i, "irq_b2",	&pia[i].irq_b2, 1);
		state_save_register_UINT8("6821pia", i, "in_set",	&pia[i].in_set, 1);
		state_save_register_func_postload(pia_postload_funcs[i]);
	}
}
#endif

/******************* un-configuration *******************/

void pia_init(void) // was pia_unconfig
{
	memset(&pia, 0, sizeof(pia));
}

void pia_exit(void)
{
	pia_init();
}

/******************* configuration *******************/

void pia_config(int which, int addressing, const struct pia6821_interface *intf)
{
	if (which >= MAX_PIA) return;
	memset(&pia[which], 0, sizeof(pia[0]));
	if (!intf) return;
	pia[which].intf = intf;
	pia[which].addr = addressing;
	// set default read values.
	// Ports A,CA1,CA2 default to 1
	// Ports B,CB1,CB2 are three-state and undefined (set to 0)
	pia[which].in_a = pia[which].in_ca1 = pia[which].in_ca2 = 0xff;
	if ((intf->in_a_func) && ((FPTR)(intf->in_a_func) <= 0x100))
		{ pia[which].in_a = ((FPTR)(intf->in_a_func) - 1); pia[which].in_set |= PIA_IN_SET_A; }
	if ((intf->in_b_func) && ((FPTR)(intf->in_b_func) <= 0x100))
		{ pia[which].in_b = ((FPTR)(intf->in_b_func) - 1); pia[which].in_set |= PIA_IN_SET_B; }
	if ((intf->in_ca1_func) && ((FPTR)(intf->in_ca1_func) <= 0x100))
		{ pia[which].in_ca1 = ((FPTR)(intf->in_ca1_func) - 1); pia[which].in_set |= PIA_IN_SET_CA1; }
	if ((intf->in_ca2_func) && ((FPTR)(intf->in_ca2_func) <= 0x100))
		{ pia[which].in_ca2 = ((FPTR)(intf->in_ca2_func) - 1); pia[which].in_set |= PIA_IN_SET_CA2; }
	if ((intf->in_cb1_func) && ((FPTR)(intf->in_cb1_func) <= 0x100))
		{ pia[which].in_cb1 = ((FPTR)(intf->in_cb1_func) - 1); pia[which].in_set |= PIA_IN_SET_CB1; }
	if ((intf->in_cb2_func) && ((FPTR)(intf->in_cb2_func) <= 0x100))
		{ pia[which].in_cb2 = ((FPTR)(intf->in_cb2_func) - 1); pia[which].in_set |= PIA_IN_SET_CB2; }
}


/******************* reset *******************/

void pia_reset(void)
{
	int i;

	/* zap each structure, preserving the interface and swizzle */
	for (i = 0; i < MAX_PIA; i++) pia_config(i, pia[i].addr, pia[i].intf);
}


/******************* wire-OR for all interrupt handlers *******************/

static void update_shared_irq_handler(void (*irq_func)(int state))
{
	int i;

	/* search all PIAs for this same IRQ function */
	for (i = 0; i < MAX_PIA; i++)
		if (pia[i].intf)
		{
			/* check IRQ A */
			if (pia[i].intf->irq_a_func == irq_func && pia[i].irq_a_state)
			{
				(*irq_func)(1);
				return;
			}

			/* check IRQ B */
			if (pia[i].intf->irq_b_func == irq_func && pia[i].irq_b_state)
			{
				(*irq_func)(1);
				return;
			}
		}

	/* if we found nothing, the state is off */
	(*irq_func)(0);
}


/******************* external interrupt check *******************/

static void update_6821_interrupts(struct pia6821 *p)
{
	int new_state;

	/* start with IRQ A */
	new_state = 0;
	if ((p->irq_a1 && IRQ1_ENABLED(p->ctl_a)) || (p->irq_a2 && IRQ2_ENABLED(p->ctl_a))) new_state = 1;
	if (new_state != p->irq_a_state)
	{
		p->irq_a_state = new_state;
		if (p->intf->irq_a_func) update_shared_irq_handler(p->intf->irq_a_func);
	}

	/* then do IRQ B */
	new_state = 0;
	if ((p->irq_b1 && IRQ1_ENABLED(p->ctl_b)) || (p->irq_b2 && IRQ2_ENABLED(p->ctl_b))) new_state = 1;
	if (new_state != p->irq_b_state)
	{
		p->irq_b_state = new_state;
		if (p->intf->irq_b_func) update_shared_irq_handler(p->intf->irq_b_func);
	}
}


/******************* CPU interface for PIA read *******************/

int pia_read(int which, int offset)
{
	struct pia6821 *p = pia + which;
	int val = 0;

	/* adjust offset for 16-bit and ordering */
	offset &= 3;
	if (p->addr & PIA_ALTERNATE_ORDERING) offset = swizzle_address[offset];

	switch (offset)
	{
		/******************* port A output/DDR read *******************/
		case PIA_DDRA:

			/* read output register */
			if (OUTPUT_SELECTED(p->ctl_a))
			{
				/* update the input */
				if ((FPTR)(p->intf->in_a_func) > 0x100)
					p->in_a = p->intf->in_a_func(0);
#ifdef MAME_DEBUG
				else if ((p->ddr_a ^ 0xff) && !(p->in_set & PIA_IN_SET_A)) {
					logerror("PIA%d: Warning! no port A read handler. Assuming pins %02x not connected\n",
					         which, p->ddr_a ^ 0xff);
					p->in_set |= PIA_IN_SET_A; // disable logging
				}
#endif // MAME_DEBUG

				/* combine input and output values */
				val = (p->out_a & p->ddr_a) + (p->in_a & ~p->ddr_a);

				/* IRQ flags implicitly cleared by a read */
				p->irq_a1 = p->irq_a2 = 0;
				update_6821_interrupts(p);

				/* CA2 is configured as output and in read strobe mode */
				if (C2_OUTPUT(p->ctl_a) && C2_STROBE_MODE(p->ctl_a))
				{
					/* this will cause a transition low; call the output function if we're currently high */
					if (p->out_ca2)
						if (p->intf->out_ca2_func) p->intf->out_ca2_func(0, 0);
					p->out_ca2 = 0;

					/* if the CA2 strobe is cleared by the E, reset it right away */
					if (STROBE_E_RESET(p->ctl_a))
					{
						if (p->intf->out_ca2_func) p->intf->out_ca2_func(0, 1);
						p->out_ca2 = 1;
					}
				}

				LOG(("%04x: PIA%d read port A = %02X\n", activecpu_get_previouspc(),  which, val));
			}

			/* read DDR register */
			else
			{
				val = p->ddr_a;
				LOG(("%04x: PIA%d read DDR A = %02X\n", activecpu_get_previouspc(), which, val));
			}
			break;

		/******************* port B output/DDR read *******************/
		case PIA_DDRB:

			/* read output register */
			if (OUTPUT_SELECTED(p->ctl_b))
			{
				/* update the input */
				if ((FPTR)(p->intf->in_b_func) > 0x100)
					p->in_b = p->intf->in_b_func(0);
#ifdef MAME_DEBUG
				else if ((p->ddr_b ^ 0xff) && !(p->in_set & PIA_IN_SET_B)) {
					logerror("PIA%d: Error! no port B read handler. Three-state pins %02x are undefined\n",
					         which, p->ddr_b ^ 0xff);
					p->in_set |= PIA_IN_SET_B; // disable logging
				}
#endif // MAME_DEBUG

				/* combine input and output values */
				val = (p->out_b & p->ddr_b) + (p->in_b & ~p->ddr_b);

				/* This read will implicitly clear the IRQ B1 flag.  If CB2 is in write-strobe
                   output mode with CB1 restore, and a CB1 active transition set the flag,
                   clearing it will cause CB2 to go high again.  Note that this is different
                   from what happens with port A. */
				if (p->irq_b1 && C2_OUTPUT(p->ctl_b) && C2_STROBE_MODE(p->ctl_b) && STROBE_C1_RESET(p->ctl_b))
				{
					/* call the CB2 output function */
					if (!p->out_cb2)
						if (p->intf->out_cb2_func) p->intf->out_cb2_func(0, 1);

					/* clear CB2 */
					p->out_cb2 = 1;
				}

				/* IRQ flags implicitly cleared by a read */
				p->irq_b1 = p->irq_b2 = 0;
				update_6821_interrupts(p);

				LOG(("%04x: PIA%d read port B = %02X\n", activecpu_get_previouspc(), which, val));
			}

			/* read DDR register */
			else
			{
				val = p->ddr_b;
				LOG(("%04x: PIA%d read DDR B = %02X\n", activecpu_get_previouspc(), which, val));
			}
			break;

		/******************* port A control read *******************/
		case PIA_CTLA:

			/* Update CA1 & CA2 if callback exists, these in turn may update IRQ's */
			if ((FPTR)(p->intf->in_ca1_func) > 0x100)
				pia_set_input_ca1(which, p->intf->in_ca1_func(0));
#ifdef MAME_DEBUG
			else if (!(p->in_set & PIA_IN_SET_CA1)) {
				logerror("PIA%d: Warning! no CA1 read handler. Assuming pin not connected\n",which);
				p->in_set |= PIA_IN_SET_CA1; // disable logging
			}
#endif // MAME_DEBUG
			if ((FPTR)(p->intf->in_ca2_func) > 0x100)
				pia_set_input_ca2(which, p->intf->in_ca2_func(0));
#ifdef MAME_DEBUG
			else if (C2_INPUT(p->ctl_a) && !(p->in_set & PIA_IN_SET_CA2)) {
				logerror("PIA%d: Warning! no CA2 read handler. Assuming pin not connected\n",which);
				p->in_set |= PIA_IN_SET_CA2; // disable logging
			}
#endif // MAME_DEBUG

			/* read control register */
			val = p->ctl_a;

			/* set the IRQ flags if we have pending IRQs */
			if (p->irq_a1) val |= PIA_IRQ1;
			if (p->irq_a2 && C2_INPUT(p->ctl_a)) val |= PIA_IRQ2;

			LOG(("%04x: PIA%d read control A = %02X\n", activecpu_get_previouspc(), which, val));
			break;

		/******************* port B control read *******************/
		case PIA_CTLB:

			/* Update CB1 & CB2 if callback exists, these in turn may update IRQ's */
			if ((FPTR)(p->intf->in_cb1_func) > 0x100)
				pia_set_input_cb1(which, p->intf->in_cb1_func(0));
#ifdef MAME_DEBUG
			else if (!(p->in_set & PIA_IN_SET_CB1)) {
				logerror("PIA%d: Error! no CB1 read handler. Three-state pin is undefined\n",which);
				p->in_set |= PIA_IN_SET_CB1; // disable logging
			}
#endif // MAME_DEBUG
			if ((FPTR)(p->intf->in_cb2_func) > 0x100)
				pia_set_input_cb2(which, p->intf->in_cb2_func(0));
#ifdef MAME_DEBUG
			else if (C2_INPUT(p->ctl_b) && !(p->in_set & PIA_IN_SET_CB2)) {
				logerror("PIA%d: Error! no CB2 read handler. Three-state pin is undefined\n",which);
				p->in_set |= PIA_IN_SET_CB2; // disable logging
			}
#endif // MAME_DEBUG

			/* read control register */
			val = p->ctl_b;

			/* set the IRQ flags if we have pending IRQs */
			if (p->irq_b1) val |= PIA_IRQ1;
			if (p->irq_b2 && C2_INPUT(p->ctl_b)) val |= PIA_IRQ2;

			LOG(("%04x: PIA%d read control B = %02X\n", activecpu_get_previouspc(), which, val));
			break;
	}

	return val;
}


/******************* CPU interface for PIA write *******************/

void pia_write(int which, int offset, int data)
{
	struct pia6821 *p = pia + which;

	/* adjust offset for 16-bit and ordering */
	offset &= 3;
	if (p->addr & PIA_ALTERNATE_ORDERING) offset = swizzle_address[offset];

	switch (offset)
	{
		/******************* port A output/DDR write *******************/
		case PIA_DDRA:

			/* write output register */
			if (OUTPUT_SELECTED(p->ctl_a))
			{
				LOG(("%04x: PIA%d port A write = %02X\n", activecpu_get_previouspc(), which, data));

				/* update the output value */
				p->out_a = data;/* & p->ddr_a; */	/* NS990130 - don't mask now, DDR could change later */

				/* send it to the output function */
				if (p->intf->out_a_func && p->ddr_a) p->intf->out_a_func(0, p->out_a & p->ddr_a);	/* NS990130 */
			}

			/* write DDR register */
			else
			{
				LOG(("%04x: PIA%d DDR A write = %02X\n", activecpu_get_previouspc(), which, data));

				if (p->ddr_a != data)
				{
					/* NS990130 - if DDR changed, call the callback again */
					p->ddr_a = data;

					/* send it to the output function */
					if (p->intf->out_a_func && p->ddr_a) p->intf->out_a_func(0, p->out_a & p->ddr_a);
				}
			}
			break;

		/******************* port B output/DDR write *******************/
		case PIA_DDRB:

			/* write output register */
			if (OUTPUT_SELECTED(p->ctl_b))
			{
				LOG(("%04x: PIA%d port B write = %02X\n", activecpu_get_previouspc(), which, data));

				/* update the output value */
				p->out_b = data;/* & p->ddr_b */	/* NS990130 - don't mask now, DDR could change later */

				/* send it to the output function */
				if (p->intf->out_b_func && p->ddr_b) p->intf->out_b_func(0, p->out_b & p->ddr_b);	/* NS990130 */

				/* CB2 is configured as output and in write strobe mode */
				if (C2_OUTPUT(p->ctl_b) && C2_STROBE_MODE(p->ctl_b))
				{
					/* this will cause a transition low; call the output function if we're currently high */
					if (p->out_cb2)
						if (p->intf->out_cb2_func) p->intf->out_cb2_func(0, 0);
					p->out_cb2 = 0;

					/* if the CB2 strobe is cleared by the E, reset it right away */
					if (STROBE_E_RESET(p->ctl_b))
					{
						if (p->intf->out_cb2_func) p->intf->out_cb2_func(0, 1);
						p->out_cb2 = 1;
					}
				}
			}

			/* write DDR register */
			else
			{
				LOG(("%04x: PIA%d DDR B write = %02X\n", activecpu_get_previouspc(), which, data));

				if (p->ddr_b != data)
				{
					/* NS990130 - if DDR changed, call the callback again */
					p->ddr_b = data;

					/* send it to the output function */
					if (p->intf->out_b_func && p->ddr_b) p->intf->out_b_func(0, p->out_b & p->ddr_b);
				}
			}
			break;

		/******************* port A control write *******************/
		case PIA_CTLA:

			/* Bit 7 and 6 read only - PD 16/01/00 */

			data &= 0x3f;


			LOG(("%04x: PIA%d control A write = %02X\n", activecpu_get_previouspc(), which, data));

			/* CA2 is configured as output */
			if (C2_OUTPUT(data))
			{
				int temp;

				if (C2_SET_MODE(data))
				{
					/* set/reset mode--bit value determines the new output */
					temp = SET_C2(data) ? 1 : 0;
				}
				else {
					/* strobe mode--output is always high unless strobed. */
					temp = 1;
				}

				/* if we're going from input to output mode, or we're already in output mode
                   and this change creates a transition, call the CA2 output function */
				if (C2_INPUT(p->ctl_a) || (C2_OUTPUT(p->ctl_a) && (p->out_ca2 ^ temp)))
					if (p->intf->out_ca2_func) p->intf->out_ca2_func(0, temp);

				/* set the new value */
				p->out_ca2 = temp;
			}

			/* update the control register */
			p->ctl_a = data;

			/* update externals */
			update_6821_interrupts(p);
			break;

		/******************* port B control write *******************/
		case PIA_CTLB:

			/* Bit 7 and 6 read only - PD 16/01/00 */

			data &= 0x3f;

			LOG(("%04x: PIA%d control B write = %02X\n", activecpu_get_previouspc(), which, data));

			/* CB2 is configured as output */
			if (C2_OUTPUT(data))
			{
				int temp;

				if (C2_SET_MODE(data))
				{
					/* set/reset mode--bit value determines the new output */
					temp = SET_C2(data) ? 1 : 0;
				}
				else {
					/* strobe mode--output is always high unless strobed. */
					temp = 1;
				}

				/* if we're going from input to output mode, or we're already in output mode
                   and this change creates a transition, call the CB2 output function */
				if (C2_INPUT(p->ctl_b) || (C2_OUTPUT(p->ctl_b) && (p->out_cb2 ^ temp)))
					if (p->intf->out_cb2_func) p->intf->out_cb2_func(0, temp);

				/* set the new value */
				p->out_cb2 = temp;
			}

			/* update the control register */
			p->ctl_b = data;

			/* update externals */
			update_6821_interrupts(p);
			break;
	}
}


/******************* interface setting PIA port A input *******************/

void pia_set_input_a(int which, int data)
{
	struct pia6821 *p = pia + which;

	/* set the input, what could be easier? */
	p->in_a = data;
	p->in_set |= PIA_IN_SET_A;
}



/******************* interface setting PIA port CA1 input *******************/

void pia_set_input_ca1(int which, int data)
{
	struct pia6821 *p = pia + which;

	/* limit the data to 0 or 1 */
	data = data ? 1 : 0;

	/* the new state has caused a transition */
	if (p->in_ca1 ^ data)
	{
		/* handle the active transition */
		if ((data && C1_LOW_TO_HIGH(p->ctl_a)) || (!data && C1_HIGH_TO_LOW(p->ctl_a)))
		{
			/* mark the IRQ */
			p->irq_a1 = 1;

			/* update externals */
			update_6821_interrupts(p);

			/* CA2 is configured as output and in read strobe mode and cleared by a CA1 transition */
			if (C2_OUTPUT(p->ctl_a) && C2_STROBE_MODE(p->ctl_a) && STROBE_C1_RESET(p->ctl_a))
			{
				/* call the CA2 output function */
				if (!p->out_ca2)
					if (p->intf->out_ca2_func) p->intf->out_ca2_func(0, 1);

				/* clear CA2 */
				p->out_ca2 = 1;
			}
		}
	}

	/* set the new value for CA1 */
	p->in_ca1 = data;
	p->in_set |= PIA_IN_SET_CA1;
}



/******************* interface setting PIA port CA2 input *******************/

void pia_set_input_ca2(int which, int data)
{
	struct pia6821 *p = pia + which;

	/* limit the data to 0 or 1 */
	data = data ? 1 : 0;

	/* CA2 is in input mode */
	if (C2_INPUT(p->ctl_a))
	{
		/* the new state has caused a transition */
		if (p->in_ca2 ^ data)
		{
			/* handle the active transition */
			if ((data && C2_LOW_TO_HIGH(p->ctl_a)) || (!data && C2_HIGH_TO_LOW(p->ctl_a)))
			{
				/* mark the IRQ */
				p->irq_a2 = 1;

				/* update externals */
				update_6821_interrupts(p);
			}
		}
	}

	/* set the new value for CA2 */
	p->in_ca2 = data;
	p->in_set |= PIA_IN_SET_CA2;
}



/******************* interface setting PIA port B input *******************/

void pia_set_input_b(int which, int data)
{
	struct pia6821 *p = pia + which;

	/* set the input, what could be easier? */
	p->in_b = data;
    p->in_set |= PIA_IN_SET_B;
}



/******************* interface setting PIA port CB1 input *******************/

void pia_set_input_cb1(int which, int data)
{
	struct pia6821 *p = pia + which;

	/* limit the data to 0 or 1 */
	data = data ? 1 : 0;

	/* the new state has caused a transition */
	if (p->in_cb1 ^ data)
	{
		/* handle the active transition */
		if ((data && C1_LOW_TO_HIGH(p->ctl_b)) || (!data && C1_HIGH_TO_LOW(p->ctl_b)))
		{
			/* mark the IRQ */
			p->irq_b1 = 1;

			/* update externals */
			update_6821_interrupts(p);

			/* If CB2 is configured as a write-strobe output which is reset by a CB1
               transition, this reset will only happen when a read from port B implicitly
               clears the IRQ B1 flag.  So we handle the CB2 reset there.  Note that this
               is different from what happens with port A. */
		}
	}

	/* set the new value for CB1 */
	p->in_cb1 = data;
	p->in_set |= PIA_IN_SET_CB1;
}



/******************* interface setting PIA port CB2 input *******************/

void pia_set_input_cb2(int which, int data)
{
	struct pia6821 *p = pia + which;

	/* limit the data to 0 or 1 */
	data = data ? 1 : 0;

	/* CB2 is in input mode */
	if (C2_INPUT(p->ctl_b))
	{
		/* the new state has caused a transition */
		if (p->in_cb2 ^ data)
		{
			/* handle the active transition */
			if ((data && C2_LOW_TO_HIGH(p->ctl_b)) || (!data && C2_HIGH_TO_LOW(p->ctl_b)))
			{
				/* mark the IRQ */
				p->irq_b2 = 1;

				/* update externals */
				update_6821_interrupts(p);
			}
		}
	}

	/* set the new value for CA2 */
	p->in_cb2 = data;
	p->in_set |= PIA_IN_SET_CB2;
}



/******************* interface retrieving DDR *******************/

UINT8 pia_get_ddr_a(int which)
{
	struct pia6821 *p = pia + which;
	return p->ddr_a;
}



UINT8 pia_get_ddr_b(int which)
{
	struct pia6821 *p = pia + which;
	return p->ddr_b;
}



#if 0
/******************* Standard 8-bit CPU interfaces, D0-D7 *******************/

READ8_HANDLER( pia_0_r ) { return pia_read(0, offset); }
READ8_HANDLER( pia_1_r ) { return pia_read(1, offset); }
READ8_HANDLER( pia_2_r ) { return pia_read(2, offset); }
READ8_HANDLER( pia_3_r ) { return pia_read(3, offset); }
READ8_HANDLER( pia_4_r ) { return pia_read(4, offset); }
READ8_HANDLER( pia_5_r ) { return pia_read(5, offset); }
READ8_HANDLER( pia_6_r ) { return pia_read(6, offset); }
READ8_HANDLER( pia_7_r ) { return pia_read(7, offset); }

WRITE8_HANDLER( pia_0_w ) { pia_write(0, offset, data); }
WRITE8_HANDLER( pia_1_w ) { pia_write(1, offset, data); }
WRITE8_HANDLER( pia_2_w ) { pia_write(2, offset, data); }
WRITE8_HANDLER( pia_3_w ) { pia_write(3, offset, data); }
WRITE8_HANDLER( pia_4_w ) { pia_write(4, offset, data); }
WRITE8_HANDLER( pia_5_w ) { pia_write(5, offset, data); }
WRITE8_HANDLER( pia_6_w ) { pia_write(6, offset, data); }
WRITE8_HANDLER( pia_7_w ) { pia_write(7, offset, data); }

/******************* Standard 16-bit CPU interfaces, D0-D7 *******************/

READ16_HANDLER( pia_0_lsb_r ) { return pia_read(0, offset); }
READ16_HANDLER( pia_1_lsb_r ) { return pia_read(1, offset); }
READ16_HANDLER( pia_2_lsb_r ) { return pia_read(2, offset); }
READ16_HANDLER( pia_3_lsb_r ) { return pia_read(3, offset); }
READ16_HANDLER( pia_4_lsb_r ) { return pia_read(4, offset); }
READ16_HANDLER( pia_5_lsb_r ) { return pia_read(5, offset); }
READ16_HANDLER( pia_6_lsb_r ) { return pia_read(6, offset); }
READ16_HANDLER( pia_7_lsb_r ) { return pia_read(7, offset); }

WRITE16_HANDLER( pia_0_lsb_w ) { if (ACCESSING_LSB) pia_write(0, offset, data & 0xff); }
WRITE16_HANDLER( pia_1_lsb_w ) { if (ACCESSING_LSB) pia_write(1, offset, data & 0xff); }
WRITE16_HANDLER( pia_2_lsb_w ) { if (ACCESSING_LSB) pia_write(2, offset, data & 0xff); }
WRITE16_HANDLER( pia_3_lsb_w ) { if (ACCESSING_LSB) pia_write(3, offset, data & 0xff); }
WRITE16_HANDLER( pia_4_lsb_w ) { if (ACCESSING_LSB) pia_write(4, offset, data & 0xff); }
WRITE16_HANDLER( pia_5_lsb_w ) { if (ACCESSING_LSB) pia_write(5, offset, data & 0xff); }
WRITE16_HANDLER( pia_6_lsb_w ) { if (ACCESSING_LSB) pia_write(6, offset, data & 0xff); }
WRITE16_HANDLER( pia_7_lsb_w ) { if (ACCESSING_LSB) pia_write(7, offset, data & 0xff); }

/******************* Standard 16-bit CPU interfaces, D8-D15 *******************/

READ16_HANDLER( pia_0_msb_r ) { return pia_read(0, offset) << 8; }
READ16_HANDLER( pia_1_msb_r ) { return pia_read(1, offset) << 8; }
READ16_HANDLER( pia_2_msb_r ) { return pia_read(2, offset) << 8; }
READ16_HANDLER( pia_3_msb_r ) { return pia_read(3, offset) << 8; }
READ16_HANDLER( pia_4_msb_r ) { return pia_read(4, offset) << 8; }
READ16_HANDLER( pia_5_msb_r ) { return pia_read(5, offset) << 8; }
READ16_HANDLER( pia_6_msb_r ) { return pia_read(6, offset) << 8; }
READ16_HANDLER( pia_7_msb_r ) { return pia_read(7, offset) << 8; }

WRITE16_HANDLER( pia_0_msb_w ) { if (ACCESSING_MSB) pia_write(0, offset, data >> 8); }
WRITE16_HANDLER( pia_1_msb_w ) { if (ACCESSING_MSB) pia_write(1, offset, data >> 8); }
WRITE16_HANDLER( pia_2_msb_w ) { if (ACCESSING_MSB) pia_write(2, offset, data >> 8); }
WRITE16_HANDLER( pia_3_msb_w ) { if (ACCESSING_MSB) pia_write(3, offset, data >> 8); }
WRITE16_HANDLER( pia_4_msb_w ) { if (ACCESSING_MSB) pia_write(4, offset, data >> 8); }
WRITE16_HANDLER( pia_5_msb_w ) { if (ACCESSING_MSB) pia_write(5, offset, data >> 8); }
WRITE16_HANDLER( pia_6_msb_w ) { if (ACCESSING_MSB) pia_write(6, offset, data >> 8); }
WRITE16_HANDLER( pia_7_msb_w ) { if (ACCESSING_MSB) pia_write(7, offset, data >> 8); }

/******************* 8-bit A/B port interfaces *******************/

WRITE8_HANDLER( pia_0_porta_w ) { pia_set_input_a(0, data); }
WRITE8_HANDLER( pia_1_porta_w ) { pia_set_input_a(1, data); }
WRITE8_HANDLER( pia_2_porta_w ) { pia_set_input_a(2, data); }
WRITE8_HANDLER( pia_3_porta_w ) { pia_set_input_a(3, data); }
WRITE8_HANDLER( pia_4_porta_w ) { pia_set_input_a(4, data); }
WRITE8_HANDLER( pia_5_porta_w ) { pia_set_input_a(5, data); }
WRITE8_HANDLER( pia_6_porta_w ) { pia_set_input_a(6, data); }
WRITE8_HANDLER( pia_7_porta_w ) { pia_set_input_a(7, data); }

WRITE8_HANDLER( pia_0_portb_w ) { pia_set_input_b(0, data); }
WRITE8_HANDLER( pia_1_portb_w ) { pia_set_input_b(1, data); }
WRITE8_HANDLER( pia_2_portb_w ) { pia_set_input_b(2, data); }
WRITE8_HANDLER( pia_3_portb_w ) { pia_set_input_b(3, data); }
WRITE8_HANDLER( pia_4_portb_w ) { pia_set_input_b(4, data); }
WRITE8_HANDLER( pia_5_portb_w ) { pia_set_input_b(5, data); }
WRITE8_HANDLER( pia_6_portb_w ) { pia_set_input_b(6, data); }
WRITE8_HANDLER( pia_7_portb_w ) { pia_set_input_b(7, data); }

READ8_HANDLER( pia_0_porta_r ) { return pia[0].in_a; }
READ8_HANDLER( pia_1_porta_r ) { return pia[1].in_a; }
READ8_HANDLER( pia_2_porta_r ) { return pia[2].in_a; }
READ8_HANDLER( pia_3_porta_r ) { return pia[3].in_a; }
READ8_HANDLER( pia_4_porta_r ) { return pia[4].in_a; }
READ8_HANDLER( pia_5_porta_r ) { return pia[5].in_a; }
READ8_HANDLER( pia_6_porta_r ) { return pia[6].in_a; }
READ8_HANDLER( pia_7_porta_r ) { return pia[7].in_a; }

READ8_HANDLER( pia_0_portb_r ) { return pia[0].in_b; }
READ8_HANDLER( pia_1_portb_r ) { return pia[1].in_b; }
READ8_HANDLER( pia_2_portb_r ) { return pia[2].in_b; }
READ8_HANDLER( pia_3_portb_r ) { return pia[3].in_b; }
READ8_HANDLER( pia_4_portb_r ) { return pia[4].in_b; }
READ8_HANDLER( pia_5_portb_r ) { return pia[5].in_b; }
READ8_HANDLER( pia_6_portb_r ) { return pia[6].in_b; }
READ8_HANDLER( pia_7_portb_r ) { return pia[7].in_b; }

/******************* 1-bit CA1/CA2/CB1/CB2 port interfaces *******************/

WRITE8_HANDLER( pia_0_ca1_w ) { pia_set_input_ca1(0, data); }
WRITE8_HANDLER( pia_1_ca1_w ) { pia_set_input_ca1(1, data); }
WRITE8_HANDLER( pia_2_ca1_w ) { pia_set_input_ca1(2, data); }
WRITE8_HANDLER( pia_3_ca1_w ) { pia_set_input_ca1(3, data); }
WRITE8_HANDLER( pia_4_ca1_w ) { pia_set_input_ca1(4, data); }
WRITE8_HANDLER( pia_5_ca1_w ) { pia_set_input_ca1(5, data); }
WRITE8_HANDLER( pia_6_ca1_w ) { pia_set_input_ca1(6, data); }
WRITE8_HANDLER( pia_7_ca1_w ) { pia_set_input_ca1(7, data); }
WRITE8_HANDLER( pia_0_ca2_w ) { pia_set_input_ca2(0, data); }
WRITE8_HANDLER( pia_1_ca2_w ) { pia_set_input_ca2(1, data); }
WRITE8_HANDLER( pia_2_ca2_w ) { pia_set_input_ca2(2, data); }
WRITE8_HANDLER( pia_3_ca2_w ) { pia_set_input_ca2(3, data); }
WRITE8_HANDLER( pia_4_ca2_w ) { pia_set_input_ca2(4, data); }
WRITE8_HANDLER( pia_5_ca2_w ) { pia_set_input_ca2(5, data); }
WRITE8_HANDLER( pia_6_ca2_w ) { pia_set_input_ca2(6, data); }
WRITE8_HANDLER( pia_7_ca2_w ) { pia_set_input_ca2(7, data); }

WRITE8_HANDLER( pia_0_cb1_w ) { pia_set_input_cb1(0, data); }
WRITE8_HANDLER( pia_1_cb1_w ) { pia_set_input_cb1(1, data); }
WRITE8_HANDLER( pia_2_cb1_w ) { pia_set_input_cb1(2, data); }
WRITE8_HANDLER( pia_3_cb1_w ) { pia_set_input_cb1(3, data); }
WRITE8_HANDLER( pia_4_cb1_w ) { pia_set_input_cb1(4, data); }
WRITE8_HANDLER( pia_5_cb1_w ) { pia_set_input_cb1(5, data); }
WRITE8_HANDLER( pia_6_cb1_w ) { pia_set_input_cb1(6, data); }
WRITE8_HANDLER( pia_7_cb1_w ) { pia_set_input_cb1(7, data); }
WRITE8_HANDLER( pia_0_cb2_w ) { pia_set_input_cb2(0, data); }
WRITE8_HANDLER( pia_1_cb2_w ) { pia_set_input_cb2(1, data); }
WRITE8_HANDLER( pia_2_cb2_w ) { pia_set_input_cb2(2, data); }
WRITE8_HANDLER( pia_3_cb2_w ) { pia_set_input_cb2(3, data); }
WRITE8_HANDLER( pia_4_cb2_w ) { pia_set_input_cb2(4, data); }
WRITE8_HANDLER( pia_5_cb2_w ) { pia_set_input_cb2(5, data); }
WRITE8_HANDLER( pia_6_cb2_w ) { pia_set_input_cb2(6, data); }
WRITE8_HANDLER( pia_7_cb2_w ) { pia_set_input_cb2(7, data); }

READ8_HANDLER( pia_0_ca1_r ) { return pia[0].in_ca1; }
READ8_HANDLER( pia_1_ca1_r ) { return pia[1].in_ca1; }
READ8_HANDLER( pia_2_ca1_r ) { return pia[2].in_ca1; }
READ8_HANDLER( pia_3_ca1_r ) { return pia[3].in_ca1; }
READ8_HANDLER( pia_4_ca1_r ) { return pia[4].in_ca1; }
READ8_HANDLER( pia_5_ca1_r ) { return pia[5].in_ca1; }
READ8_HANDLER( pia_6_ca1_r ) { return pia[6].in_ca1; }
READ8_HANDLER( pia_7_ca1_r ) { return pia[7].in_ca1; }
READ8_HANDLER( pia_0_ca2_r ) { return pia[0].in_ca2; }
READ8_HANDLER( pia_1_ca2_r ) { return pia[1].in_ca2; }
READ8_HANDLER( pia_2_ca2_r ) { return pia[2].in_ca2; }
READ8_HANDLER( pia_3_ca2_r ) { return pia[3].in_ca2; }
READ8_HANDLER( pia_4_ca2_r ) { return pia[4].in_ca2; }
READ8_HANDLER( pia_5_ca2_r ) { return pia[5].in_ca2; }
READ8_HANDLER( pia_6_ca2_r ) { return pia[6].in_ca2; }
READ8_HANDLER( pia_7_ca2_r ) { return pia[7].in_ca2; }

READ8_HANDLER( pia_0_cb1_r ) { return pia[0].in_cb1; }
READ8_HANDLER( pia_1_cb1_r ) { return pia[1].in_cb1; }
READ8_HANDLER( pia_2_cb1_r ) { return pia[2].in_cb1; }
READ8_HANDLER( pia_3_cb1_r ) { return pia[3].in_cb1; }
READ8_HANDLER( pia_4_cb1_r ) { return pia[4].in_cb1; }
READ8_HANDLER( pia_5_cb1_r ) { return pia[5].in_cb1; }
READ8_HANDLER( pia_6_cb1_r ) { return pia[6].in_cb1; }
READ8_HANDLER( pia_7_cb1_r ) { return pia[7].in_cb1; }
READ8_HANDLER( pia_0_cb2_r ) { return pia[0].in_cb2; }
READ8_HANDLER( pia_1_cb2_r ) { return pia[1].in_cb2; }
READ8_HANDLER( pia_2_cb2_r ) { return pia[2].in_cb2; }
READ8_HANDLER( pia_3_cb2_r ) { return pia[3].in_cb2; }
READ8_HANDLER( pia_4_cb2_r ) { return pia[4].in_cb2; }
READ8_HANDLER( pia_5_cb2_r ) { return pia[5].in_cb2; }
READ8_HANDLER( pia_6_cb2_r ) { return pia[6].in_cb2; }
READ8_HANDLER( pia_7_cb2_r ) { return pia[7].in_cb2; }
#endif
