#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Install (or with --remove, uninstall) the atomisp-tp10 DKMS package.
set -eu
[ "$(id -u)" = 0 ] || { echo "run as root: sudo sh $0" >&2; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/atomisp-tp10
NAME=atomisp-tp10
VER=$(sed -n 's/^PACKAGE_VERSION="\(.*\)"/\1/p' "$SRC/dkms.conf")
CONF=/etc/modprobe.d/atomisp-tp10.conf

remove_all() {
	for v in $(dkms status -m "$NAME" 2>/dev/null | sed -n "s|^$NAME/\([^,]*\),.*|\1|p" | sort -u); do
		dkms remove "$NAME/$v" --all || true
	done
}

if [ "${1:-}" = --remove ]; then
	remove_all
	rm -f "$CONF"
	echo "Removed $NAME; reboot to load the distribution's modules"
	exit 0
fi

remove_all
rm -rf "/usr/src/$NAME-$VER"
cp -a "$SRC" "/usr/src/$NAME-$VER"
dkms install "$NAME/$VER"
cat > "$CONF" <<'CONF'
# ThinkPad 10: let atomisp (DKMS atomisp-tp10) drive the ISP at 00:03.0
# instead of the power-off-only intel_atomisp2_pm driver.
blacklist intel_atomisp2_pm
CONF
echo "Installed $NAME $VER; reboot to use it"
