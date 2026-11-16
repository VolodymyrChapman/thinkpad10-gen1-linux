#!/bin/sh
# Install current Firefox (not ESR) from Mozilla's own APT repository.
#
# Why: Firefox ESR 153 demands camera frames without row padding, which
# atomisp cannot deliver (e.g. 1624-pixel rows stored 1664 bytes apart).
# Firefox 160+ reads the row stride from each PipeWire buffer instead.
#
#   sh install-firefox-release.sh        (asks for your sudo password)
#
# Firefox ESR stays installed; both can be used side by side.
set -eu

KEY=/etc/apt/keyrings/packages.mozilla.org.asc
FPR=35BAA0B33E9EB396F59CA838C0BA5CE6DC6315A3   # Mozilla's published fingerprint

sudo install -d -m 0755 /etc/apt/keyrings
wget -q https://packages.mozilla.org/apt/repo-signing-key.gpg -O- | sudo tee "$KEY" >/dev/null

got=$(gpg -n -q --import --import-options import-show "$KEY" 2>/dev/null | sed -n 2p | tr -d ' ')
if [ "$got" != "$FPR" ]; then
	echo "Key fingerprint mismatch ($got), aborting" >&2
	sudo rm -f "$KEY"
	exit 1
fi
echo "Mozilla key fingerprint OK"

echo "deb [signed-by=$KEY] https://packages.mozilla.org/apt mozilla main" |
	sudo tee /etc/apt/sources.list.d/mozilla.list >/dev/null

# Only take the firefox package (and its language packs) from Mozilla, so
# nothing else, including firefox-esr, changes source.
printf '%s\n' 'Package: firefox firefox-l10n-*' 'Pin: origin packages.mozilla.org' 'Pin-Priority: 1000' '' \
	'Package: *' 'Pin: origin packages.mozilla.org' 'Pin-Priority: 100' |
	sudo tee /etc/apt/preferences.d/mozilla >/dev/null

sudo apt-get update
sudo apt-get install -y firefox
firefox --version
