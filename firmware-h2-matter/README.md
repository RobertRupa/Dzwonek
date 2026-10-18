# Matter over Thread – U195 ESP32-H2 + M5StickC Plus2

## Implementation

This directory contains a *source overlay* for the official [esp-matter `light_switch`](https://github.com/espressif/esp-matter/tree/main/examples/light_switch) example, not a bundled copy of Espressif's large SDK. The H2 acts as a **Matter-over-Thread On/Off Light Switch** (with binding), not a native doorbell device type. M5StickC Plus2 is the display + Wi-Fi setup frontend.

### Prerequisites

1. Install a compatible ESP-IDF version and [esp-matter](https://github.com/espressif/esp-matter) following the matching SDK instructions. Include its required submodules.
2. Export `ESP_MATTER_PATH` pointing to the clone.
3. In this project, run `./firmware-h2-matter/scripts/prepare.sh`. It creates `firmware-h2-matter/generated/light_switch` (gitignored).
4. `cd firmware-h2-matter/generated/light_switch`
5. `idf.py set-target esp32h2 && idf.py menuconfig && idf.py build`
6. **Check build size / flash partitions for 2 MB U195**. If the image cannot fit, do not flash it: remove unused features or choose an H2 module with more flash. The original upstream sample is often configured for other boards and 4 MB.
7. **Verify U195 UART routing and monitor configuration** before flashing. The U195 Grove interface uses `H2 RX=GPIO23`, `H2 TX=GPIO24` (UART0 pins). The bridge uses `UART_NUM_1` routed through those GPIOs, and the console is configured for USB Serial/JTAG. After generating, inspect `sdkconfig` and check for conflicting GPIO usage.
8. Flash H2 over its own USB interface with `idf.py -p /dev/ttyACM0 flash monitor` (use actual detected port). Flash the M5StickC via PlatformIO separately.

### UART protocol

`MATTER INFO` => `MATTER 20202021 34970112332 MT:Y.K9042C00KA0648G00 available` for **unmodified official development onboarding defaults** only. This is illustrative, not production security. `RING ...` triggers the upstream light-switch On/Off *client* toggle event, which needs Matter binding to an appropriate On/Off server. `PING` → `PONG H2 UART 115200`, `HELLO`, `STATUS` are supported. If there is no `H2 <` response in the M5Stick serial monitor, check H2 power, flashed firmware, GPIO23/24 and crossed UART lines. Legacy `DATASET` / `TARGET` / UDP are not supported in this firmware.

> Note: PIN, manual code and QR MUST correspond to actual installed commissioning data. If you change provisioning, replace the constants in `overlay/doorbell_uart.cpp` with an API reading your actual commissioning payload from CHIP (do not keep hardcoded constants). Test firmware is **not secure for public deployment**.

### Pairing

Pair the ESP32-H2 device through a Thread-capable Matter controller + Thread Border Router. On the M5StickC, press **B** to show PIN and QR. The same code appears in the M5Stick WebGUI at `192.168.4.1`. Matter commissioning uses BLE for discovery and the Thread dataset supplied by the controller. The M5Stick Wi-Fi credentials are *not* Thread credentials.

Important: the upstream light-switch device requires a Matter binding (and appropriate ACL) after commissioning before a press can actuate another device. See the official light-switch README. This package does not implement Matter Generic Switch `InitialPress`/`ShortRelease` events or native doorbell semantics.

The QR is an **onboarding QR for firmware that actually runs the Matter stack**, not a way to magically enable Matter in the legacy OpenThread UDP firmware.

### Security

The project's original AP is open and has no web authentication as requested. Anybody nearby can view the Matter onboarding code, change Wi-Fi credentials, and invoke the bell. Disable the AP or add authentication before deploying in a shared/public environment. The demo setup code is also shared with other development devices. Production devices require distinct commissioning data and secure provisioning.


### Zmiana konfiguracji / odświeżenie projektu

Ponownie uruchom `prepare.sh` po aktualizacji kodu. Skrypt usuwa tylko wygenerowaną kopię `generated/light_switch`. W `idf.py menuconfig` upewnij się, że konsola jest na USB Serial/JTAG, GPIO RX=23, TX=24. Po zmianie `sdkconfig.defaults` istniejące `sdkconfig` może zachować stare wartości — użyj `idf.py reconfigure` lub wygeneruj konfigurację od nowa. Brak kodu Matter na M5Stick nie dowodzi błędu kodu parowania; najpierw sprawdź odpowiedź `PONG` z H2.


### U195 2 MB: single factory image, without OTA

The source `examples/light_switch/partitions.csv` targets 4 MB with two OTA slots. The `prepare.sh` overlay replaces it with `overlay/partitions_2mb.csv`, reserving the app region from 0x20000 to 0x1e0000 (1,835,008 bytes) and retaining the NVS/fctry partitions. OTA Requestor and CHIP shell are disabled. **An app image larger than the factory app partition will not fit.** A successful partition-table generation is not proof the complete Matter binary fits on H2-MINI-1-N2.

Recreate generated sources after pulling repo changes (this deletes the previously generated example; preserve any local modifications first):

```bash
cd ~/Documents/Dzwonek
export ESP_MATTER_PATH=$HOME/esp/esp-matter
./firmware-h2-matter/scripts/prepare.sh
cd firmware-h2-matter/generated/light_switch
idf.py set-target esp32h2
idf.py build
```

Do not change the configured 2 MB flash size to 4 MB on a 2 MB module. This image has no OTA partition. If the binary overflows, feature trimming or a higher-flash module is required.
