.DEFAULT_GOAL := all
ifeq ($(strip $(DEVKITPRO)),)
$(error Install devkitPro and run from its MSYS2 shell; DEVKITPRO is unset)
endif
include $(DEVKITPRO)/libnx/switch_rules
TARGET := AI1Emu
APP_TITLE := AI1 Emu
APP_AUTHOR := AI1 Emu
APP_VERSION := 0.5.16
PORTLIBS := $(DEVKITPRO)/portlibs/switch
OUT := build/switch
SOURCES := main library_ui ui_kit app_data game_session online_cheats game_database platform_library legacy_migration online_updater network cover_fetch sysclk_profile
OBJECTS := $(addprefix $(OUT)/,$(addsuffix .o,$(SOURCES)))
CXXFLAGS := -D__SWITCH__ -O2 -std=gnu++17 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE -ffunction-sections -MMD -MP -I$(LIBNX)/include -I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2 -Ivendor/mgba/src/platform/libretro
LDFLAGS := -specs=$(LIBNX)/switch.specs -march=armv8-a -mtp=soft -fPIE -Wl,--gc-sections,-z,nopack-relative-relocs -L$(LIBNX)/lib -L$(PORTLIBS)/lib
.PHONY: all clean
all: $(TARGET).nro
$(OUT):
	mkdir -p $@
$(OUT)/%.o: source/%.cpp | $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@# Native Windows GCC writes drive paths (E:/...) into dependency files, which make
	@# misreads, so header edits stopped rebuilding objects. Rewrite them as /e/...
	@sed -i -E 's#(^|[ \t])([A-Za-z]):/#\1/\L\2/#g' $(@:.o=.d)
build/mgba/mgba_libretro.a:
	@echo "Missing mGBA core: run bash scripts/build-switch.sh."
	@false
$(OUT)/$(TARGET).elf: $(OBJECTS) build/mgba/mgba_libretro.a
	$(CXX) $(LDFLAGS) $^ -lcurl -lz -lSDL2 -lEGL -lglapi -ldrm_nouveau -lnx -lpthread -lm -o $@
$(OUT)/$(TARGET).nacp: Makefile | $(OUT)
	nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $@
$(TARGET).nro: $(OUT)/$(TARGET).elf $(OUT)/$(TARGET).nacp $(wildcard romfs/*)
	elf2nro $< $@ --nacp=$(OUT)/$(TARGET).nacp --icon=romfs/app-icon.jpg --romfsdir=romfs
clean:
	rm -f $(OUT)/*.o $(OUT)/*.d $(OUT)/$(TARGET).elf $(OUT)/$(TARGET).nacp $(TARGET).nro
-include $(OBJECTS:.o=.d)

# Each emulator is a separate Switch executable; no incompatible core licenses
# or duplicate Libretro entry points are combined in a single binary.
# Shared v0.5 in-game menu, linked into every emulator NRO.
MENU_OBJECTS := $(OUT)/ui_kit.o $(OUT)/app_data.o $(OUT)/game_session.o $(OUT)/online_cheats.o $(OUT)/platform_library.o $(OUT)/network.o
CORE_OBJECTS := $(OUT)/core_runner.o $(MENU_OBJECTS)
# Emulator NROs carry only the menu fonts in their RomFS.
CORE_ROMFS := $(OUT)/core-romfs
CORE_FONTS := Rounded-Medium.ttf Rounded-Bold.ttf Rounded-ExtraBold.ttf Rubik-Medium.ttf Rubik-Bold.ttf Rubik-ExtraBold.ttf Nunito-SemiBold.ttf Nunito-ExtraBold.ttf Nunito-Black.ttf GeistMono-Medium.ttf MPLUSRounded1c-OFL.txt Rubik-OFL.txt Nunito-OFL.txt Geist-OFL.txt
$(CORE_ROMFS)/.stamp: $(addprefix romfs/,$(CORE_FONTS))
	mkdir -p $(CORE_ROMFS)
	cp $^ $(CORE_ROMFS)/
	touch $@
CORE_LIBS := -lcurl -lSDL2 -lEGL -lglapi -ldrm_nouveau -lz -lnx -lpthread -lm
$(OUT)/cores:
	mkdir -p $@
$(OUT)/cores/melonds.elf: $(OUT)/ds_runner.o $(MENU_OBJECTS) vendor/melonds-libretro/melonds_libretro_libnx.a | $(OUT)/cores
	$(CXX) $(LDFLAGS) $^ $(CORE_LIBS) -o $@
$(OUT)/cores/fceumm.elf: $(CORE_OBJECTS) vendor/fceumm/fceumm_libretro_libnx.a | $(OUT)/cores
	$(CXX) $(LDFLAGS) $^ $(CORE_LIBS) -o $@
$(OUT)/cores/snes9x.elf: $(CORE_OBJECTS) vendor/snes9x/libretro/snes9x_libretro_libnx.a | $(OUT)/cores
	$(CXX) $(LDFLAGS) $^ $(CORE_LIBS) -o $@
# Genesis carries an older embedded zlib. HTTPS uses devkitPro zlib; link one
# implementation rather than mixing internals or allowing duplicate symbols.
$(OUT)/genesis-core.a: vendor/genesis-plus-gx/genesis_plus_gx_libretro_libnx.a Makefile | $(OUT)
	cp $< $@
	$(AR) d $@ adler32.o crc32.o inflate.o inftrees.o zutil.o
$(OUT)/cores/genesis-plus-gx.elf: $(CORE_OBJECTS) $(OUT)/genesis-core.a | $(OUT)/cores
	$(CXX) $(LDFLAGS) $^ $(CORE_LIBS) -o $@
$(OUT)/cores/%.nacp: Makefile | $(OUT)/cores
	nacptool --create "AI1 Emu - $*" "AI1 Emu" "$(APP_VERSION)" $@
CORE_NROS := $(addprefix $(OUT)/cores/,melonds.nro fceumm.nro snes9x.nro genesis-plus-gx.nro)
$(CORE_NROS): $(OUT)/cores/%.nro: $(OUT)/cores/%.elf $(OUT)/cores/%.nacp Makefile romfs/app-icon.jpg $(CORE_ROMFS)/.stamp
	elf2nro $< $@ --nacp=$(OUT)/cores/$*.nacp --icon=romfs/app-icon.jpg --romfsdir=$(CORE_ROMFS)
.PHONY: emulator-cores
emulator-cores: $(CORE_NROS)

-include $(CORE_OBJECTS:.o=.d) $(MENU_OBJECTS:.o=.d)



$(OUT)/cores/mupen64plus-next.elf: $(OUT)/n64_runner.o $(OUT)/n64_video.o $(MENU_OBJECTS) vendor/mupen64plus-next/mupen64plus_next_libretro_libnx.a | $(OUT)/cores
	$(CXX) $(LDFLAGS) $^ $(CORE_LIBS) -o $@
$(OUT)/cores/mupen64plus-next.nro: $(OUT)/cores/mupen64plus-next.elf $(OUT)/cores/mupen64plus-next.nacp romfs/app-icon.jpg $(CORE_ROMFS)/.stamp
	elf2nro $< $@ --nacp=$(OUT)/cores/mupen64plus-next.nacp --icon=romfs/app-icon.jpg --romfsdir=$(CORE_ROMFS)
-include $(OUT)/n64_runner.d $(OUT)/n64_video.d

-include $(OUT)/ds_runner.d
