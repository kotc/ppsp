// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PP-Speaker driver for Linux
 *
 * Copyright (C) 1997-2001  David Woodhouse
 * Copyright (C) 2001-2008  Stas Sergeev
 * Copyright (C) 2022-2022  ariel/KotCzarny
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/mm.h>
#include "ppsp_input.h"
#include "ppsp.h"
#include <linux/version.h>

MODULE_AUTHOR("ariel/KotCzarny <tjosko@yahoo.com>");
MODULE_AUTHOR("Stas Sergeev <stsp@users.sourceforge.net>");
MODULE_DESCRIPTION("PP-Speaker driver");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{PC-Speaker, ppsp}}");
MODULE_ALIAS("platform:pcspkr");

// https://www.kernel.org/doc/html/v5.4/sound/index.html
// https://wiki.osdev.org/Programmable_Interval_Timer

static int index = SNDRV_DEFAULT_IDX1;	/* Index 0-MAX */
static char *id = SNDRV_DEFAULT_STR1;	/* ID for this card */
static bool enable = SNDRV_DEFAULT_ENABLE1;	/* Enable this card */
static bool nopcm;	/* Disable PCM capability of the driver */
static int pp_port = 0x378;
int hr_thr = 24000;
int allow_vol_boost = 0;

#if PPSP_DEBUG
static int debug = PPSP_DEBUG;
#endif

#if PPSP_DEBUG
module_param(debug, int, 0444);
MODULE_PARM_DESC(debug, "Debugging messages.");
#endif
module_param(pp_port, int, 0444);
MODULE_PARM_DESC(pp_port, "Port number of the parallel port. (default: 0x378)");
//unused. module_param(pp_irq, int, 0444);
//MODULE_PARM_DESC(pp_irq, "IRQ number of the parallel port. (default: 0x7)");
module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for ppsp soundcard.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for ppsp soundcard.");
module_param(hr_thr, int, 0444);
MODULE_PARM_DESC(hr_thr, "Enable half-rate mode above this freq, for slow machines. (default: 24000)");
module_param(allow_vol_boost, int, 0444);
MODULE_PARM_DESC(allow_vol_boost, "Allow volume over 100%. (default: 0)");
#if 0
module_param(enable, bool, 0444);
MODULE_PARM_DESC(enable, "Enable PC-Speaker sound.");
module_param(nopcm, bool, 0444);
MODULE_PARM_DESC(nopcm, "Disable PC-Speaker PCM sound. Only beeps remain.");
#endif

struct snd_ppsp ppsp_chip;

static int snd_ppsp_create(struct snd_card *card)
{
	static struct snd_device_ops ops = { };
	unsigned int resolution;
	int err;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
	struct timespec tp;
	hrtimer_get_res(CLOCK_MONOTONIC, &tp);
	resolution = tp.tv_sec || tp.tv_nsec;
#else
	resolution = hrtimer_resolution;
#endif
	if (!nopcm) {
		if (resolution > PPSP_MAX_PERIOD_NS) {
			printk(KERN_ERR "PPSP: Timer resolution is not sufficient "
				"(%unS)\n", resolution);
			printk(KERN_ERR "PPSP: Make sure you have HPET and ACPI "
				"enabled.\n");
			printk(KERN_ERR "PPSP: Turned into nopcm mode.\n");
			nopcm = 1;
		}
	}

//	if (loops_per_jiffy >= PPSP_MIN_LPJ && resolution <= PPSP_MIN_PERIOD_NS)
//		min_div = MIN_DIV;
//	else
//		min_div = MAX_DIV;
#if PPSP_DEBUG
	if(debug)
		printk(KERN_DEBUG "PPSP: lpj=%li, res=%u\n",
			loops_per_jiffy, resolution);
#endif

	ppsp_chip.toggle1 = 0;
	ppsp_chip.toggle2 = 0;
	ppsp_chip.playback_ptr = 0;
	ppsp_chip.period_ptr = 0;
	atomic_set(&ppsp_chip.timer_active, 0);
	ppsp_chip.enable = 1;
	ppsp_chip.ppspkr = 0;

	spin_lock_init(&ppsp_chip.substream_lock);

	ppsp_chip.card = card;
	ppsp_chip.port = pp_port;
	ppsp_chip.irq = -1;
	ppsp_chip.dma = -1;

	ppsp_chip.srate = PPSP_DEFAULT_SRATE;
	ppsp_chip.half_rate = 0;

	/* Register device */
	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, &ppsp_chip, &ops);
	if (err < 0)
		return err;

	return 0;
}

