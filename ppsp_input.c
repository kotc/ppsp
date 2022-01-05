// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  PP-Speaker beeper driver for Linux
 *
 *  Copyright (c) 1992 Orest Zborowski
 *  Copyright (c) 2002 Vojtech Pavlik
 *  Copyright (C) 2022-2022  ariel/KotCzarny
 */


#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include "ppsp.h"
#include "ppsp_input.h"

static void ppspkr_do_sound(unsigned int count)
{
#if PPSP_I8253
	unsigned long flags;
	raw_spin_lock_irqsave(&i8253_lock, flags);
// #if 0
	if (count) {
		/* set command for counter 2, 2 byte write */
//		outb_p(0xB6, 0x43);
		/* select desired HZ */
//		outb_p(count & 0xff, 0x42);
//		outb((count >> 8) & 0xff, 0x42);
		/* enable counter 2 */
//		outb_p(inb_p(0x61) | 3, 0x61);
	} else {
		/* disable counter 2 */
//		outb(inb_p(0x61) & 0xFC, 0x61);
	}
// #endif

	raw_spin_unlock_irqrestore(&i8253_lock, flags);
#endif
}

void ppspkr_stop_sound(void)
{
	ppspkr_do_sound(0);
}

static int ppspkr_input_event(struct input_dev *dev, unsigned int type,
			      unsigned int code, int value)
{
	unsigned int count = 0;

	if (atomic_read(&ppsp_chip.timer_active) || !ppsp_chip.ppspkr)
		return 0;

	switch (type) {
	case EV_SND:
		switch (code) {
		case SND_BELL:
			if (value)
				value = 1000;
		case SND_TONE:
			break;
		default:
			return -1;
		}
		break;

	default:
		return -1;
	}

//	if (value > 20 && value < 32767)
//		count = PIT_TICK_RATE2 / value;

	ppspkr_do_sound(count);

	return 0;
}

int ppspkr_input_init(struct input_dev **rdev, struct device *dev)
{
	int err;

	struct input_dev *input_dev = input_allocate_device();
	if (!input_dev)
		return -ENOMEM;

	input_dev->name = "PC Speaker";
	input_dev->phys = "isa0061/input0";
	input_dev->id.bustype = BUS_ISA;
	input_dev->id.vendor = 0x001f;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	input_dev->dev.parent = dev;

	input_dev->evbit[0] = BIT(EV_SND);
	input_dev->sndbit[0] = BIT(SND_BELL) | BIT(SND_TONE);
	input_dev->event = ppspkr_input_event;

	err = input_register_device(input_dev);
	if (err) {
		input_free_device(input_dev);
		return err;
	}

	*rdev = input_dev;
	return 0;
}

int ppspkr_input_remove(struct input_dev *dev)
{
	ppspkr_stop_sound();
	input_unregister_device(dev);	/* this also does kfree() */

	return 0;
}
