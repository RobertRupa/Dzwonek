# Thread Doorbell · M5StickC Plus2 + M5Stack Unit Gateway H2 U195

Projekt do VS Code / PlatformIO (ESP32 M5StickC Plus2) oraz ESP-IDF 5.5.x (ESP32-H2).

## Co działa / co jest zaimplementowane
- M5StickC Plus2: ekran, Button A jako dzwonek, Button B jako status, sygnał dźwiękowy.
- AP **bez hasła** `Doorbell-Setup-XXXX`, panel HTTP `http://192.168.4.1/`.
- Po uruchomieniu AP jest dostępne stale (również gdy działa Wi-Fi STA), co ułatwia ponowną konfigurację. **Brak hasła to ryzyko bezpieczeństwa:** każdy w zasięgu może zmienić ustawienia i wywołać dzwonek; nie instaluj w miejscu publicznym bez zabezpieczeń.
- Wi-Fi STA, panel także pod adresem LAN i `http://thread-doorbell.local/` jeśli mDNS działa.
- Preferences/NVS: SSID, hasło Wi-Fi, nazwa, IPv6 celu, port UDP, Thread Active Dataset TLV hex.
- H2: pełny OpenThread endpoint FTD, UART polecenia, transmisja IPv6 UDP z danymi `DOORBELL,nazwa,licznik`.
- H2 przechowuje dataset Thread we własnym NVS (OpenThread Settings). Dane konfiguracyjne panelu są osobno w NVS M5Stick.

**Nie jest to Matter i nie wykonuje automatycznego commissioning Thread.** Wymagany jest Active Operational Dataset TLV sieci Thread z posiadanego Border Routera, wpisany w panel. H2 nie jest Border Routerem. Aby odebrać UDP z LAN, Border Router musi zapewniać trasę IPv6 do adresu odbiorcy; najprościej użyć drugiego węzła Thread z nasłuchem UDP. Implementacja H2 wymaga weryfikacji na docelowym hardware, szczególnie GPIO UART.

## Połączenia

| StickC Plus2 | Grove | Unit Gateway H2 |
|---|---|---|
| GND | czarny | GND |
| 5V | czerwony | 5V |
| GPIO32 TX | żółty | H2 RX |
| GPIO33 RX | biały | H2 TX |

**Uwaga:** M5Stack dokumentuje złącze H2 jako UART RX/TX, ale nie podaje w tabeli pinów chipu H2. W firmware H2 domyślne wartości `CONFIG_DB_UART_RX=4`, `CONFIG_DB_UART_TX=5` są tylko PRZYKŁADEM i wymagają sprawdzenia schematu U195 / pomiaru przed flashowaniem. Możesz je zmienić w `idf.py menuconfig` → `Doorbell Gateway H2 UART`. Nie myl RX/TX patrząc od strony płytki i opisu przewodów.

## Budowanie M5Stick
1. Zainstaluj VS Code i rozszerzenie PlatformIO.
2. Otwórz katalog główny `thread-doorbell`.
3. `pio run -e m5stickc_plus2`
4. `pio run -e m5stickc_plus2 -t upload --upload-port /dev/ttyUSB0` (dopasuj port).
5. `pio device monitor -b 115200`.

PlatformIO używa płyty `m5stick-c` z nadpisaniem flash 8MB; przed flashowaniem zweryfikuj w logu wykryty flash i zgodność bootloadera z egzemplarzem Plus2. Zależności instalują się podczas pierwszego builda.

## Budowanie H2
1. Zainstaluj i aktywuj **ESP-IDF 5.5.x** (nie jest to projekt PlatformIO dla H2 — moduł OpenThread ESP-IDF jest rozwijany w natywnym toolchainie).
2. `cd firmware-h2`
3. `idf.py set-target esp32h2`
4. `idf.py menuconfig` → sprawdź **Doorbell Gateway H2 UART** oraz konfigurację OpenThread (Native radio / FTD).
5. `idf.py build`
6. Podłącz H2 przez jego USB-C, wejdź w tryb BOOT, jeśli konieczny, i `idf.py -p /dev/ttyACM0 flash monitor` (dopasuj port).

H2 wymaga ESP-IDF z obsługą `openthread`. Przy zmianie targetu `idf.py fullclean`, ponownie `set-target` i build. Nie edytuj generowanego `sdkconfig`, tylko `sdkconfig.defaults` albo menuconfig; bieżący `sdkconfig` jest ignorowany przez Git.

## Pierwsze uruchomienie
1. Uruchom M5Stick, połącz telefon/PC z otwartą siecią `Doorbell-Setup-XXXX`.
2. Otwórz `http://192.168.4.1/` (HTTPS nieobsługiwany).
3. Wprowadź SSID i hasło do sieci Wi-Fi.
4. Wprowadź **Thread Active Dataset TLVs (hex)** pobrane z routera Thread oraz adres IPv6 i port odbiornika (standardowo 61631).
5. Kliknij Zapisz. M5Stick restartuje się, zapisując konfigurację do NVS.
6. Po uruchomieniu H2 odbierze `DATASET` i `TARGET`, a po przyciśnięciu A dostanie `RING` i wyśle UDP.
7. Button B wywołuje dodatkowe żądanie `STATUS`.

