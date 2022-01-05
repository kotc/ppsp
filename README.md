// SPDX-License-Identifier: GPL-2.0-or-later

PP-Speaker driver for Linux

Copyright (C) 2022-2022  ariel/KotCzarny (tjosko@yahoo.com)


Small FAQ:

Q: What is it?
A: Alsa driver for parallel port connected audio dongle, commonly called covox.
   Code started from PC-Speaker driver and was improved for usability.

Q: What works?
A: Most things you would expect out of audio driver. Plays U8/S16,
   mono/stereo, 8kHz-48kHz streams. There is also softvol mixer implemented.

Q: How to use it?
A: - adjust kernel headers/config location
   - run make to produce snd-ppsp.ko, At the moment it expects alsa already
   enabled in kernel .config:
   (PCSPKR_PLATFORM, X86, HIGH_RES_TIMERS, INPUT, SND_PCM)
   - insmod the module (adjusting i/o port if it
   is not 0x378), you will have new audio device in the system
   - confirm with lsmod, dmesg and aplay -l
   - you can use it just like any other sound card

Q: What still needs to be done?
A: Improve audio quality, remove some known bugs, test on more configurations.
   Currently it was only tested via native and VirtualBox running on Dell E6440 with
   PR02X dock and some primitive covox clone.


Known bugs:
- bug related to alsa period buffer
- sometimes audio plays garbled, pausing/restarting stream few times helps
- softvol and downmixer are crude and unoptimized


Params:
- pp_port: Port number of the parallel port (default: 0x378). (int)
- hr_thr: Enable half-rate mode above this freq, for slow machines (default: 24000). (int)
- index: Index value for ppsp soundcard. (int)
- id: ID string for ppsp soundcard. (charp)
