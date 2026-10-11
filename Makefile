PROGS := smolinput_test

HEADERS += smolinput.h

LIBC_ELFS := $(addsuffix .libc.elf,$(PROGS))

all: $(LIBC_ELFS)

include smolcommon.mk

ifdef SMOL_ARCH
ELFS := $(addsuffix .$(SMOL_SUFFIX).elf,$(PROGS))

all: $(ELFS)
endif

.PHONY: all clean
clean:
	$(Q)rm -f .buildmode.* *.elf *.elf.dbg smolinput_test
