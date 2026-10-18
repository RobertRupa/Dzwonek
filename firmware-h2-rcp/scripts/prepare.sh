#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${IDF_PATH:?Activate ESP-IDF first: source ~/esp/esp-idf/export.sh}"
UPSTREAM="$IDF_PATH/examples/openthread/ot_rcp"
DEST="$ROOT/generated/ot_rcp"

if [[ ! -f "$UPSTREAM/main/esp_ot_config.h" || ! -f "$UPSTREAM/sdkconfig.defaults" ]]; then
  echo "ERROR: ESP-IDF OpenThread RCP example not found: $UPSTREAM" >&2
  exit 1
fi
if [[ "$DEST" == "$UPSTREAM" ]]; then
  echo "Refusing to overwrite upstream example" >&2; exit 1
fi
if [[ -e "$DEST" ]]; then
  echo "ERROR: $DEST already exists; remove it manually if regeneration is intended." >&2
  exit 1
fi
mkdir -p "$ROOT/generated"
cp -a "$UPSTREAM" "$DEST"
cat >> "$DEST/sdkconfig.defaults" <<'EOF'

# M5Stack Unit Gateway H2 U195: Grove UART Spinel transport
CONFIG_OPENTHREAD_RCP_UART=y
CONFIG_OPENTHREAD_UART_PIN_MANUAL=y
CONFIG_OPENTHREAD_UART_RX_PIN=23
CONFIG_OPENTHREAD_UART_TX_PIN=24

# Keep UART0 free for Spinel: console via dedicated USB Serial/JTAG
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_UART_DEFAULT=n

# U195 H2-MINI-1-N2 has 2 MB flash
CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y
EOF
echo "Generated: $DEST"
echo "UART Spinel (460800 8N1): H2 RX GPIO23, H2 TX GPIO24; console USB Serial/JTAG"
echo "NEXT: cd '$DEST'; idf.py set-target esp32h2; idf.py build; idf.py size"
echo "Verify actual U195 Grove GPIO routing and sdkconfig before flashing."
