PROGS := smolinput_test smolinput_mock

HEADERS += smolinput.h

LIBC_ELFS := $(addsuffix .libc.elf,$(PROGS))

all: $(LIBC_ELFS)

include smolcommon.mk

ifdef SMOL_ARCH
ELFS := $(addsuffix .$(SMOL_SUFFIX).elf,$(PROGS))
TARBALL := smolinput.$(SMOL_SUFFIX).tar

all: $(ELFS)

ifdef TARWAK
all: $(TARBALL)

$(TARBALL): rootfs.tarwak.json $(ELFS)
	$(MSG) TARWAK $@
	$(Q)$(TARWAK) -i $< -o $@ -b ./ -p "%s.$(SMOL_SUFFIX).elf"
endif
endif

.PHONY: all clean
clean:
	$(Q)rm -f .buildmode.* *.elf *.elf.dbg *.tar smolinput_test
