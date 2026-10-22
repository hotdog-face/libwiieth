# libwiieth: builds lib/libwiieth.a (driver + lwIP 2.2.1 + OS layer + netx) with devkitPPC / libogc.
#
#   make                  the library
#   make examples         the library and the example apps (examples/*/*.dol)
#   make install          copy lib + headers into $(DESTDIR) (default: devkitPro's portlibs/wii)
#   make clean
#
# No devkitPro installed? ./build.sh runs this in the devkitpro/devkitppc Docker image.
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPro/devkitPPC")
endif
include $(DEVKITPPC)/wii_rules

BUILD    := build
LIBDIR   := lib
TARGET   := $(LIBDIR)/libwiieth.a
SOURCES  := source lwip/core lwip/core/ipv4 lwip/api lwip/netif
INCLUDES := include source source/port lwip/include
DESTDIR  ?= $(PORTLIBS_PATH)/wii

CFLAGS   := -g -O2 -Wall $(MACHDEP) -D_REENTRANT $(foreach d,$(INCLUDES),-I$(d)) -I$(LIBOGC_INC)

CFILES   := $(foreach d,$(SOURCES),$(wildcard $(d)/*.c))
OFILES   := $(patsubst %.c,$(BUILD)/%.o,$(CFILES))
DEPS     := $(OFILES:.o=.d)

.PHONY: all examples install clean
all: $(TARGET)

$(TARGET): $(OFILES)
	@mkdir -p $(LIBDIR)
	@rm -f $@
	$(AR) -rcs $@ $^
	@echo built $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo $<
	@$(CC) -MMD -MP $(CFLAGS) -c $< -o $@

examples: $(TARGET)
	@for d in examples/*/; do $(MAKE) --no-print-directory -C $$d || exit 1; done

install: $(TARGET)
	mkdir -p $(DESTDIR)/lib $(DESTDIR)/include
	cp $(TARGET) $(DESTDIR)/lib/
	cp include/wiieth.h include/wiieth_asix.h include/wiieth_netx.h $(DESTDIR)/include/

clean:
	rm -rf $(BUILD) $(LIBDIR)
	@for d in examples/*/; do $(MAKE) --no-print-directory -C $$d clean; done

-include $(DEPS)
