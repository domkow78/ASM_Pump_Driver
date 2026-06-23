# ASM_Pump_Driver
Simple driver for ASM type water pump

## Opis Aplikacji

Aplikacja dla Arduino Nano steruje przekaźnikiem pompy wody na podstawie odczytu z czujnika prądu ACS724 oraz prezentuje bieżący stan na wyświetlaczu OLED SSD1306 (I2C).

### Funkcjonalności:
- Odczyt sygnału analogowego z ACS724 (`A0`) i przeliczenie na napięcie.
- Sterowanie przekaźnikiem (`D8`) z użyciem histerezy (oddzielny próg załączenia i wyłączenia).
- Wyświetlanie na OLED:
	- aktualnego napięcia z wejścia analogowego,
	- stanu wyjścia przekaźnika (`ON`/`OFF`).

### Podłączenie:
- Przekaźnik -> `D8`
- Czujnik ACS724 (wyjście analogowe) -> `A0`
- OLED SSD1306 (I2C) -> `SDA/SCL`

### Działanie:
1. Aplikacja inicjalizuje wyświetlacz OLED oraz wyjście przekaźnika.
2. W pętli głównej odczytuje sygnał analogowy z ACS724 i przelicza go na napięcie.
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

### Wymagane biblioteki:
- `Wire.h`
- `Adafruit_GFX.h`
- `Adafruit_SSD1306.h`

### Użycie:
Aby uruchomić aplikację:
1. Podłącz elementy zgodnie z sekcją „Podłączenie”.
2. Wgraj [src/asm_pump_driver/asm_pump_driver.ino](src/asm_pump_driver/asm_pump_driver.ino) na Arduino Nano.
3. Obserwuj napięcie i stan przekaźnika na OLED.
4. Dostosuj progi histerezy w kodzie do rzeczywistego sygnału z ACS724.
