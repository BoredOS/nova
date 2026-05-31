# Nova Desktop Environment & GUI SDK

This repository groups the complete graphical desktop environment and user interface widget libraries for BoredOS.

## Component Layout
- **`libui/`**: Core GUI Widget components (buttons, text boxes, windows, rendering, anti-aliased panel outlines).
- **`libtheme/`**: Desktop theme loading and graphical assets manager.
- **`libnovaproto/`**: IPC and Compositor communication protocol layer.
- **`src/`**: Compositor (`nova.c`), shell bar (`taskbar.c`), and active wallpaper rendering daemon (`wallpaperd.c`), along with basic GUI applications.

## Decoupled Building & Two-Stage SDK Export

This repository is designed to compile **either within the main BoredOS tree OR completely standalone**.

### 1. Integrated Build (Within BoredOS)
If built from the BoredOS root tree, the build system compiles the pure standard library first, then compiles `nova` passing `BOREDOS_SDK`. 
`nova` compiles `libui`, `libtheme`, and `libnovaproto`, exports them and their headers into the shared SDK (**Stage 2 SDK Export**), and links the compositor/desktop apps:
```bash
make BOREDOS_SDK=/path/to/shared/sdk
```

### 2. Standalone Build (Isolated Clone)
If cloned completely separately in isolation, running `make` will **automatically bootstrap standard dependencies**:
```bash
make
```
If `build/sdk` is missing, the Makefile automatically clones the pure standard library dependency from `https://github.com/boredos/libc.git`, compiles it, installs it to `build/sdk`, compiles `nova`'s internal libraries, exports them to the local SDK, and builds the graphical apps cleanly in full isolation!

## Staging Installation
To stage the compiled executables and configurations into your target initrd root filesystem directory:
```bash
make DESTDIR=/path/to/initrd/root install
```
- Binaries (`*.elf`) are routed to `/bin/`
- Styling configurations (`*.conf`) are routed to `/Library/conf/` and `/etc/nova/`
