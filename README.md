# ASM_Pump_Driver
Simple driver for ASM type water pump

## Opis Aplikacji

Aplikacja dla Arduino Nano steruje przekaźnikiem pompy wody na podstawie odczytu z czujnika prądu ACS712 oraz prezentuje bieżący stan na wyświetlaczu OLED SSD1306 (I2C).

### Funkcjonalności:
- Odczyt sygnału analogowego z ACS712 (`A0`) i przeliczenie na napięcie.
- Automatyczny pomiar napięcia zasilania Arduino i korekcja przeliczenia ADC.
- Sterowanie przekaźnikiem (`D8`) z użyciem histerezy (oddzielny próg załączenia i wyłączenia).
- Wyświetlanie na OLED:
	- aktualnego napięcia z wejścia analogowego,
	- napięcia zasilania `Vcc`,
	- stanu wyjścia przekaźnika (`ON`/`OFF`).

### Podłączenie:
- Przekaźnik -> `D8`
- Czujnik ACS712 (wyjście analogowe) -> `A0`
- OLED SSD1306 (I2C) -> `SDA/SCL`

### Działanie:
1. Aplikacja inicjalizuje wyświetlacz OLED oraz wyjście przekaźnika.
2. W pętli głównej odczytuje sygnał analogowy z ACS712 i przelicza go na napięcie.
3. Logika histerezy działa na dwóch progach:
	 - `threshold_on` — próg załączenia przekaźnika,
	 - `threshold_off` — próg wyłączenia przekaźnika.
4. Dzięki histerezie przekaźnik nie „klapie” przy sygnale blisko progu.
5. OLED pokazuje bieżące napięcie oraz stan przekaźnika (`ON`/`OFF`).

### Strojenie progów histerezy:
Poziom sygnału z czujnika nie jest jeszcze docelowo ustalony, dlatego progi należy dobrać eksperymentalnie:
- Zwiększ `threshold_on`, jeśli przekaźnik załącza się zbyt wcześnie.
- Zmniejsz `threshold_off`, jeśli przekaźnik wyłącza się zbyt szybko.
- Zachowaj warunek: `threshold_on > threshold_off`.

### Kalibracja pomiaru ADC:
Kod automatycznie mierzy napięcie zasilania Arduino (`Vcc`) z użyciem wewnętrznego źródła odniesienia mikrokontrolera i wykorzystuje ten wynik do przeliczenia odczytu ADC. Przy zasilaniu z USB napięcie odniesienia często nie wynosi idealnie `5.000 V`, więc taka korekcja ogranicza błąd wskazania na OLED.

Dla pierwszego uruchomienia zmierzono:
- zasilanie Arduino z USB: `4.737 V`,
- napięcie na wejściu ADC: `2.367 V`.

Układ powinien teraz sam reagować na zmianę napięcia zasilania i odpowiednio korygować przeliczanie wartości z wejścia `A0`.

Uwagi:
- mechanizm działa dla klasycznego Arduino Nano z mikrokontrolerem `ATmega328P`,
- dokładność zależy od tolerancji wewnętrznego źródła odniesienia, więc do pomiarów dokładnych nadal warto porównać wynik z multimetrem,
- bieżące `Vcc` jest pokazywane na OLED i w `Serial`.

### Wymagane biblioteki:
- `Wire.h`
- `Adafruit_GFX.h`
- `Adafruit_SSD1306.h`

### Użycie:
Aby uruchomić aplikację:
1. Podłącz elementy zgodnie z sekcją „Podłączenie”.
2. Wgraj [src/asm_pump_driver/asm_pump_driver.ino](src/asm_pump_driver/asm_pump_driver.ino) na Arduino Nano.
3. Obserwuj napięcie i stan przekaźnika na OLED.
4. Dostosuj progi histerezy w kodzie do rzeczywistego sygnału z ACS712.
