#!/bin/sh
# NOWAY HOME — click-and-run install for Haiku.
# Double-click this script in Tracker, or:  sh install.sh
#
# Copies the native window, Linux ELF kernel, assets (including music/SFX),
# fonts, and sys_compat into ~/config/non-packaged, then a Deskbar leaf.
#
# Copyright © 2026 Sean Collins, 2 Paws Machine and Engineering. SCSL v1.0.
set -e

HERE=`dirname "$0"`
cd "$HERE"
HERE=`pwd`

is_haiku() {
	u=`uname -s 2>/dev/null || echo`
	[ "$u" = "Haiku" ]
}

APPDIR="/boot/home/config/non-packaged/apps"
BINDIR="/boot/home/config/non-packaged/bin"
DATDIR="/boot/home/config/non-packaged/data/ailang_arcade"
DRVBIN="/boot/home/config/non-packaged/add-ons/kernel/drivers/bin"
DRVMISC="/boot/home/config/non-packaged/add-ons/kernel/drivers/dev/misc"
SYSBIN="/boot/system/non-packaged/add-ons/kernel/drivers/bin"
SYSMISC="/boot/system/non-packaged/add-ons/kernel/drivers/dev/misc"
MENU="/boot/home/config/non-packaged/data/deskbar/menu/Applications"
LEAF="/boot/home/config/settings/deskbar/menu"

find_file() {
	for p in "$@"; do
		if [ -f "$p" ]; then
			echo "$p"
			return 0
		fi
	done
	return 1
}

find_dir() {
	for p in "$@"; do
		if [ -d "$p" ]; then
			echo "$p"
			return 0
		fi
	done
	return 1
}

echo "=== NOWAY HOME / Haiku-Arcade install ==="

if ! is_haiku; then
	echo "This installer runs on Haiku."
	echo "On Linux, keep Arcade and Haiku-Arcade as siblings and run:"
	echo "  sh pack.sh"
	echo "then copy dist/NOWAY-HOME-Haiku onto a Haiku machine and run install.sh there."
	exit 1
fi

# Shared tree: this repo (symlinks or a packed drop) or sibling ../Arcade.
ASSETS=`find_dir \
	"$HERE/assets" \
	"$HERE/../Arcade/assets"` || {
	echo "FAIL: assets/ not found (clone Arcade as a sibling, or use the packed drop)."
	exit 1
}
FONTS=`find_dir \
	"$HERE/fonts" \
	"$HERE/../Arcade/fonts"` || {
	echo "FAIL: fonts/ not found."
	exit 1
}
KERN=`find_file \
	"$HERE/arcade_app.x" \
	"$HERE/bin/arcade_app.x" \
	"$HERE/../Arcade/arcade_app.x"` || {
	echo "FAIL: arcade_app.x not found (Linux ELF kernel)."
	exit 1
}

DRV=`find_file \
	"$HERE/linux_abi/sys_compat" \
	"$HERE/../Arcade/linux_abi/sys_compat" \
	"/boot/home/config/non-packaged/add-ons/kernel/drivers/bin/sys_compat" \
	"/boot/system/non-packaged/add-ons/kernel/drivers/bin/sys_compat"` || {
	DRV=""
}
RUN=`find_file \
	"$HERE/linux_abi/sys_compat_run" \
	"$HERE/bin/sys_compat_run" \
	"/boot/home/sys_compat_run" \
	"/boot/home/config/non-packaged/bin/sys_compat_run"` || {
	RUN=""
}

if [ -z "$DRV" ] || [ -z "$RUN" ]; then
	echo "FAIL: sys_compat / sys_compat_run missing."
	echo "      Put them in linux_abi/ (from a CAD Haiku install), or install CAD first."
	exit 1
fi

# Native window: use a prebuilt Haiku ELF, or compile here.
GUI=`find_file \
	"$HERE/apps/NOWAY HOME" \
	"$HERE/arcade_shell_haiku"` || GUI=""

