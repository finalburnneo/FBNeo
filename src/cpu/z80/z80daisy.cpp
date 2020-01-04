/***************************************************************************

    z80daisy.c

    Z80/180 daisy chaining support functions.

***************************************************************************/

#include "burnint.h"
#include "z80daisy.h"
#include "z80ctc.h"
#include "z80pio.h"

#define CLEAR_LINE	0
#define ASSERT_LINE	1

static z80_irq_daisy_chain *main_chain = NULL;
static z80_irq_daisy_chain *daisy_end = NULL;

INT32 z80daisy_has_ctc = 0;

static void add_dev(int dev)
{
	switch (dev) {
        case Z80_CTC:
            daisy_end->reset 	 = z80ctc_reset;
            daisy_end->irq_state = z80ctc_irq_state;
            daisy_end->irq_ack   = z80ctc_irq_ack;
            daisy_end->irq_reti  = z80ctc_irq_reti;
            daisy_end->dev_exit  = z80ctc_exit;
            daisy_end->dev_scan  = z80ctc_scan;
            daisy_end->param     = 0;

            z80daisy_has_ctc = 1;
            break;

        case Z80_PIO:
            daisy_end->reset 	 = z80pio_reset;
            daisy_end->irq_state = z80pio_irq_state;
            daisy_end->irq_ack   = z80pio_irq_ack;
            daisy_end->irq_reti  = z80pio_irq_reti;
            daisy_end->dev_exit  = z80pio_exit;
            daisy_end->dev_scan  = z80pio_scan;
            daisy_end->param     = 0;
            break;

        default:
            daisy_end->reset 	 = NULL;
            daisy_end->irq_state = NULL;
            daisy_end->irq_ack   = NULL;
            daisy_end->irq_reti  = NULL;
            daisy_end->dev_exit  = NULL;
            daisy_end->dev_scan  = NULL;
            daisy_end->param     = -1;
            break;
    }

    daisy_end++;
}

void z80daisy_init(int dev0, int dev1)
{
    main_chain = (z80_irq_daisy_chain *)BurnMalloc(sizeof(z80_irq_daisy_chain) * 4);
    daisy_end = main_chain;
    memset(main_chain, 0, sizeof(z80_irq_daisy_chain) * 4);

    z80daisy_has_ctc = 0;

    add_dev(dev0);
    add_dev(dev1);
    add_dev(0); // end of list

    Z80SetDaisy(main_chain);
}

void z80daisy_exit()
{
    struct z80_irq_daisy_chain *daisy = main_chain;

    for ( ; daisy->param != -1; daisy++)
		if (daisy->dev_exit)
			(*daisy->dev_exit)();

    BurnFree(main_chain);
    daisy_end = NULL;

    z80daisy_has_ctc = 0;
}

void z80daisy_scan(int nAction)
{
    struct z80_irq_daisy_chain *daisy = main_chain;

    for ( ; daisy->param != -1; daisy++)
		if (daisy->dev_scan)
			(*daisy->dev_scan)(nAction);
}

void z80daisy_reset(const struct z80_irq_daisy_chain *daisy)
{
	/* loop over all devices and call their reset function */
	for ( ; daisy->param != -1; daisy++)
		if (daisy->reset)
			(*daisy->reset)();
}


int z80daisy_update_irq_state(const struct z80_irq_daisy_chain *daisy)
{
	/* loop over all devices; dev[0] is highest priority */
	for ( ; daisy->param != -1; daisy++)
	{
		int state = (*daisy->irq_state)();

		/* if this device is asserting the INT line, that's the one we want */
		if (state & Z80_DAISY_INT)
			return ASSERT_LINE;

		/* if this device is asserting the IEO line, it blocks everyone else */
		if (state & Z80_DAISY_IEO)
			return CLEAR_LINE;
	}

	return CLEAR_LINE;
}


int z80daisy_call_ack_device(const struct z80_irq_daisy_chain *daisy)
{
	/* loop over all devices; dev[0] is the highest priority */
	for ( ; daisy->param != -1; daisy++)
	{
		int state = (*daisy->irq_state)();

		/* if this device is asserting the INT line, that's the one we want */
		if (state & Z80_DAISY_INT)
			return (*daisy->irq_ack)();
	}

//	logerror("z80daisy_call_ack_device: failed to find an device to ack!\n");
	return 0;
}


void z80daisy_call_reti_device(const struct z80_irq_daisy_chain *daisy)
{
	/* loop over all devices; dev[0] is the highest priority */
	for ( ; daisy->param != -1; daisy++)
	{
		int state = (*daisy->irq_state)();

		/* if this device is asserting the IEO line, that's the one we want */
		if (state & Z80_DAISY_IEO)
		{
			(*daisy->irq_reti)();
			return;
		}
	}

//	logerror("z80daisy_call_reti_device: failed to find an device to reti!\n");
}