### Ważne ograniczenia
- H2 firmware nigdy nie był fizycznie testowany w tej sesji; poprawność GPIO oraz zgodność z konkretną konfiguracją ESP-IDF 5.5 należy zweryfikować po pierwszym buildzie.
- Otwarte AP i panel HTTP **nie są bezpieczne** dla publicznego, niekontrolowanego dostępu. Wi-Fi hasło jest przechowywane w NVS, domyślnie bez NVS encryption.
- Docelowy UDP/IPv6 musi mieć poprawną trasę w Thread. Nie wystarczy wpisać IPv4 z LAN.
- Polecenie `DATASET` przesyła poufny klucz sieci Thread otwartym UART; panel AP ujawnia konfigurację bez hasła. Nie używaj w niezaufanym środowisku.
- Brak szyfrowania uwierzytelniającego zdarzeń UDP ponad wbudowaną ochronę linku Thread.
- Po zapisaniu pustego SSID urządzenie nie łączy się z Wi-Fi, pozostaje otwartym AP.

## Protokół UART
- `HELLO` → `HELLO H2-Thread-Doorbell v1`
- `STATUS` → `STATE disabled|detached|child|router|leader`
- `TARGET <IPv6> <port>` → `OK TARGET`
- `DATASET <TLVs-hex>` → `OK DATASET`
- `RING DOORBELL,<name>,<counter>` → `OK RING` / `ERR ...`
- UART 115200 8N1, ASCII, LF.

## Odbiór testowy UDP
Na linuxowym odbiorniku z działającą łącznością IPv6 do sieci Thread:

```bash
python3 tools/udp_receiver.py --bind :: --port 61631
```

W polu `IPv6 celu` wpisz **OSIĄGALNY adres IPv6 tego odbiornika**. Na węźle Thread może to być Mesh-Local EID. Nie ma tu bridge Wi-Fi ↔ Thread; H2 pracuje jako endpoint Thread.

## Źródła dokumentacji
- https://docs.m5stack.com/en/unit/Unit_Gateway_H2
- https://docs.m5stack.com/en/core/M5StickC%20PLUS2
- https://github.com/espressif/esp-idf/tree/release/v5.5/examples/openthread/ot_cli
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32h2/api-guides/openthread.html


## PlatformIO / Arduino-ESP32 3.x: błąd pins_arduino.h

Wersja 1.1 zawiera poprawkę `board_build.variant = esp32` w `platformio.ini`.
Na instalacjach z platformą Espressif 32 55.x i Arduino 3.3.x definicja
`board = m5stick-c` może wskazywać wariant bez pliku `pins_arduino.h`.
Standardowy wariant `esp32` dostarcza ten plik; GPIO modułu Grove
pozostają jawnie przypisane w kodzie jako TX=32, RX=33.

Po podmianie projektu usuń poprzedni build i zbuduj od początku:

```bash
rm -rf .pio
platformio run -e m5stickc_plus2
```

Plus2 ma 8 MB pamięci flash, ale dla bezpieczeństwa zachowano domyślną
tabelę partycji płytki; samo zwiększenie deklarowanej pojemności nie
powiększa automatycznie partycji aplikacji.

Ta poprawka dotyczy błędu wyszukiwania nagłówka; nie stanowi potwierdzenia
pełnej kompilacji na każdym wydaniu platformy 55.x.

## Matter onboarding extension (2026-10)

- New Matter pairing screen: press **B** to switch between status and PIN/QR. Pairing code is also visible on the WebGUI. The QR is served by the M5Stick itself at `/matter.svg`, with no remote QR service.
- `firmware-h2-matter/` contains an overlay to the official Espressif Matter-over-Thread `light_switch` example. Generate it using `./firmware-h2-matter/scripts/prepare.sh`, then build with ESP-IDF/ESP-Matter. See `firmware-h2-matter/README.md`.
- This is a **development proof-of-concept**, not a confirmed fully buildable U195 Matter binary: 2 MB flash may prevent the example from fitting, and internal H2 UART GPIO defaults require board-schematic verification. It uses the standard Espressif *test* setup credentials (PIN 20202021, manual 34970112332, QR `MT:Y.K9042C00KA0648G00`) and assumes you have not changed the factory provisioning.
- Firmware in `firmware-h2/` is **legacy OpenThread/UDP only**. It cannot commission Matter. To use the Matter PIN, install the Matter firmware from `firmware-h2-matter/` on H2.
- The Matter firmware exposes an On/Off switch with binding, **not yet a native Matter doorbell/Generic Switch event**. The controller must configure Matter bindings/ACLs after commissioning.
