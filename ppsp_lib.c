// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PP-Speaker driver for Linux
 *
 * Copyright (C) 1993-1997  Michael Beck
 * Copyright (C) 1997-2001  David Woodhouse
 * Copyright (C) 2001-2008  Stas Sergeev
 * Copyright (C) 2022-2022  ariel/KotCzarny
 */

#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <sound/pcm.h>
#include "ppsp.h"
#include <linux/version.h>

#define DMIX_WANTS_S16	1

/*
 * Call snd_pcm_period_elapsed in a tasklet
 * This avoids spinlock messes and long-running irq contexts
 */
static void ppsp_call_pcm_elapsed(unsigned long priv)
{
	if (atomic_read(&ppsp_chip.timer_active)) {
		struct snd_pcm_substream *substream;
		substream = ppsp_chip.playback_substream;
		if (substream)
			snd_pcm_period_elapsed(substream);
	}
}

static DECLARE_TASKLET(ppsp_pcm_tasklet, ppsp_call_pcm_elapsed, 0);

//#include <linux/time.h>
#if PPSP_DEBUG
static void gett(char *t) {
	struct timespec tv; getnstimeofday(&tv);
	sprintf(t,"%ld.%09ld", tv.tv_sec, tv.tv_nsec);
}
#endif

#if PPSP_DEBUG
static u32 ppsp_i=0;
#endif

#define PPSP_DUMP 0

#if PPSP_DUMP
static u32 ppsp_i2=0;
struct file * ppsp_dbgfp=NULL;
char ppsp_dbgbuf[128];
loff_t ppsp_dbgoff=0;
#endif

/* write the port and returns the next expire time in ns;
 * called at the trigger-start and in hrtimer callback
 */
