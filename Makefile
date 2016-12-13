target=mem_error_inject

all: module

ifndef KERNEL_DIR
KERNEL_DIR = /lib/modules/`uname -r`/build
endif

obj-m := $(target).o

EXTRA_CFLAGS += -DEXPORT_SYMTAB

.PHONY: module
module:
	make -C $(KERNEL_DIR) M=$(PWD) modules

.PHONY: sparse
sparse:
	make C=2 -C $(KERNEL_DIR) M=$(PWD) modules

.PHONY: cocci
cocci:
	make coccicheck MODE=report -C $(KERNEL_DIR) M=$(PWD) 

.PHONY: clean
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
	rm *~ > /dev/null 2>&1

.PHONY: install
install:
	sudo insmod $(target).ko

.PHONY: uninstall
uninstall:
	sudo rmmod $(target).ko

.PHONY: reload
reload:
	sudo rmmod $(target).ko
	sudo insmod $(target).ko

.PHONY: test
test:
	sudo insmod $(target).ko
	sudo rmmod $(target).ko