if [ -z "$GUI" ]; then
	if command -v g++ >/dev/null 2>&1; then
		echo "--- Building arcade_shell_haiku ---"
		if [ -f "$HERE/Makefile" ]; then
			make -C "$HERE"
		else
			g++ -O2 -o "$HERE/arcade_shell_haiku" "$HERE/arcade_shell_haiku.cxx" -lbe -lgame
		fi
		GUI="$HERE/arcade_shell_haiku"
	else
		echo "FAIL: no arcade_shell_haiku binary and no g++."
		echo "      HaikuDepot → install gcc, then re-run, or use the packed drop."
		exit 1
	fi
fi

echo "--- Linux ABI (sys_compat) ---"
mkdir -p "$DRVBIN" "$DRVMISC" "$BINDIR"
cp -f "$DRV" "$DRVBIN/sys_compat"
chmod 755 "$DRVBIN/sys_compat"
ln -sfn "$DRVBIN/sys_compat" "$DRVMISC/sys_compat"
mimeset -f "$DRVBIN/sys_compat" 2>/dev/null || true
if mkdir -p "$SYSBIN" "$SYSMISC" 2>/dev/null; then
	cp -f "$DRV" "$SYSBIN/sys_compat"
	chmod 755 "$SYSBIN/sys_compat"
	ln -sfn "$SYSBIN/sys_compat" "$SYSMISC/sys_compat"
	echo "  driver: $SYSBIN/sys_compat"
fi
echo "  driver: $DRVBIN/sys_compat"

cp -f "$RUN" "$BINDIR/sys_compat_run"
cp -f "$RUN" /boot/home/sys_compat_run
chmod 755 "$BINDIR/sys_compat_run" /boot/home/sys_compat_run
echo "  runner: /boot/home/sys_compat_run"

i=0
while [ "$i" -lt 8 ]; do
	if [ -e /dev/misc/sys_compat ]; then
		break
	fi
	i=`expr "$i" + 1`
	sleep 1
done

mkdir -p /dev/shm 2>/dev/null || true

echo "--- NOWAY HOME ---"
mkdir -p "$APPDIR" "$DATDIR" "$DATDIR/assets" "$DATDIR/fonts" "$MENU" "$LEAF"
cp -f "$GUI" "$APPDIR/NOWAY HOME"
chmod 755 "$APPDIR/NOWAY HOME"
mimeset -f "$APPDIR/NOWAY HOME" 2>/dev/null || true

cp -f "$KERN" "$DATDIR/arcade_app.x"
cp -f "$KERN" /boot/home/arcade_app.x
chmod 755 "$DATDIR/arcade_app.x" /boot/home/arcade_app.x

echo "  assets: $ASSETS"
# Follow symlinks; copy real files so the game does not depend on this tree.
cp -a "$ASSETS/." "$DATDIR/assets/"
cp -a "$FONTS/." "$DATDIR/fonts/"

if [ ! -f "$DATDIR/assets/sfx/shot.wav" ]; then
	echo "FAIL: assets/sfx/shot.wav missing after copy."
	exit 1
fi
if [ ! -d "$DATDIR/assets/music" ]; then
	echo "WARN: assets/music missing — BGM will be silent, SFX still play."
fi

ln -sfn "$APPDIR/NOWAY HOME" "$MENU/NOWAY HOME"
ln -sfn "$APPDIR/NOWAY HOME" "$LEAF/NOWAY HOME"
echo "  app:  $APPDIR/NOWAY HOME"
echo "  data: $DATDIR"
echo "  kernel: $KERN"

if [ -e /dev/misc/sys_compat ]; then
	echo "  /dev/misc/sys_compat is present"
else
	echo "  /dev/misc/sys_compat not up yet — reboot once, then Deskbar → NOWAY HOME."
fi

if ps | grep -q media_server; then
	echo "  media_server is running (system mixer)"
else
	echo "  WARN: media_server not seen — start Media prefs, unmute, then launch."
fi

echo
echo "Installed. Deskbar leaf → NOWAY HOME"
echo "  Arrows move  Z/Space fire  Enter start  P settings  Esc quit"
echo "  Audio is Haiku media_server (Media prefs volume)."
echo "  Need a sound card on the machine (or a virtual AC97/HDA in a VM)."