static int snd_card_ppsp_probe(int devnum, struct device *dev)
{
	struct snd_card *card;
	int err;

	if (devnum != 0)
		return -EINVAL;

	hrtimer_init(&ppsp_chip.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ppsp_chip.timer.function = ppsp_do_timer;

	err = snd_card_new(dev, index, id, THIS_MODULE, 0, &card);
	if (err < 0)
		return err;

	err = snd_ppsp_create(card);
	if (err < 0)
		goto free_card;

	if (!nopcm) {
		err = snd_ppsp_new_pcm(&ppsp_chip);
		if (err < 0)
			goto free_card;
	}
	err = snd_ppsp_new_mixer(&ppsp_chip, nopcm);
	if (err < 0)
		goto free_card;

	strcpy(card->driver, "PP-Speaker");
	strcpy(card->shortname, "ppsp");
	sprintf(card->longname, "%s (%s) at port 0x%x",
		card->driver, card->shortname, ppsp_chip.port);

	err = snd_card_register(card);
	if (err < 0)
		goto free_card;

	return 0;

free_card:
	snd_card_free(card);
	return err;
}

static int alsa_card_ppsp_init(struct device *dev)
{
	int err;

	err = snd_card_ppsp_probe(0, dev);
	if (err) {
		printk(KERN_ERR "PP-Speaker initialization failed.\n");
		return err;
	}

	/* Well, CONFIG_DEBUG_PAGEALLOC makes the sound horrible. Lets alert */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
#ifdef CONFIG_DEBUG_PAGEALLOC
	printk(KERN_WARNING "PPSP: CONFIG_DEBUG_PAGEALLOC is enabled, "
	       "which may make the sound noisy.\n");
#endif
#else
	if (debug_pagealloc_enabled())
	{
		printk(KERN_WARNING "PPSP: CONFIG_DEBUG_PAGEALLOC is enabled, "
		       "which may make the sound noisy.\n");
	}
#endif

	return 0;
}

static void alsa_card_ppsp_exit(struct snd_ppsp *chip)
{
	snd_card_free(chip->card);
}

static int ppsp_probe(struct platform_device *dev)
{
	int err;

	err = ppspkr_input_init(&ppsp_chip.input_dev, &dev->dev);
	if (err < 0)
		return err;

	err = alsa_card_ppsp_init(&dev->dev);
	if (err < 0) {
		ppspkr_input_remove(ppsp_chip.input_dev);
		return err;
	}

	platform_set_drvdata(dev, &ppsp_chip);
	return 0;
}

static int ppsp_remove(struct platform_device *dev)
{
	struct snd_ppsp *chip = platform_get_drvdata(dev);
	ppspkr_input_remove(chip->input_dev);
	alsa_card_ppsp_exit(chip);
	return 0;
}

static void ppsp_stop_beep(struct snd_ppsp *chip)
{
	ppsp_sync_stop(chip);
	ppspkr_stop_sound();
}

#ifdef CONFIG_PM_SLEEP
static int ppsp_suspend(struct device *dev)
{
	struct snd_ppsp *chip = dev_get_drvdata(dev);
	ppsp_stop_beep(chip);
	return 0;
}

static SIMPLE_DEV_PM_OPS(ppsp_pm, ppsp_suspend, NULL);
#define PPSP_PM_OPS	&ppsp_pm
#else
#define PPSP_PM_OPS	NULL
#endif	/* CONFIG_PM_SLEEP */

static void ppsp_shutdown(struct platform_device *dev)
{
	struct snd_ppsp *chip = platform_get_drvdata(dev);
	ppsp_stop_beep(chip);
}

static struct platform_driver ppsp_platform_driver = {
	.driver		= {
		.name	= "pcspkr",
		.pm	= PPSP_PM_OPS,
	},
	.probe		= ppsp_probe,
	.remove		= ppsp_remove,
	.shutdown	= ppsp_shutdown,
};

static int __init ppsp_init(void)
{
	if (!enable)
		return -ENODEV;
	return platform_driver_register(&ppsp_platform_driver);
}

static void __exit ppsp_exit(void)
{
	platform_driver_unregister(&ppsp_platform_driver);
}

module_init(ppsp_init);
module_exit(ppsp_exit);
