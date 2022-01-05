
#KBUILD_CFLAGS += -I$(src)
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build


ppsp-objs = ppsp.o ppsp_lib.o ppsp_mixer.o ppsp_input.o

obj-m += snd-ppsp.o


KERNEL_MAKE_OPTS := -C $(KERNEL_DIR) M=$(CURDIR)
ifneq ($(ARCH),)
KERNEL_MAKE_OPTS += ARCH=$(ARCH)
endif
ifneq ($(CROSS_COMPILE),)
KERNEL_MAKE_OPTS += CROSS_COMPILE=$(CROSS_COMPILE)
endif


module:
	@$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

all:	module