static u64 ppsp_timer_update(struct snd_ppsp *chip)
{
	u8 val[4]; int div; u64 pos;
#if PPSP_DUMP
//	s16 vs[16];
	u8 vs[16];
#endif
	struct snd_pcm_substream *substream;
	struct snd_pcm_runtime *runtime;
#if PPSP_DEBUG
	char tt[32],tt2[32];
#endif
#if PPSP_I8253
	unsigned long flags;
#endif

#if PPSP_DUMP
	size_t ppsp_dbglen;
	if(!ppsp_dbgfp) ppsp_dbgfp=filp_open("/tmp/ppsp_dump.txt",
		O_WRONLY | O_CREAT | O_TRUNC, 0600);
#endif

	substream = chip->playback_substream;
	if (!substream)
		return 0;

	local_irq_disable();

	runtime = substream->runtime;
	/* assume it is mono! */
/*
One thing to be noted is that the configured buffer and period sizes are stored in
 "frames" in the runtime. In the ALSA world, 1 frame = channels * samples-size.
 For conversion between frames and bytes, you can use the frames_to_bytes() and
 bytes_to_frames() helper functions.
period_bytes = frames_to_bytes(runtime, runtime->period_size);
Also, many software parameters (sw_params) are stored in frames, too. Please check
 the type of the field. snd_pcm_uframes_t is for the frames as unsigned integer while
 snd_pcm_sframes_t is for the frames as signed integer.
The DMA buffer is defined by the following four fields, dma_area, dma_addr, dma_bytes and
 dma_private. The dma_area holds the buffer pointer (the logical address). You can call
 memcpy() from/to this pointer. Meanwhile, dma_addr holds the physical address of the
 buffer. This field is specified only when the buffer is a linear buffer. dma_bytes holds
 the size of buffer in bytes. dma_private is used for the ALSA DMA allocator.
*/
	pos=chip->playback_ptr + chip->fmt_size - 1 + chip->toggle1;
	div=chip->chans*(chip->half_rate+1);
	val[0] = runtime->dma_area[pos];

	switch(div) {
		case 2:
			val[1] = runtime->dma_area[pos+chip->fmt_size];
			val[0] = ((((val[0]<<24)/div + (val[1]<<24)/div))>>24)&0xff;
			break;
		case 4:
			val[1] = runtime->dma_area[pos+chip->fmt_size];
			val[2] = runtime->dma_area[pos+chip->fmt_size*2];
			val[3] = runtime->dma_area[pos+chip->fmt_size*3];
			val[0] = ((((val[0]<<24)/div + (val[1]<<24)/div
			 + (val[2]<<24)/div + (val[3]<<24)/div))>>24)&0xff;
			break;
	}

	if(chip->volume!=30) {
		val[0] = (((val[0]<<24)/30*chip->volume)>>24)&0xff;
	}

#if PPSP_DUMP
	memcpy(&vs, runtime->dma_area+chip->playback_ptr, sizeof(vs[0])*8);
	ppsp_i2++; if(ppsp_i2>chip->srate/4) { ppsp_i2=0; } if(ppsp_i2<20)
	{
		switch(sizeof(vs[0])) {
			case 1:
				snprintf(ppsp_dbgbuf, 127, "%lld"
				" %02hx %02hx %02hx %02hx,"" %02hx %02hx %02hx %02hx,"
//				" %02hx %02hx %02hx %02hx,"" %02hx %02hx %02hx %02hx,"
				" hr%d s%d %02x %02x (%hhd %hhd)\n", ppsp_dbgoff,
				vs[0], vs[1], vs[2], vs[3],  vs[4], vs[5], vs[6], vs[7],
//				vs[8], vs[9], vs[10], vs[11],  vs[12], vs[13], vs[14], vs[15],
				chip->half_rate, chip->is_signed, val, val ^ 0x80
				, val[0], val[0] ^ 0x80);
				break;
			case 2:
				snprintf(ppsp_dbgbuf, 127, "%lld"
				" %04hx %04hx %04hx %04hx,"" %04hx %04hx %04hx %04hx,"
//				" %04hx %04hx %04hx %04hx,"" %04hx %04hx %04hx %04hx,"
				" hr%d s%d %02x %02x (%hhd %hhd)\n", ppsp_dbgoff,
				vs[0], vs[1], vs[2], vs[3],  vs[4], vs[5], vs[6], vs[7],
//				vs[8], vs[9], vs[10], vs[11],  vs[12], vs[13], vs[14], vs[15],
				chip->half_rate, chip->is_signed, val, val ^ 0x80
				, val[0], val[0] ^ 0x80);
				break;
		}
		ppsp_dbgbuf[127]=0;
		ppsp_dbglen=strlen(ppsp_dbgbuf);
		kernel_write(ppsp_dbgfp, ppsp_dbgbuf, ppsp_dbglen, &ppsp_dbgoff);
	}
#endif

	if (chip->is_signed)
	{
		val[0] ^= 0x80;
	}

	if (chip->enable) {
#if PPSP_DEBUG
		ppsp_i++;
		if(0&&debug)
		if((ppsp_i % chip->srate) == 0)
			printk(KERN_INFO "PPSP: val=0x%08x i=%d\n", val, ppsp_i);
#endif
#if PPSP_I8253
		raw_spin_lock_irqsave(&i8253_lock, flags);
#endif
#if PPSP_DEBUG
		if(debug>=2 && (ppsp_i % chip->srate) == 0) {
			gett(tt);
			outb(val[0], chip->port);
			gett(tt2);
			printk(KERN_INFO "%s\n%s\n",tt,tt2);
		} else
#endif
			outb(val[0], chip->port);

#if PPSP_I8253
		raw_spin_unlock_irqrestore(&i8253_lock, flags);
#endif
		chip->last_val=val[0];
	}

	local_irq_enable();

#if PPSP_DEBUG
	if(debug && (ppsp_i % chip->srate) == 0) {
		gett(tt);
                printk(KERN_INFO "PPSP: %s i=%d val=0x%04x srate=%d hr=%d ns=%lld\n",
		tt, ppsp_i, val, chip->srate, chip->half_rate, chip->NS);
		printk(KERN_INFO "PPSP: val=%08x sig=%d ch=%d fmt=%08x U8=%llx S16LE=%llx\n",
		val, chip->is_signed,
		runtime->channels, runtime->format, SNDRV_PCM_FMTBIT_U8, SNDRV_PCM_FMTBIT_S16_LE
		);
	}
#endif
	return chip->NS;
}

