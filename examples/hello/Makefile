# Example app for libwiieth: make (after building the library in the top directory) -> TARGET.dol
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPro/devkitPPC")
endif
include $(DEVKITPPC)/wii_rules

TARGET   := wiieth-$(notdir $(CURDIR))
BUILD    := build
WIIETH   := $(CURDIR)/../..

CFLAGS   := -g -O2 -Wall $(MACHDEP) -I$(WIIETH)/include -I$(LIBOGC_INC)
LDFLAGS  := -g $(MACHDEP) -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS     := -L$(WIIETH)/lib -lwiieth -L$(LIBOGC_LIB) -lwiiuse -lbte -logc -lm

CFILES   := $(wildcard source/*.c)
OFILES   := $(patsubst source/%.c,$(BUILD)/%.o,$(CFILES))

.PHONY: all clean
all: $(TARGET).dol

$(TARGET).dol: $(BUILD)/$(TARGET).elf
	@elf2dol $< $@
	@echo built $(CURDIR)/$@

$(BUILD)/$(TARGET).elf: $(OFILES) $(WIIETH)/lib/libwiieth.a
	@$(CC) $(OFILES) $(LDFLAGS) $(LIBS) -o $@

$(BUILD)/%.o: source/%.c
	@mkdir -p $(BUILD)
	@echo $<
	@$(CC) -MMD -MP $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).dol

-include $(OFILES:.o=.d)
