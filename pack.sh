#!/bin/sh
# Build a click-and-run drop for Haiku testers.
# Run from this repo on a machine that has Arcade as a sibling:
#   ~/Arcade
#   ~/Haiku-Arcade
#
# Result: dist/NOWAY-HOME-Haiku/  (copy that folder onto Haiku, run install.sh)
#
# Copyright © 2026 Sean Collins, 2 Paws Machine and Engineering. SCSL v1.0.
set -e
HERE=`dirname "$0"`
cd "$HERE"
HERE=`pwd`
ROOT=`dirname "$HERE"`
ARCADE="$ROOT/Arcade"
DIST="$HERE/dist/NOWAY-HOME-Haiku"

if [ ! -d "$ARCADE/assets" ] || [ ! -f "$ARCADE/arcade_app.x" ]; then
	echo "FAIL: expected sibling $ARCADE with assets/ and arcade_app.x"
	echo "      clone https://github.com/AiLang-Author/ARCADE next to this repo"
	exit 1
fi

echo "=== packing NOWAY HOME for Haiku ==="
rm -rf "$HERE/dist"
mkdir -p "$DIST/linux_abi" "$DIST/bin" "$DIST/apps" "$DIST/assets" "$DIST/fonts"

cp -f "$HERE/install.sh" "$DIST/"
cp -f "$HERE/Makefile" "$DIST/"
cp -f "$HERE/arcade_shell_haiku.cxx" "$DIST/"
cp -f "$HERE/arcade_shell_haiku.rdef" "$DIST/"
cp -f "$HERE/README.md" "$DIST/"
cp -f "$ARCADE/LICENSE" "$DIST/" 2>/dev/null || true

# Kernel + shared art (dereference symlinks).
cp -f "$ARCADE/arcade_app.x" "$DIST/arcade_app.x"
cp -f "$ARCADE/arcade_app.x" "$DIST/bin/arcade_app.x"
cp -a "$ARCADE/assets/." "$DIST/assets/"
cp -a "$ARCADE/fonts/." "$DIST/fonts/"

# Optional prebuilt Haiku GUI (if harvested from a Haiku box).
if [ -f "$HERE/apps/NOWAY HOME" ]; then
	cp -f "$HERE/apps/NOWAY HOME" "$DIST/apps/NOWAY HOME"
elif [ -f "$HERE/arcade_shell_haiku" ]; then
	cp -f "$HERE/arcade_shell_haiku" "$DIST/arcade_shell_haiku"
	cp -f "$HERE/arcade_shell_haiku" "$DIST/apps/NOWAY HOME"
fi

# Haiku-native Linux ABI. Prefer this tree, then a live Haiku, then CAD.
copy_abi() {
	if [ -f "$1" ] && [ -f "$2" ]; then
		cp -f "$1" "$DIST/linux_abi/sys_compat"
		cp -f "$2" "$DIST/linux_abi/sys_compat_run"
		chmod 755 "$DIST/linux_abi/sys_compat" "$DIST/linux_abi/sys_compat_run"
		echo "  linux_abi from $1"
		return 0
	fi
	return 1
}

if ! copy_abi "$HERE/linux_abi/sys_compat" "$HERE/linux_abi/sys_compat_run"; then
	if ! copy_abi \
		/boot/system/non-packaged/add-ons/kernel/drivers/bin/sys_compat \
		/boot/home/sys_compat_run; then
		if ! copy_abi \
			/boot/home/config/non-packaged/add-ons/kernel/drivers/bin/sys_compat \
			/boot/home/config/non-packaged/bin/sys_compat_run; then
			echo "WARN: sys_compat not vendored. Testers need CAD Haiku already installed,"
			echo "      or copy the pair into linux_abi/ and re-run pack.sh."
			cp -f "$HERE/linux_abi/README" "$DIST/linux_abi/" 2>/dev/null || true
		fi
	fi
fi

chmod 755 "$DIST/install.sh"
echo "  $DIST"
echo "Copy that folder onto Haiku and double-click install.sh"
