.RECIPEPREFIX := >
ifeq ($(strip $(PS3DEV)),)
$(error "Set PS3DEV")
endif
ifeq ($(strip $(PSL1GHT)),)
PSL1GHT := $(PS3DEV)
endif

TITLE     := Xbox360 Pad Tester
APPID     := XPAD00001
CONTENTID := UP0001-$(APPID)_00-0000000000000000
TARGET    := xbox360pad
SFOXML    ?= $(firstword $(wildcard $(PS3DEV)/bin/sfo.xml $(PSL1GHT)/bin/sfo.xml $(PS3DEV)/samples/sfo.xml $(PSL1GHT)/samples/sfo.xml))

CC     := $(PS3DEV)/ppu/bin/ppu-gcc
SPRX   := $(PS3DEV)/bin/sprxlinker
SFO    := $(PS3DEV)/bin/sfo.py
PKG    := $(PS3DEV)/bin/pkg.py
NPDRM  := $(PS3DEV)/bin/make_self_npdrm

CFLAGS := -O2 -Wall -mcpu=cell -I$(PSL1GHT)/ppu/include -I$(PS3DEV)/portlibs/ppu/include
LDFLAGS := -L$(PSL1GHT)/ppu/lib -L$(PS3DEV)/portlibs/ppu/lib
LIBS   := -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

all: $(TARGET).pkg

$(TARGET).elf: main.c
> $(CC) $(CFLAGS) main.c -o $@ $(LDFLAGS) $(LIBS)

$(TARGET).pkg: $(TARGET).elf
> $(SPRX) $(TARGET).elf
> rm -rf pkg && mkdir -p pkg/USRDIR
> $(NPDRM) $(TARGET).elf pkg/USRDIR/EBOOT.BIN $(CONTENTID)
> $(SFO) --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) pkg/PARAM.SFO
> $(PKG) --contentid $(CONTENTID) pkg/ $@

clean:
> rm -rf pkg *.elf *.pkg *.self