#if PPSP_DEBUG
static int ti=0;
#endif
static void ppsp_pointer_update(struct snd_ppsp *chip)
{
	struct snd_pcm_substream *substream;
	size_t period_bytes, buffer_bytes;
	int periods_elapsed;
	unsigned long flags;

	/* update the playback position */
	substream = chip->playback_substream;
	if (!substream)
		return;
/*
 printk(KERN_INFO "PPSP: buffer_bytes mod period_bytes != 0 ? (%zi %zi %zi)\n",
  chip->playback_ptr, period_bytes, buffer_bytes);
*/
	period_bytes = snd_pcm_lib_period_bytes(substream);
	buffer_bytes = snd_pcm_lib_buffer_bytes(substream);

	spin_lock_irqsave(&chip->substream_lock, flags);
	chip->playback_ptr += PPSP_INDEX_INC() * chip->fmt_size * chip->chans;
	periods_elapsed = chip->playback_ptr - chip->period_ptr;
	if (periods_elapsed < 0) {
#if PPSP_DEBUG
		ti++;
		if(debug && (ti%1000)==1)
		printk(KERN_INFO "PPSP: pb_ptr(%zi) pe_ptr(%zi)"
			", buffer_b(%zi) %% period_b(%zi) = %zi != 0 ?\n",
			chip->playback_ptr, chip->period_ptr,
			buffer_bytes, period_bytes, buffer_bytes % period_bytes
		);
#endif
		periods_elapsed += buffer_bytes;
	}
	periods_elapsed /= period_bytes;
	/* wrap the pointer _before_ calling snd_pcm_period_elapsed(),
	 * or ALSA will BUG on us. */
	chip->playback_ptr %= buffer_bytes;

	if (periods_elapsed) {
		chip->period_ptr += periods_elapsed * period_bytes;
		chip->period_ptr %= buffer_bytes;
	}
	spin_unlock_irqrestore(&chip->substream_lock, flags);

	if (periods_elapsed)
		tasklet_schedule(&ppsp_pcm_tasklet);
}

enum hrtimer_restart ppsp_do_timer(struct hrtimer *handle)
{
	struct snd_ppsp *chip = container_of(handle, struct snd_ppsp, timer);
	int pointer_update;
	u64 ns;

	if (!atomic_read(&chip->timer_active) || !chip->playback_substream)
		return HRTIMER_NORESTART;

	pointer_update = 1;
	ns = ppsp_timer_update(chip);
	if (!ns) {
		printk(KERN_WARNING "PPSP: unexpected stop\n");
		return HRTIMER_NORESTART;
	}

	if (pointer_update)
		ppsp_pointer_update(chip);

	hrtimer_forward(handle, hrtimer_get_expires(handle), ns_to_ktime(ns));

	return HRTIMER_RESTART;
}

static int ppsp_start_playing(struct snd_ppsp *chip)
{
	int i=chip->last_val, j=(i<0x80?1:-1);

#if PPSP_DEBUG
	if(debug)
		printk(KERN_INFO "PPSP: %s called\n", __FUNCTION__);
#endif

//printk(KERN_DEBUG "PPSP: start, i=%d j=%d\n", i, j);
	while(i!=0x80) {
		i+=j;
		outb_p(i, chip->port);
//printk(KERN_DEBUG "PPSP: start, i=%d j=%d\n", i, j);
	}
	chip->last_val=i;

	if (atomic_read(&chip->timer_active)) {
		printk(KERN_ERR "PPSP: Timer already active\n");
		return -EIO;
	}

#if PPSP_I8253
	raw_spin_lock(&i8253_lock);
	chip->val61 = inb(0x61) | 0x03;
	outb_p(0x92, 0x43);	/* binary, mode 1, LSB only, ch 2 */
	raw_spin_unlock(&i8253_lock);
#endif
	atomic_set(&chip->timer_active, 1);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
	hrtimer_start(&ppsp_chip.timer, ktime_set(0, 0), HRTIMER_MODE_REL);
#else
	hrtimer_start(&ppsp_chip.timer, 0, HRTIMER_MODE_REL);
#endif
	return 0;
}

