# Haiku-Arcade

Native **Haiku** window for the Arcade shelf. First title: **NOWAY HOME**.

This is **not** inside the Linux Arcade tree. Keep the two repos as siblings:

```
~/Arcade          https://github.com/AiLang-Author/ARCADE
~/Haiku-Arcade    this repo
```

Shared pieces are **symlinks** in this checkout (not copies):

| here            | points at                          |
|-----------------|------------------------------------|
| `assets/`       | `../Arcade/assets` (SVG, SFX, BGM) |
| `fonts/`        | `../Arcade/fonts`                  |
| `arcade_app.x`  | `../Arcade/arcade_app.x` (kernel)  |
| `LICENSE`       | `../Arcade/LICENSE` (SCSL v1.0)    |
| `noway-home/`   | `../Arcade/noway-home` (shots)     |

The kernel still owns the pixels. This process is Haiku-native (`libbe` blit,
`libgame` → **media_server**). Linux GTK / miniaudio stay in Arcade.

## Click-and-run (Haiku testers)

On a Linux box that has both repos as siblings:

```
cd ~/Haiku-Arcade
sh pack.sh
```

That writes `dist/NOWAY-HOME-Haiku/` with real copies (assets, music, kernel,
installer). Copy **that folder** onto Haiku (USB, shared folder, scp).

On Haiku:

1. Open the folder in Tracker.
2. Double-click `install.sh` (or Terminal: `sh install.sh`).
3. Deskbar leaf → **NOWAY HOME**.
4. If `/dev/misc/sys_compat` is missing, reboot once, then launch again.
5. Media prefs: unmute, raise volume. Audio is the system mixer, not QEMU.

Needs Haiku R1/beta4+ x86_64, `media_server` (default desktop), and a sound
device (real hardware, or a virtual AC97/HDA if you are in a VM). The app
does not talk to any hypervisor.

If the drop has no prebuilt `arcade_shell_haiku`, install **gcc** from
HaikuDepot and the script will compile it (`-lbe -lgame`).

## Development checkout

```
git clone git@github.com:AiLang-Author/ARCADE.git
git clone git@github.com:AiLang-Author/Haiku-Arcade.git
# same parent directory so the symlinks resolve
```

On Haiku, with gcc:

```
cd Haiku-Arcade
make
sh install.sh
```

`install.sh` follows the symlinks and copies into `~/config/non-packaged/`.

## What gets installed

| path | what |
|------|------|
| `~/config/non-packaged/apps/NOWAY HOME` | Interface Kit window |
| `~/config/non-packaged/data/ailang_arcade/` | kernel, `assets/`, `fonts/` |
| `~/config/non-packaged/bin/sys_compat_run` | Linux ELF loader |
| `…/add-ons/kernel/drivers/…/sys_compat` | Linux ABI driver |
| Deskbar leaf **NOWAY HOME** | launch |

IPC is `/tmp/arcade_app` on Haiku (RAM cache, not disk). Linux GTK still uses
`/dev/shm/arcade_app`.

## Keys

| Key | Action |
|-----|--------|
| ← → | move |
| z / space | fire |
| Enter | start / next wave |
| p | pause / settings (music + SFX volume) |
| Esc / q | quit |

## Audio

The kernel only writes `sfx.bin` and `vol.txt`. This window plays them through
Haiku **media_server** (`BFileGameSound`). Clips live in `assets/sfx/` and
`assets/music/` (48 kHz stereo WAV). Pause settings volume is live.

Linux plays the same ring with miniaudio inside the GTK host. Different mixer,
same files.

## Layout

```
arcade_shell_haiku.cxx   BApplication / BBitmap / Game Kit mixer
arcade_shell_haiku.rdef  signature application/x-vnd.Ailang-Arcade
Makefile                 g++ -lbe -lgame
install.sh               click-and-run on Haiku
pack.sh                  vendor a tester drop (run next to Arcade)
linux_abi/               sys_compat + sys_compat_run (Haiku binaries)
assets -> ../Arcade/assets
fonts  -> ../Arcade/fonts
arcade_app.x -> ../Arcade/arcade_app.x
```

## Build (on Haiku)

```
g++ -O2 -o arcade_shell_haiku arcade_shell_haiku.cxx -lbe -lgame
# or
make
```

`arcade_app.x` is built on Linux with `ailang.x` in the Arcade repo. Do not
rebuild it on Haiku.

## sys_compat

Without the driver, the kernel cannot run. The installer uses, in order:

1. `linux_abi/` in this drop
2. An existing CAD Haiku install

If you already run AILang CAD on the same Haiku box, you already have it.

## Screenshots

Same playfield as Linux (kernel owns the pixels). Shots live in Arcade
`noway-home/` and are linked here.

**Attract**

![Title](https://raw.githubusercontent.com/AiLang-Author/ARCADE/main/noway-home/Intro.png)

**Stage**

![Play](https://raw.githubusercontent.com/AiLang-Author/ARCADE/main/noway-home/gameplay.png)

**Queen**

![Queen](https://raw.githubusercontent.com/AiLang-Author/ARCADE/main/noway-home/bossbattle.png)

Copyright © 2026 Sean Collins, 2 Paws Machine and Engineering. SCSL v1.0.
