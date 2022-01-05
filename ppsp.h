// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PP-Speaker driver for Linux
 *
 * Copyright (C) 1993-1997  Michael Beck
 * Copyright (C) 1997-2001  David Woodhouse
 * Copyright (C) 2001-2008  Stas Sergeev
 * Copyright (C) 2022-2022  ariel/KotCzarny
 */

#ifndef __PPSP_H__
#define __PPSP_H__

#define PPSP_SOUND_VERSION 0x040	/* read 0.4.0 */
#define PPSP_DEBUG 0
#define PPSP_I8253 0

#include <linux/hrtimer.h>
#if PPSP_I8253
#include <linux/i8253.h>
#include <linux/timex.h>
#endif

// #define PIT_TICK_RATE 1193182ul
// #define PIT_TICK_RATE2 48000ul*16ul

/* default timer freq for PC-Speaker: 18643 Hz */
//#define DIV_18KHZ 64
//#define MAX_DIV DIV_18KHZ
//#define CALC_DIV(d) (MAX_DIV >> (d))
//#define CUR_DIV() CALC_DIV(chip->treble)
//#define PPSP_MAX_TREBLE 1

/* unfortunately, with hrtimers 37KHz does not work very well :( */
//#define PPSP_DEFAULT_TREBLE 1
//#define MIN_DIV (MAX_DIV >> PPSP_MAX_TREBLE)

/* wild guess */
#define PPSP_MIN_LPJ 1000000
//#define PPSP_DEFAULT_SDIV (DIV_18KHZ >> 1)
#define PPSP_DEFAULT_SRATE (24000)
//#define PPSP_INDEX_INC() (1 << (PPSP_MAX_TREBLE - chip->treble))
#define PPSP_INDEX_INC() (1 + chip->half_rate)
//#define PPSP_CALC_RATE(i) (PIT_TICK_RATE2 / CALC_DIV(i))
//#define PPSP_RATE() PPSP_CALC_RATE(chip->treble)
#define PPSP_CALC_RATE(i) (i)
#define PPSP_RATE() PPSP_CALC_RATE(chip->srate)
#define PPSP_MIN_RATE__1 8000
#define PPSP_MAX_RATE__1 48000
#define PPSP_MAX_PERIOD_NS (1000000000ULL * PPSP_MIN_RATE__1)
#define PPSP_MIN_PERIOD_NS (1000000000ULL * PPSP_MAX_RATE__1)
#define PPSP_CALC_NS() ({ \
	u64 __val = 1000000000ULL; \
	do_div(__val, (chip->srate >> chip->half_rate)); \
	__val; \
})

#define PPSP_MAX_PERIOD_SIZE	(64*1024)
#define PPSP_MAX_PERIODS	512
#define PPSP_BUFFER_SIZE	(128*1024)

#define PPSP_VOL2MOD() (15 - chip->volume)

struct snd_ppsp {
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct input_dev *input_dev;
	struct hrtimer timer;
	unsigned short port, irq, dma;
	spinlock_t substream_lock;
	struct snd_pcm_substream *playback_substream;
	unsigned int fmt_size;
	unsigned int is_signed;
	unsigned int chans;
	unsigned int srate;
	unsigned int half_rate;
	size_t playback_ptr;
	size_t period_ptr;
	atomic_t timer_active;
	int enable;
	int toggle1;
	int toggle2;
	int ppspkr;
	u64 NS;
	int volume;
	int volume_mod;
	u8 last_val;
};

#if PPSP_DEBUG
extern int debug;
#endif

extern int hr_thr;
extern int allow_vol_boost;

extern struct snd_ppsp ppsp_chip;

extern enum hrtimer_restart ppsp_do_timer(struct hrtimer *handle);
extern void ppsp_sync_stop(struct snd_ppsp *chip);

extern int snd_ppsp_new_pcm(struct snd_ppsp *chip);
extern int snd_ppsp_new_mixer(struct snd_ppsp *chip, int nopcm);

#endif