static void ppsp_stop_playing(struct snd_ppsp *chip)
{
        int i=chip->last_val, j=(i>0?-1:1);

#if PPSP_DEBUG
	if(debug)
		printk(KERN_INFO "PPSP: %s called\n", __FUNCTION__);
#endif
#if PPSP_DUMP
	if(ppsp_dbgfp) {
		filp_close(ppsp_dbgfp,0);
		ppsp_dbgfp=NULL;
		ppsp_dbgoff=0;
	}
#endif
	if (!atomic_read(&chip->timer_active))
		return;

	atomic_set(&chip->timer_active, 0);
#if 0
	outb(0x00, chip->port);
#else
//printk(KERN_DEBUG "PPSP: stop, i=%d j=%d\n", i, j);
	while(i!=0) {
		i+=j;
		outb_p(j, chip->port);
//printk(KERN_DEBUG "PPSP: stop, i=%d j=%d\n", i, j);
	}
	chip->last_val=i;
#endif

#if PPSP_I8253
	raw_spin_lock(&i8253_lock);
	/* restore the timer */
	outb_p(0xb6, 0x43);	/* binary, mode 3, LSB/MSB, ch 2 */
	outb(chip->val61 & 0xFC, 0x61);
	raw_spin_unlock(&i8253_lock);
#endif
}

/*
 * Force to stop and sync the stream
 */
void ppsp_sync_stop(struct snd_ppsp *chip)
{
	local_irq_disable();
	ppsp_stop_playing(chip);
	local_irq_enable();
	hrtimer_cancel(&chip->timer);
	tasklet_kill(&ppsp_pcm_tasklet);
}

static int snd_ppsp_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_ppsp *chip = snd_pcm_substream_chip(substream);
#if PPSP_DEBUG
	if(debug)
		printk(KERN_INFO "PPSP: %s called\n", __FUNCTION__);
#endif
	ppsp_sync_stop(chip);
	chip->playback_substream = NULL;
	return 0;
}

/*
This is called when the hardware parameter (hw_params) is set up by the application,
 that is, once when the buffer size, the period size, the format, etc. are defined for
 the pcm substream.
*/
static int snd_ppsp_playback_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	struct snd_ppsp *chip = snd_pcm_substream_chip(substream);
	int err;
#if PPSP_DEBUG
	int i;
#endif
	ppsp_sync_stop(chip);
	err = snd_pcm_lib_malloc_pages(substream,
				      params_buffer_bytes(hw_params));
#if PPSP_DEBUG
	i=params_rate(hw_params);
	if(debug)
		printk(KERN_INFO "PPSP: params_rate=%d\n", i);
#endif
	if (err < 0)
		return err;
	return 0;
}

static int snd_ppsp_playback_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_ppsp *chip = snd_pcm_substream_chip(substream);
#if PPSP_DEBUG
	if(debug)
		printk(KERN_INFO "PPSP: %s called\n", __FUNCTION__);
#endif
	ppsp_sync_stop(chip);
	return snd_pcm_lib_free_pages(substream);
}

/*
This callback is called when the pcm is "prepared". You can set the format type,
 sample rate, etc. here. The difference from hw_params is that the prepare
 callback will be called each time snd_pcm_prepare() is called, i.e. when
 recovering after underruns, etc.
Note that this callback is now non-atomic. You can use schedule-related functions
 safely in this callback.
*/
static int snd_ppsp_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_ppsp *chip = snd_pcm_substream_chip(substream);
	ppsp_sync_stop(chip);
	chip->playback_ptr = 0;
	chip->period_ptr = 0;
	chip->fmt_size =
		snd_pcm_format_physical_width(substream->runtime->format) >> 3;
	chip->is_signed = snd_pcm_format_signed(substream->runtime->format);
	chip->chans = substream->runtime->channels;
	chip->srate = substream->runtime->rate;
	chip->half_rate=(chip->srate > hr_thr ? 1 : 0);
	chip->NS=PPSP_CALC_NS();
