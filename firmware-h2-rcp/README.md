# ESP32-H2 U195 — natywny OpenThread RCP (Spinel)

Ten target jest osobny od `firmware-h2-matter/` (`light_switch`). Kopiuje oficjalny przykład ESP-IDF 5.5 `examples/openthread/ot_rcp` do lokalnego, ignorowanego katalogu `generated/ot_rcp` i ustawia parametry płytki U195. **Nie wymaga ESP-Matter, Pigweed ani GN.**

## Przygotowanie / kompilacja

```bash
source "$HOME/esp/esp-idf/export.sh"
cd ~/Documents/Dzwonek
bash ./firmware-h2-rcp/scripts/prepare.sh
cd firmware-h2-rcp/generated/ot_rcp
idf.py set-target esp32h2
idf.py build
idf.py size
grep -E 'CONFIG_OPENTHREAD_RCP_UART=|CONFIG_OPENTHREAD_UART_PIN_MANUAL=|CONFIG_OPENTHREAD_UART_[RT]X_PIN=|CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=|CONFIG_ESPTOOLPY_FLASHSIZE_2MB=' sdkconfig
```

Po sprawdzeniu sprzętowego mapowania GPIO, wyboru transportu UART oraz wielkości obrazu:

```bash
idf.py -p /dev/ttyACM0 flash
```

Nie używaj `monitor` jako terminala do wydawania poleceń Spinel. RCP jest urządzeniem binarnym i potrzebuje hosta OpenThread. Skrypt nie nadpisuje istniejącego `generated/ot_rcp`, by nie usuwać lokalnego `sdkconfig` bez ostrzeżenia.

## UART / płytka

- M5StickC Plus2 GPIO32 TX → H2 GPIO23 RX (Grove)
- M5StickC Plus2 GPIO33 RX ← H2 GPIO24 TX (Grove)
- GND ↔ GND
- UART0, 460800 8N1, bez sprzętowego flow control (ustawienie oficjalnego przykładu).
- USB Serial/JTAG do flashowania/diagnostyki, UART0 do Spinel.
- Flash U195 H2-MINI-1-N2: 2 MB; sprawdź `idf.py size` oraz `sdkconfig` po `set-target`.

**Weryfikacja na sprzęcie jest konieczna:** przypisanie GPIO23/GPIO24 do złącza U195 pochodzi z wcześniejszej konfiguracji projektu; nie zostało potwierdzone pomiarem w tej zmianie.

## Ważna niekompatybilność

Obecne firmware M5StickC Plus2 oczekuje tekstowych komend `HELLO`, `RING`, `STATUS`, `MATTER INFO` z H2. **Nie współpracuje z natywnym RCP.** Po wgraniu RCP obecne funkcje dzwonka przez Thread/Matter oraz ekran kodu parowania nie będą działać, dopóki główny procesor nie uzyska **hosta OpenThread + Matter** (lub osobnego hosta/BR przekazującego zdarzenia). Samo przełączenie szybkości UART nie wystarcza.

RCP nie ma własnego Matter commissioning, kodu PIN ani QR; interfejs radiowy jest kontrolowany przez hosta Spinel. Docelowa architektura hosta wymaga osobnej implementacji i doboru pamięci procesora głównego.

## Pozostawiony wariant

`firmware-h2-matter/scripts/prepare.sh` nadal generuje odrębny target `light_switch`; wymaga ESP-Matter. Nie uruchamiaj obu firmware jednocześnie na jednym H2. Każdy flash zastępuje poprzedni obraz.
