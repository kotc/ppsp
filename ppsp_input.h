// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PP-Speaker driver for Linux
 *
 * Copyright (C) 2001-2008  Stas Sergeev
 * Copyright (C) 2022-2022  ariel/KotCzarny
 */

#ifndef __PPSP_INPUT_H__
#define __PPSP_INPUT_H__

int ppspkr_input_init(struct input_dev **rdev, struct device *dev);
int ppspkr_input_remove(struct input_dev *dev);
void ppspkr_stop_sound(void);

#endif