// #if PPSP_DEBUG
//	if(debug)
	{
		printk(KERN_DEBUG "PPSP: %dHz/%d %dch sig=%d fmtsiz=%i ns=%lld"
			" bsize=%zi psize=%zi f=%zi periods=%i\n",
			chip->srate, (chip->half_rate+1), chip->chans, chip->is_signed,
			chip->fmt_size, chip->NS,
			snd_pcm_lib_buffer_bytes(substream),
			snd_pcm_lib_period_bytes(substream),
			snd_pcm_lib_buffer_bytes(substream) / snd_pcm_lib_period_bytes(substream),
			substream->runtime->periods);
	}
// #endif
	while(chip->last_val>0) {
		outb_p(chip->last_val, chip->port);
		chip->last_val>>=1;
	}
	outb_p(0, chip->port);
	return 0;
}

/*
This is called when the pcm is started, stopped or paused.
*/
static int snd_ppsp_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_ppsp *chip = snd_pcm_substream_chip(substream);
#if PPSP_DEBUG
	if(debug)
		printk(KERN_INFO "PPSP: %s called\n", __FUNCTION__);
#endif
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return ppsp_start_playing(chip);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ppsp_stop_playing(chip);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static snd_pcm_uframes_t snd_ppsp_playback_pointer(struct snd_pcm_substream
						   *substream)
{
	struct snd_ppsp *chip = snd_pcm_substream_chip(substream);
	unsigned int pos;
	spin_lock(&chip->substream_lock);
	pos = chip->playback_ptr;
	spin_unlock(&chip->substream_lock);
	return bytes_to_frames(substream->runtime, pos);
}

static const struct snd_pcm_hardware snd_ppsp_playback = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_HALF_DUPLEX |
		 SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_U8
#if DMIX_WANTS_S16
		    | SNDRV_PCM_FMTBIT_S16_LE
#endif
	    ),
	.rates =
//		SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000
		| SNDRV_PCM_RATE_22050
		| SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000
	,
	.rate_min = PPSP_MIN_RATE__1,
	.rate_max = PPSP_MAX_RATE__1,
	.channels_min = 1,
	.channels_max = 2,
	.buffer_bytes_max = PPSP_BUFFER_SIZE,
	.period_bytes_min = 2,
	.period_bytes_max = PPSP_MAX_PERIOD_SIZE,
	.periods_min = 2,
	.periods_max = PPSP_MAX_PERIODS,
	.fifo_size = 0,
};

static int snd_ppsp_playback_open(struct snd_pcm_substream *substream)
{
	struct snd_ppsp *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
#if PPSP_DEBUG
	if(debug)
		printk(KERN_INFO "PPSP: %s called\n", __FUNCTION__);
#endif
	if (atomic_read(&chip->timer_active)) {
		printk(KERN_ERR "PPSP: still active!!\n");
		return -EBUSY;
	}
	runtime->hw = snd_ppsp_playback;
	chip->playback_substream = substream;
	return 0;
}

static const struct snd_pcm_ops snd_ppsp_playback_ops = {
	.open = snd_ppsp_playback_open,
	.close = snd_ppsp_playback_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_ppsp_playback_hw_params,
	.hw_free = snd_ppsp_playback_hw_free,
	.prepare = snd_ppsp_playback_prepare,
	.trigger = snd_ppsp_trigger,
	.pointer = snd_ppsp_playback_pointer,
};

int snd_ppsp_new_pcm(struct snd_ppsp *chip)
{
	int err;

	err = snd_pcm_new(chip->card, "ppspeaker", 0, 1, 0, &chip->pcm);
	if (err < 0)
		return err;

	snd_pcm_set_ops(chip->pcm, SNDRV_PCM_STREAM_PLAYBACK,
			&snd_ppsp_playback_ops);

	chip->pcm->private_data = chip;
	chip->pcm->info_flags = SNDRV_PCM_INFO_HALF_DUPLEX;
	strcpy(chip->pcm->name, "ppsp");

	snd_pcm_lib_preallocate_pages_for_all(chip->pcm,
					      SNDRV_DMA_TYPE_CONTINUOUS,
					      snd_dma_continuous_data
					      (GFP_KERNEL), PPSP_BUFFER_SIZE,
					      PPSP_BUFFER_SIZE);

	return 0;
}
