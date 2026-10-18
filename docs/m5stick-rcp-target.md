# M5StickC Plus2 — niezależny target diagnostyczny H2 RCP

Target `m5stickc_plus2_rcp` jest odseparowany od istniejącej aplikacji `m5stickc_plus2`. Wspiera oficjalny firmware `ot_rcp` z H2 U195 na poziomie diagnostyki transportu Spinel/HDLC; **nie zawiera OpenThread Host, Matter ani obsługi sieci Thread**.

## Uruchomienie

```bash
cd ~/Documents/Dzwonek
git pull origin master
pio run -e m5stickc_plus2_rcp
pio run -e m5stickc_plus2_rcp -t upload --upload-port /dev/ttyUSB0
pio device monitor -b 115200
```

GPIO32 (M5 TX) → GPIO23 (H2 RX), GPIO33 (M5 RX) ← GPIO24 (H2 TX), GND wspólne. Należy potwierdzić rzeczywiste wyprowadzenia U195. Spinel UART0, 460800 8N1, HDLC. Naciśnięcie B wysyła Spinel RESET i status `HDLC OK` oznacza poprawną ramkę zwrotną, nie aktywną sieć Thread.

Button A albo WebGUI `/ring` wysyła komunikat `DOORBELL,StickPlus2,<licznik>` UDP przez **Wi-Fi** do skonfigurowanego hosta i portu, nie przez H2. Odbiór: `python3 tools/udp_receiver.py --bind 0.0.0.0 --port 61631`. Wywołanie zwraca co najwyżej sukces lokalnego nadania UDP, nie potwierdzenie odbioru.

Panel: otwarte AP `Doorbell-RCP-XXXX`, http://192.168.4.1/ (brak uwierzytelnienia; tylko sieć testowa). Konfiguracja nowego targetu jest izolowana w osobnej przestrzeni Preferences `doorbell-rcp`.

**Ograniczenie sprzętowe/architektoniczne:** skompilowanie Matter over Thread na M5StickC Plus2 (klasyczny ESP32, 8 MB flash) wymaga osobnego portu ESP-IDF, OpenThread Host, CHIP i analizy RAM/flash. Nie można uzyskać Matter samym włączeniem binarnego RCP. Wybierz zachowany target `light_switch` na H2, jeśli zależy Ci na Matter bez budowania hosta na Sticku.
