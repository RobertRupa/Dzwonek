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


## Poprawka UDP/DNS i Wi-Fi

W targetcie `m5stickc_plus2_rcp` rozwiązywanie DNS i wysyłka UDP wykonują się w osobnym zadaniu FreeRTOS, dzięki czemu timeout DNS nie blokuje obsługi przycisków ani LCD. Maksymalnie cztery wiadomości oczekują w kolejce; nadmiar jest odrzucany, bez blokowania pętli. Wpisanie adresu IPv4 pomija DNS. Preferuj docelowy adres IPv4 przy testach zamiast hostname.

Hasło WPA ma 8–63 znaków. Pozostawienie pustego pola zachowuje hasło tylko dla tego samego SSID; zmiana SSID wymaga nowego hasła. Dla sieci otwartej użyj dedykowanego przełącznika. Zmiany walidowane są przed zapisem do NVS.

Log `DNS failed: verify destination` wskazuje problem z nazwą odbiorcy lub DNS. Nie potwierdza błędnego hasła do Wi-Fi. Dla diagnostyki sprawdź status połączenia i odbiorcę w WebGUI.

Weryfikacja kompilacji i testy na rzeczywistym sprzęcie pozostają do wykonania.

## WebGUI na żywo i powiadomienia

Panel odświeża `/api/status` co 700 ms, bez przeładowywania całej strony. Zawiera licznik ramek Spinel/HDLC, stan RCP i licznik naciśnięć dzwonka. Po kliknięciu **Włącz dźwięk przeglądarki** używa Web Audio API do odtwarzania dźwięku przy wzroście licznika naciśnięć. Otwarcie karty bez wcześniejszego kliknięcia nie gwarantuje dźwięku — Chrome wymaga gestu użytkownika. Dźwięk dotyczy tego konkretnego otwartego WebGUI i działa, dopóki działa strona i polling.

PIN dostępny w WebGUI jest **lokalnym, losowo wygenerowanym kodem identyfikacyjnym/testowym** zapisywanym w NVS. **Nie jest PIN-em ani kodem QR Matter**. Podstawowy firmware H2 RCP nie ma własnego Matter commissioning.

## Odbiór UDP

Poprzedni skrypt `tools/udp_receiver.py` nasłuchiwał tylko IPv6. Obecny target StickC nadaje po IPv4; zaktualizowany odbiornik przyjmuje obydwie rodziny:

```bash
python3 tools/udp_receiver.py --port 61631
```

W konfiguracji dzwonka ustaw IPv4 komputera uruchamiającego odbiornik oraz port 61631. Do broadcastu w typowej podsieci `192.168.68.0/24` służy `192.168.68.255`, ale konkretna maska i polityka Wi-Fi/VLAN mogą się różnić. Sprawdź ruch przez `sudo tcpdump -ni any udp port 61631` oraz reguły zapory (np. UFW). Komunikat `UDP sent (unconfirmed)` nie gwarantuje dostarczenia.

## Diagnostyka H2 na konsoli USB StickC Plus2

Po uruchomieniu `pio device monitor -b 115200` firmware zapisuje do portu Serial: początek sondowania, GPIO i baudrate, ramkę TX Spinel RESET, odebrane ramki HDLC w HEX, błędne CRC i timeout 2000 ms. Przyciskiem B można ponowić sondowanie. Komunikat `RX valid HDLC` potwierdza transport, **nie** gotowość sieci Thread/Matter. Przy braku ramek sprawdź RX/TX, GND, piny U195 i tryb RCP UART zamiast RCP USB.

## Matter QR / kod manualny

Lokalny, losowy 11-cyfrowy kod usunięto. Strona wyświetla teraz importowany kod manualny i QR payload rzeczywistego endpointu Matter. Sam ESP32-H2 jako RCP nie ma aplikacji Matter ani danych commissioning; import/wyświetlanie nie umożliwia parowania StickC. Kod QR i manualny muszą odpowiadać setup passcode/discriminator/verifier działającego endpointu.
