#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# Install the ThinkPad 10 camera switcher for the current user (no root).
#
#   sh install.sh           command + menu entry "Switch Camera"
#   sh install.sh --panel   also add a "Switch Camera" button to the
#                           XFCE panel (right-click it for Front/Rear)
#   sh install.sh --remove  remove all of the above
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=$HOME/.local/bin
APPS=${XDG_DATA_HOME:-$HOME/.local/share}/applications
DESKTOP=tp10-camera.desktop
PANEL_DIR=$HOME/.config/xfce4/panel

panel_plugin_id() { # id of our panel launcher, if any
	for d in "$PANEL_DIR"/launcher-*; do
		[ -e "$d/$DESKTOP" ] && { basename "$d" | sed 's/launcher-//'; return 0; }
	done
	return 1
}

panel_ids() {
	xfconf-query -c xfce4-panel -p /panels/panel-1/plugin-ids 2>/dev/null |
		grep -E '^[0-9]+$'
}

set_panel_ids() { # ids...
	args=""
	for i in "$@"; do args="$args -t int -s $i"; done
	# shellcheck disable=SC2086
	xfconf-query -c xfce4-panel -p /panels/panel-1/plugin-ids --force-array $args
}

add_panel() {
	command -v xfconf-query >/dev/null || { echo "XFCE not found, skipping panel button"; return 0; }
	if id=$(panel_plugin_id); then
		echo "Panel button already present (plugin $id)"
		return 0
	fi
	max=$(xfconf-query -c xfce4-panel -p /plugins -l | sed -n 's|^/plugins/plugin-\([0-9]*\)$|\1|p' | sort -n | tail -1)
	id=$(( ${max:-0} + 1 ))
	mkdir -p "$PANEL_DIR/launcher-$id"
	cp "$HERE/$DESKTOP" "$PANEL_DIR/launcher-$id/"
	xfconf-query -c xfce4-panel -p /plugins/plugin-$id -n -t string -s launcher
	xfconf-query -c xfce4-panel -p /plugins/plugin-$id/items -n --force-array -t string -s "$DESKTOP"
	# Put it after the last existing launcher, or at the end.
	after=$(for i in $(panel_ids); do
		[ "$(xfconf-query -c xfce4-panel -p /plugins/plugin-$i 2>/dev/null)" = launcher ] && echo "$i"
	done | tail -1)
	new=""
	for i in $(panel_ids); do
		new="$new $i"
		[ "$i" = "$after" ] && new="$new $id"
	done
	[ -n "$after" ] || new="$new $id"
	# shellcheck disable=SC2086
	set_panel_ids $new
	xfce4-panel -r >/dev/null 2>&1 &
	echo "Added a Switch Camera button to the panel (plugin $id)"
}

remove_panel() {
	id=$(panel_plugin_id) || return 0
	new=$(panel_ids | grep -vx "$id" | tr '\n' ' ')
	# shellcheck disable=SC2086
	set_panel_ids $new
	xfconf-query -c xfce4-panel -p /plugins/plugin-$id -r -R
	rm -rf "$PANEL_DIR/launcher-$id"
	xfce4-panel -r >/dev/null 2>&1 &
	echo "Removed the panel button"
}

case "${1:-}" in
--remove)
	remove_panel
	rm -f "$BIN/tp10-camera" "$APPS/$DESKTOP"
	echo "Removed tp10-camera"
	exit 0 ;;
""|--panel) ;;
*) echo "usage: sh install.sh [--panel|--remove]" >&2; exit 2 ;;
esac

for c in v4l2-ctl wpctl fuser; do
	command -v "$c" >/dev/null || echo "warning: $c not found (packages: v4l-utils, wireplumber, psmisc)"
done

install -D -m 0755 "$HERE/tp10-camera" "$BIN/tp10-camera"
install -D -m 0644 "$HERE/$DESKTOP" "$APPS/$DESKTOP"
command -v update-desktop-database >/dev/null && update-desktop-database "$APPS" 2>/dev/null || true
echo "Installed $BIN/tp10-camera and the \"Switch Camera\" menu entry"
case ":$PATH:" in *":$BIN:"*) ;; *) echo "note: $BIN is not on PATH in this shell; log out and in again" ;; esac

[ "${1:-}" = --panel ] && add_panel
exit 0
