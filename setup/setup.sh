#!/bin/bash
# Full setup of the Zero on a fresh Raspberry Pi OS (Bookworm or newer).
#
#   curl -fsSL https://raw.githubusercontent.com/SDU-Krakens/rc-boat/main/setup/setup.sh | sudo bash
#
# Safe to run again. All secrets are prompted, nothing secret is in this file.

set -euo pipefail

REPO_URL=https://github.com/SDU-Krakens/rc-boat.git
REPO_BRANCH=main
# Built by .github/workflows/build.yml on every push to main
RELEASE_URL=https://github.com/SDU-Krakens/rc-boat/releases/latest/download
LOG_DIR=/var/log/rc-boat
GROUP=rc-boat
CMD_USER=cmd
CMD_SHELL=/usr/local/bin/rc-boat-cmd
WIFI_CON=rc-boat-wifi
DNS=1.1.1.1
BOAT_HOSTNAME=boat

step() { printf '\n==> %s\n' "$*"; }

# stdin is this script when piped from curl, so prompts read /dev/tty
ask() {
	local var=$1 prompt=$2 def=${3-} ans
	read -rp "$prompt${def:+ [$def]}: " ans </dev/tty
	printf -v "$var" '%s' "${ans:-$def}"
}

ask_secret() {
	local var=$1 prompt=$2 a b
	while true; do
		read -rsp "$prompt: " a </dev/tty
		echo >/dev/tty
		read -rsp "$prompt (again): " b </dev/tty
		echo >/dev/tty
		if [ -z "$a" ]; then
			echo "empty, try again" >/dev/tty
		elif [ "$a" != "$b" ]; then
			echo "no match, try again" >/dev/tty
		else
			break
		fi
	done
	printf -v "$var" '%s' "$a"
}

if [ "$(id -u)" -ne 0 ]; then
	echo "run as root" >&2
	exit 1
fi

step "Questions"
ask REPO_DIR "Repo directory" /opt/rc-boat
ask WIFI_SSID "Wi-Fi SSID"
ask_secret WIFI_PASS "Wi-Fi passphrase"
ask_secret CMD_PASS "Password for the $CMD_USER user"
ask_secret ROOT_PASS "Password for root"

step "Packages"
apt-get update
apt-get install -y git curl make python3 openocd

step "Repo $REPO_BRANCH in $REPO_DIR"
if [ -d "$REPO_DIR/.git" ]; then
	git -C "$REPO_DIR" fetch origin "$REPO_BRANCH"
	git -C "$REPO_DIR" checkout -f "$REPO_BRANCH"
	git -C "$REPO_DIR" reset --hard "origin/$REPO_BRANCH"
else
	mkdir -p "$(dirname "$REPO_DIR")"
	git clone --branch "$REPO_BRANCH" "$REPO_URL" "$REPO_DIR"
fi

step "Download the latest build"
# Download to a temp file and mv over the old one: a running rc-boat cannot be
# written to ("Text file busy"), but it can be replaced
download() {
	local name=$1 dest=$2
	mkdir -p "$(dirname "$dest")"
	curl -fsSL -o "$dest.tmp" "$RELEASE_URL/$name"
	mv -f "$dest.tmp" "$dest"
}
download rc-boat "$REPO_DIR/dist/rpi/rc-boat"
download pico.elf "$REPO_DIR/dist/pico/pico.elf"
download config.mk "$REPO_DIR/dist/config/config.mk"
chmod 755 "$REPO_DIR/dist/rpi/rc-boat"

step "Log directory"
mkdir -p "$LOG_DIR"

step "UART and SPI"
raspi-config nonint do_serial_hw 0
raspi-config nonint do_serial_cons 1
raspi-config nonint do_spi 0

step "Group $GROUP and user $CMD_USER"
getent group "$GROUP" >/dev/null || groupadd --system "$GROUP"
sed "s|@REPO_DIR@|$REPO_DIR|g" "$REPO_DIR/setup/cmd-shell" >"$CMD_SHELL"
chmod 755 "$CMD_SHELL"
grep -qxF "$CMD_SHELL" /etc/shells || echo "$CMD_SHELL" >>/etc/shells
if id "$CMD_USER" >/dev/null 2>&1; then
	usermod -s "$CMD_SHELL" -aG "$GROUP" "$CMD_USER"
else
	useradd -m -s "$CMD_SHELL" -G "$GROUP" "$CMD_USER"
fi
printf '%s:%s\n' "$CMD_USER" "$CMD_PASS" | chpasswd

step "Root login over SSH"
printf 'root:%s\n' "$ROOT_PASS" | chpasswd
echo "PermitRootLogin yes" >/etc/ssh/sshd_config.d/rc-boat.conf

step "Hostname $BOAT_HOSTNAME"
raspi-config nonint do_hostname "$BOAT_HOSTNAME"

step "boat.service"
sed "s|@REPO_DIR@|$REPO_DIR|g" "$REPO_DIR/setup/boat.service" \
	>/etc/systemd/system/boat.service
systemctl daemon-reload
systemctl enable boat

step "Flash the Pico"
if ! make -C "$REPO_DIR" flash; then
	echo "WARNING: flash failed, run 'make flash' in $REPO_DIR later" >&2
fi

# Last, because it can drop the current SSH session
step "Wi-Fi $WIFI_SSID"
nmcli connection delete "$WIFI_CON" >/dev/null 2>&1 || true
nmcli connection add type wifi con-name "$WIFI_CON" ifname wlan0 \
	ssid "$WIFI_SSID" \
	wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$WIFI_PASS" \
	ipv4.dns "$DNS" ipv4.ignore-auto-dns yes \
	connection.autoconnect yes connection.autoconnect-priority 10

step "Done"
echo "repo:    $REPO_DIR ($REPO_BRANCH)"
echo "service: boat (starts after reboot)"
echo "host:    $BOAT_HOSTNAME"
echo "cmd:     ssh $CMD_USER@<zero>"
echo "root:    ssh root@<zero>"
echo "wifi:    $WIFI_SSID (connection $WIFI_CON, DNS $DNS)"
echo "Rebooting for UART/SPI changes..."
reboot
