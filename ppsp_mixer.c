// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PP-Speaker driver for Linux
 *
 * Mixer implementation.
 * Copyright (C) 2001-2008  Stas Sergeev
 * Copyright (C) 2022-2022  ariel/KotCzarny
 */

#include <sound/core.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include "ppsp.h"


static int ppsp_toggle1_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = -1;
	uinfo->value.integer.max = 8;
	return 0;
}
static int ppsp_toggle1_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = chip->toggle1;
	return 0;
}
static int ppsp_toggle1_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int toggle1 = ucontrol->value.integer.value[0];
	if(chip->toggle1!=toggle1) {
		changed = 1;
		chip->toggle1 = toggle1;
	}
	return changed;
}

static int ppsp_volume_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 39;
	return 0;
}
static int ppsp_volume_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = chip->volume;
	return 0;
}
static int ppsp_volume_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int vmax = 30;
	int volume = ucontrol->value.integer.value[0];
	if(!allow_vol_boost && volume>vmax) { volume=vmax; }
	if(chip->volume!=volume) {
		changed = 1;
		chip->volume = volume;
#if PPSP_DEBUG
		printk(KERN_DEBUG "PPSP: %s, vol:%d\n", __FUNCTION__, chip->volume);
#endif
	}
	return changed;
}

static int ppsp_enable_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int ppsp_enable_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = chip->enable;
	return 0;
}

static int ppsp_enable_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int enab = ucontrol->value.integer.value[0];
	if (enab != chip->enable) {
		chip->enable = enab;
		changed = 1;
	}
	return changed;
}

#if 0
static int ppsp_toggle1_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = chip->max_treble + 1;
	if (uinfo->value.enumerated.item > chip->max_treble)
		uinfo->value.enumerated.item = chip->max_treble;
	sprintf(uinfo->value.enumerated.name, "%lu",
		(unsigned long)PPSP_CALC_RATE(uinfo->value.enumerated.item));
	return 0;
}

static int ppsp_toggle1_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	ucontrol->value.enumerated.item[0] = chip->toggle1;
	return 0;
}

static int ppsp_toggle1_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int treble = ucontrol->value.enumerated.item[0];
	if (treble != chip->treble) {
		chip->treble = treble;
#if PPSP_DEBUG
		if(debug)
			printk(KERN_INFO "PPSP: rate set to %i\n", PPSP_RATE());
#endif
		changed = 1;
	}
	return changed;
}
#endif

#if 0
static int ppsp_ppspkr_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}

static int ppsp_ppspkr_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = chip->ppspkr;
	return 0;
}

static int ppsp_ppspkr_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ppsp *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	int spkr = ucontrol->value.integer.value[0];
	if (spkr != chip->ppspkr) {
		chip->ppspkr = spkr;
		changed = 1;
	}
	return changed;
}
#endif

#define PPSP_MIXER_CONTROL(ctl_type, ctl_name) \
{ \
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER, \
	.name =		ctl_name, \
	.info =		ppsp_##ctl_type##_info, \
	.get =		ppsp_##ctl_type##_get, \
	.put =		ppsp_##ctl_type##_put, \
}

static struct snd_kcontrol_new snd_ppsp_controls_pcm[] = {
	PPSP_MIXER_CONTROL(enable, "Master Playback Switch"),
	PPSP_MIXER_CONTROL(volume, "Master Playback Volume"),
	PPSP_MIXER_CONTROL(toggle1, "Toggle1 Playback Volume"),
};

static struct snd_kcontrol_new snd_ppsp_controls_spkr[] = {
//	PPSP_MIXER_CONTROL(ppspkr, "Beep Playback Switch"),
};

static int snd_ppsp_ctls_add(struct snd_ppsp *chip,
			     struct snd_kcontrol_new *ctls, int num)
{
	int i, err;
	struct snd_card *card = chip->card;
	for (i = 0; i < num; i++) {
		err = snd_ctl_add(card, snd_ctl_new1(ctls + i, chip));
		if (err < 0)
			return err;
	}
	return 0;
}

int snd_ppsp_new_mixer(struct snd_ppsp *chip, int nopcm)
{
	int err;
	struct snd_card *card = chip->card;

	if (!nopcm) {
		err = snd_ppsp_ctls_add(chip, snd_ppsp_controls_pcm,
			ARRAY_SIZE(snd_ppsp_controls_pcm));
		if (err < 0)
			return err;
	}
	err = snd_ppsp_ctls_add(chip, snd_ppsp_controls_spkr,
		ARRAY_SIZE(snd_ppsp_controls_spkr));
	if (err < 0)
		return err;

	strcpy(card->mixername, "PP-Speaker");

	return 0;
}
