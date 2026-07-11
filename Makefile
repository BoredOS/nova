# Copyright (c) 2026 Christiaan (chris@boreddev.nl)
# Nova Desktop Environment & GUI SDK Makefile

.SECONDARY:

CC = x86_64-elf-gcc
AR = x86_64-elf-ar
LD = x86_64-elf-ld

# Smart SDK Resolution Logic
ifneq ($(BOREDOS_SDK),)
  ifeq ($(wildcard $(BOREDOS_SDK)/lib/libc.a),)
    BOOTSTRAP_SDK = $(BOREDOS_SDK)
    SDK_PATH      = $(BOREDOS_SDK)
  else
    SDK_PATH      = $(BOREDOS_SDK)
  endif
endif

# If SDK is still unresolved, fall back to a local standalone build folder
ifeq ($(SDK_PATH),)
  SDK_PATH = $(abspath build/sdk)
  ifeq ($(wildcard $(SDK_PATH)/lib/libc.a),)
    BOOTSTRAP_SDK = $(SDK_PATH)
  endif
endif

DESTDIR ?= $(abspath build/dist)

CFLAGS  = -Wall -Wextra -std=gnu11 -ffreestanding -O2 -fno-stack-protector \
          -fno-stack-check -fno-lto -fno-pie -m64 -march=x86-64 -mno-red-zone \
          -isystem $(SDK_PATH)/include -Ilibnovaproto -Intk -I. -Isrc

LDFLAGS = -m elf_x86_64 -nostdlib -static -no-pie -Ttext=0x40000000 \
          --no-dynamic-linker -z text -z max-page-size=0x1000 -e _start \
          -L$(SDK_PATH)/lib

LIBS = obj/libnovaproto.a obj/libntk.a
APPS = nova.elf taskbar.elf wallpaperd.elf about.elf helloworld.elf run.elf installer.elf

all: $(LIBS)
	$(MAKE) export-sdk
	$(MAKE) $(APPS)

# Compile GUI static libraries
obj/libnovaproto.o: libnovaproto/novaproto.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

NTK_SRC = $(wildcard ntk/*.c)
NTK_OBJ = $(patsubst ntk/%.c,obj/ntk_%.o,$(NTK_SRC)) obj/stb_image.o obj/utf-8.o

obj/ntk_%.o: ntk/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/stb_image.o: src/stb_image.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/utf-8.o: src/utf-8.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/libntk.a: $(NTK_OBJ)
	$(AR) rcs $@ $(NTK_OBJ)

obj/libnovaproto.a: obj/libnovaproto.o
	$(AR) rcs $@ $<

# Export GUI SDK components into the resolved SDK path
export-sdk: $(LIBS)
	mkdir -p $(SDK_PATH)/include $(SDK_PATH)/lib
	cp libnovaproto/*.h ntk/*.h src/utf-8.h src/stb_image.h src/stb_image_write.h $(SDK_PATH)/include/
	cp $(LIBS) $(SDK_PATH)/lib/

# Compile Desktop Application binaries
obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

%.elf: obj/%.o $(LIBS)
	$(LD) $(LDFLAGS) $(SDK_PATH)/lib/crt0.o $(SDK_PATH)/lib/crti.o $< -lntk -lnovaproto -lc $(SDK_PATH)/lib/crtn.o -o $@

install: all
	mkdir -p $(DESTDIR)/bin
	cp $(APPS) $(DESTDIR)/bin/
	mkdir -p $(DESTDIR)/Library/AppData/org.boredos.nova
	cp assets/taskbar.conf assets/wallpaper.conf assets/nova.conf $(DESTDIR)/Library/AppData/org.boredos.nova/
	@if [ -d pack/assets ]; then \
		mkdir -p $(DESTDIR)/Library; \
		cp -a pack/assets/* $(DESTDIR)/Library/; \
	fi
	@if [ -d pack/apps ]; then \
		mkdir -p $(DESTDIR)/Library/AppData/org.boredos.nova; \
		cp pack/apps/*.desktop $(DESTDIR)/Library/AppData/org.boredos.nova/; \
	fi

.PHONY: deps bup
.PHONY: deps
deps:
	@if [ ! -d "external/libc" ]; then \
		git clone https://github.com/boredos/libc.git external/libc; \
	else \
		echo "external/libc already present"; \
	fi

.PHONY: bup
bup: all
	@sh -c 'V=$$(sed -n '\''s/^#define NOVA_VERSION //p'\'' libnovaproto/novaproto.h); \
	BUPNAME=nova-$$V.bup; \
	echo "Assembling package $$BUPNAME..."; \
	rm -rf build/package; \
	mkdir -p build/package/bin build/package/lib build/package/assets build/package/config build/package/usr/share/applications; \
	cp -a pack/MANIFEST.toml build/package/; \
	for f in $(APPS); do if [ -f "$$f" ]; then cp $$f build/package/bin/; fi; done; \
	if [ -d assets ]; then cp -a assets/* build/package/config/; fi; \
	if [ -f assets/nova.conf ]; then cp assets/nova.conf build/package/config/; fi; \
	if [ -d pack/assets ]; then cp -a pack/assets/* build/package/assets/; fi; \
	if [ -d pack/scripts ]; then cp -a pack/scripts build/package/; fi; \
	if [ -d pack/apps ]; then cp -a pack/apps/*.desktop build/package/usr/share/applications/; fi; \
	mkdir -p build; OUT=build/$$BUPNAME; \
	SRCDIRS="MANIFEST.toml bin config assets usr"; \
	if [ -d build/package/scripts ]; then SRCDIRS="$$SRCDIRS scripts"; fi; \
	x86_64-elf-strip --strip-unneeded build/package/bin/*.elf 2>/dev/null || true; \
	tar -cf build/nova.tar -C build/package $$SRCDIRS; \
	lz4 -f build/nova.tar build/nova.bup; \
	cp build/nova.bup $$OUT; \
	rm -f build/nova.tar; \
	echo "Created $$OUT"; \
	rm -rf build/package'

clean:
	rm -rf obj build $(APPS)
