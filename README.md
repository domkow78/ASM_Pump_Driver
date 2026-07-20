# ASM_Pump_Driver
Simple driver for ASM type water pump

## Opis Aplikacji

Aplikacja dla Arduino Nano steruje przekaźnikiem pompy wody na podstawie odczytu z czujnika prądu ACS712 oraz prezentuje bieżący stan na wyświetlaczu OLED SSD1306 (I2C).

### Funkcjonalności:
- Odczyt sygnału analogowego z ACS712 (`A0`) i przeliczenie na prąd.
- Automatyczny pomiar napięcia zasilania Arduino i korekcja przeliczenia ADC.
- Uśrednianie pomiaru `Vcc`, aby ograniczyć wahania wskazań.
- Automatyczna kalibracja `zero offset` przy starcie układu.
- Sterowanie przekaźnikiem (`D8`) z użyciem histerezy (oddzielny próg załączenia i wyłączenia).
- Wyświetlanie na OLED:
	- aktualnego prądu w amperach,
	- napięcia zasilania `Vcc`,
	- napięcia `zero offset`,
	- stanu wyjścia przekaźnika (`ON`/`OFF`).

### Podłączenie:
- Przekaźnik -> `D8`
- Czujnik ACS712 (wyjście analogowe) -> `A0`
- OLED SSD1306 (I2C) -> `SDA/SCL`

### Działanie:
1. Aplikacja inicjalizuje wyświetlacz OLED oraz wyjście przekaźnika.
2. Przy starcie układ wyznacza `zero offset`, czyli napięcie wyjściowe ACS712 przy zerowym prądzie.
3. W pętli głównej odczytuje sygnał analogowy z ACS712, odejmuje `zero offset` i przelicza wynik na ampery.
4. Logika histerezy działa na dwóch progach prądu:
	 - `threshold_on` — próg załączenia przekaźnika,
	 - `threshold_off` — próg wyłączenia przekaźnika.
5. Dzięki histerezie przekaźnik nie „klapie” przy sygnale blisko progu.
6. OLED pokazuje bieżący prąd, `Vcc`, `zero offset` oraz stan przekaźnika (`ON`/`OFF`).

### Strojenie progów histerezy:
Progi są teraz wyrażone w amperach, dlatego należy dobrać je do rzeczywistego poboru pompy:
- Zwiększ `threshold_on`, jeśli przekaźnik załącza się przy zbyt małym prądzie.
- Zmniejsz `threshold_off`, jeśli przekaźnik wyłącza się zbyt wcześnie.
- Zachowaj warunek: `threshold_on > threshold_off`.

### Zero offset i czułość ACS712:
`Zero offset` to napięcie wyjściowe czujnika przy zerowym prądzie. Dla ACS712 jest to zwykle około połowy napięcia zasilania. Kod mierzy tę wartość automatycznie przy starcie, gdy pompa powinna być wyłączona.

Przeliczenie na prąd odbywa się według wzoru:
- `I = (Vsensor - Vzero) / sensitivity`

Aktualnie w kodzie ustawiono:
- `acs712SensitivityVoltsPerAmp = 0.100`

To odpowiada wersji `ACS712 20A`. Jeśli używasz innej wersji, ustaw odpowiednio:
- `5A` → `0.185 V/A`
- `20A` → `0.100 V/A`
- `30A` → `0.066 V/A`

### Kalibracja pomiaru ADC:
Kod automatycznie mierzy napięcie zasilania Arduino (`Vcc`) z użyciem wewnętrznego źródła odniesienia mikrokontrolera i wykorzystuje ten wynik do przeliczenia odczytu ADC. Przy zasilaniu z USB napięcie odniesienia często nie wynosi idealnie `5.000 V`, więc taka korekcja ogranicza błąd wskazania na OLED.

Dla pierwszego uruchomienia zmierzono:
- zasilanie Arduino z USB: `4.737 V`,
- napięcie na wejściu ADC: `2.367 V`.

Układ powinien teraz sam reagować na zmianę napięcia zasilania i odpowiednio korygować przeliczanie wartości z wejścia `A0`.

Ponieważ wewnętrzne źródło odniesienia AVR nie ma idealnie `1.100 V`, w kodzie dodano kalibrację stałej:
- `bandgapReferenceMillivolts = 1068`

Wartość została wyliczona z pomiaru:
- rzeczywiste `Vcc`: `4.689 V`,
- wskazanie OLED przed kalibracją: `4.829 V`.

Wzór do kolejnej korekty:
- `nowa_stala = stara_stala * (Vcc_rzeczywiste / Vcc_wyswietlane)`

Dla tego przypadku:
- `1068 ≈ 1100 * (4.689 / 4.829)`

Uwagi:
- mechanizm działa dla klasycznego Arduino Nano z mikrokontrolerem `ATmega328P`,
- dokładność zależy od tolerancji wewnętrznego źródła odniesienia, więc do pomiarów dokładnych nadal warto porównać wynik z multimetrem,
- `Vcc` używane do przeliczenia ADC jest uśredniane w oknie 8 ostatnich pomiarów,
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
