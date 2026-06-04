# Copyright (c) 2026 Christiaan (chris@boreddev.nl)
# Nova Desktop Environment & GUI SDK Makefile

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
          -isystem $(SDK_PATH)/include -Ilibnovaproto -Ilibtheme -Ilibui -Ilibapp -Ilibwidget -I.

LDFLAGS = -m elf_x86_64 -nostdlib -static -no-pie -Ttext=0x40000000 \
          --no-dynamic-linker -z text -z max-page-size=0x1000 -e _start \
          -L$(SDK_PATH)/lib

LIBS = obj/libnovaproto.a obj/libtheme.a obj/libui.a obj/libapp.a obj/libwidget.a
APPS = nova.elf taskbar.elf wallpaperd.elf about.elf helloworld.elf explorer.elf libappdemo.elf

all: bootstrap-sdk
	$(MAKE) export-sdk
	$(MAKE) $(APPS)

# Autonomic SDK Bootstrapper
.PHONY: bootstrap-sdk
bootstrap-sdk:
ifdef BOOTSTRAP_SDK
	@if [ ! -f "$(BOOTSTRAP_SDK)/lib/libc.a" ]; then \
		if [ -d "../libc" ]; then \
			echo "[STANDALONE] Peer libc found at ../libc. Building standard SDK..."; \
			$(MAKE) -C ../libc SDK_DIR=$(BOOTSTRAP_SDK) install; \
		else \
			echo "[STANDALONE] SDK and peer libc not found. Fetching libc from GitHub..."; \
			mkdir -p build; \
			if [ ! -d "build/libc_src" ]; then \
				git clone https://github.com/boredos/libc.git build/libc_src; \
			fi; \
			$(MAKE) -C build/libc_src SDK_DIR=$(BOOTSTRAP_SDK) install; \
		fi \
	fi
endif

# Compile GUI static libraries
obj/libnovaproto.o: libnovaproto/novaproto.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/libtheme.o: libtheme/theme.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/libui.o: libui/ui.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/libapp.o: libapp/app.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/libwidget.o: libwidget/widget.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

obj/lib%.a: obj/lib%.o
	$(AR) rcs $@ $<

# Export GUI SDK components into the resolved SDK path
export-sdk: $(LIBS)
	mkdir -p $(SDK_PATH)/include $(SDK_PATH)/lib
	cp libnovaproto/*.h libtheme/*.h libui/*.h libapp/*.h libwidget/*.h $(SDK_PATH)/include/
	cp $(LIBS) $(SDK_PATH)/lib/

# Compile Desktop Application binaries
obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

%.elf: obj/%.o $(LIBS)
	$(LD) $(LDFLAGS) $(SDK_PATH)/lib/crt0.o $< -lwidget -lapp -lui -ltheme -lnovaproto -lc -o $@

install: all
	mkdir -p $(DESTDIR)/bin
	cp $(APPS) $(DESTDIR)/bin/
	mkdir -p $(DESTDIR)/Library/conf
	cp assets/taskbar.conf assets/wallpaper.conf $(DESTDIR)/Library/conf/
	mkdir -p $(DESTDIR)/etc/nova
	cp assets/nova.conf $(DESTDIR)/etc/nova/

clean:
	rm -rf obj build $(APPS)
