# ASM_Pump_Driver
Simple driver for ASM type water pump

## Opis Aplikacji

Aplikacja dla Arduino Nano steruje przekaźnikiem pompy wody na podstawie pomiaru prądu przemiennego (`AC RMS`) z czujnika ACS724 ±2.5A (Pololu 4040) oraz prezentuje bieżący stan na wyświetlaczu OLED SSD1306 (I2C).

### Funkcjonalności:
- Odczyt sygnału analogowego z ACS724 (`A0`) i przeliczenie na prąd skuteczny `RMS`.
- Automatyczny pomiar napięcia zasilania Arduino i korekcja przeliczenia ADC.
- Uśrednianie pomiaru `Vcc`, aby ograniczyć wahania wskazań.
- Dynamiczne wyznaczanie punktu zerowego (`Vzero`) czujnika w każdym oknie pomiarowym.
- Martwa strefa eliminująca resztkowy szum przy zerowym prądzie.
- Sterowanie przekaźnikiem (`D8`) z użyciem histerezy (oddzielny próg załączenia i wyłączenia).
- Wyświetlanie na OLED:
	- prądu skutecznego `Irms` w amperach,
	- napięcia zasilania `Vcc`,
	- stanu wyjścia przekaźnika (`ON`/`OFF`).

### Podłączenie:
- Przekaźnik -> `D8`
- Czujnik ACS724 ±2.5A (wyjście analogowe) -> `A0`
- OLED SSD1306 (I2C) -> `SDA/SCL`

### Działanie:
1. Aplikacja inicjalizuje wyświetlacz OLED oraz wyjście przekaźnika.
2. W pętli głównej przez okno ~`100 ms` (5 okresów `50 Hz`) szybko próbkuje sygnał z ACS724.
3. Z próbek wyznacza dynamicznie `Vzero` (średnia) oraz prąd skuteczny `Irms`.
4. Logika histerezy działa na dwóch progach prądu:
	 - `threshold_on` — próg załączenia przekaźnika,
	 - `threshold_off` — próg wyłączenia przekaźnika.
5. Dzięki histerezie przekaźnik nie „klapie” przy sygnale blisko progu.
6. OLED pokazuje bieżący prąd `Irms`, `Vcc` oraz stan przekaźnika (`ON`/`OFF`).

### Strojenie progów histerezy:
Progi są wyrażone w amperach `RMS`, dlatego należy dobrać je do rzeczywistego poboru pompy:
- Zwiększ `threshold_on`, jeśli przekaźnik załącza się przy zbyt małym prądzie.
- Zmniejsz `threshold_off`, jeśli przekaźnik wyłącza się zbyt wcześnie.
- Zachowaj warunek: `threshold_on > threshold_off`.

### Pomiar AC RMS, Vzero i czułość ACS724:
Czujnik ACS724 przy zerowym prądzie daje na wyjściu napięcie około połowy zasilania (`~Vcc/2`). Przy prądzie przemiennym sygnał oscyluje wokół tego punktu, dlatego pojedynczy odczyt nie ma sensu — liczona jest wartość skuteczna `RMS`.

Punkt zerowy `Vzero` jest wyznaczany dynamicznie jako średnia próbek w oknie pomiarowym, co eliminuje dryf offsetu. Prąd skuteczny liczony jest według wzoru:
- `Irms = sqrt(mean(V^2) - mean(V)^2) / sensitivity`

Aktualnie w kodzie ustawiono:
- `acs724SensitivityVoltsPerAmp = 0.400`

To odpowiada wersji `ACS724 ±2.5A (Pololu 4040)` o czułości `400 mV/A`. Dodatkowo martwa strefa `currentDeadZoneAmps = 0.05` wymusza wskazanie `0 A` poniżej tego progu, aby wyeliminować szum.

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
3. Obserwuj prąd `Irms` i stan przekaźnika na OLED.
4. Dostosuj progi histerezy w kodzie do rzeczywistego poboru prądu pompy.
