#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${ESP_MATTER_PATH:?Set ESP_MATTER_PATH to an installed esp-matter checkout}"
UPSTREAM="$ESP_MATTER_PATH/examples/light_switch"
DEST="$ROOT/generated/light_switch"
[[ -f "$UPSTREAM/main/app_main.cpp" ]] || { echo "Missing official light_switch example at $UPSTREAM" >&2; exit 1; }
mkdir -p "$ROOT/generated"
rm -rf "$DEST"
cp -a "$UPSTREAM" "$DEST"
cp "$ROOT/overlay/doorbell_uart.cpp" "$DEST/main/doorbell_uart.cpp"
cp "$ROOT/overlay/doorbell_uart.h" "$DEST/main/doorbell_uart.h"
cat "$ROOT/overlay/Kconfig.projbuild" >> "$DEST/main/Kconfig.projbuild"
python3 - "$DEST" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]); cm=root/'main/CMakeLists.txt'; s=cm.read_text()
s=s.replace('set(SRC_DIRS_LIST ".")', 'set(SRC_DIRS_LIST ".")')
s += '\n# Add serial UI transport to original esp-matter light switch example\ntarget_sources(${COMPONENT_LIB} PRIVATE "doorbell_uart.cpp")\n'
cm.write_text(s)
p=root/'main/app_main.cpp';s=p.read_text();
s='#include "doorbell_uart.h"\n'+s
needle='    err = esp_matter::start(app_event_cb);'
assert needle in s
s=s.replace(needle, '    doorbell_uart_init();\n'+needle)
s=s.replace('    app_reset_button_register(switch_handle);', '    // Physical button is on M5StickC, not on the H2\n')
p.write_text(s)
p=root/'main/app_driver.cpp';s=p.read_text()
s+='\nextern "C" void doorbell_matter_press(void) { app_driver_button_toggle_cb(NULL, NULL); }\n'
p.write_text(s)
# Disable physical board-button dependency (Button A is on the M5Stick, not on the H2).
start=s.index('    /* Initialize button */',s.index('app_driver_handle_t app_driver_switch_init()'))
end=s.index('    /* Other initializations */',start)
s=s[:start]+'    /* Doorbell input arrives over UART from M5StickC Plus2. */\n'+s[end:]
s=s.replace('    return (app_driver_handle_t)btns[0];','    return nullptr;')
p.write_text(s)
# Ensure correct flash capacity of U195 H2-MINI-1-N2 (2 MB).
p=root/'sdkconfig.defaults';s=p.read_text().replace('CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y','CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y')
s+='\nCONFIG_DB_H2_UART_RX=23\nCONFIG_DB_H2_UART_TX=24\nCONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y\nCONFIG_ESP_CONSOLE_UART_DEFAULT=n\n'
p.write_text(s)
PY
printf 'Created %s\n' "$DEST"
echo 'U195 Grove: H2 RX=GPIO23, TX=GPIO24 (UART0 pins; console via USB Serial/JTAG). Confirm flash partition fit before flashing.'
echo "cd '$DEST'; idf.py set-target esp32h2; idf.py menuconfig; idf.py build"
