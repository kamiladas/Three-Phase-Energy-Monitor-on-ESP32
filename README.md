# Energy Monitor ESP-IDF

Nowa, niezależna implementacja trójfazowego licznika energii dla ESP32 i ESP-IDF 6.x. Projekt Arduino z katalogu `Three-Phase-Energy-Monitor-on-ESP32-main` jest tylko materiałem referencyjnym — nie kopiujemy jego architektury ani bibliotek.

## Najszybsze uruchomienie QEMU i strony WWW

1. Otwórz w VS Code katalog `energy-monitor-idf`.
2. Wybierz **Terminal → New Terminal**.
3. W terminalu uruchom najprostszy wariant:

```powershell
.\tools\run-qemu.cmd
```

Możesz też uruchomić bezpośrednio skrypt PowerShell:

```powershell
.\tools\run-qemu.ps1
```

Jeśli Windows blokuje skrypty `.ps1`, korzystaj z wersji `.cmd`. Skrypt rozpoznaje już działający emulator i zamiast uruchamiać drugą instancję wyświetla adresy strony.

Skrypt kolejno:

- ładuje środowisko ESP-IDF 6.0;
- kompiluje firmware;
- tworzy obraz flash dla QEMU;
- tworzy trwałą emulowaną kartę SD 4 GB, jeśli jeszcze jej nie ma;
- uruchamia ESP32 w QEMU;
- przekierowuje port HTTP emulatora na port `8000` komputera.

UART jest widoczny w tym samym terminalu VS Code. Nie zamykaj terminala, jeśli emulator ma dalej działać. QEMU kończy się skrótem `Ctrl+A`, a następnie `X`.

Po pojawieniu się komunikatu o uruchomieniu serwera otwórz:

- dashboard: <http://127.0.0.1:8000/>
- statystyki: <http://127.0.0.1:8000/statistics>
- status SD: <http://127.0.0.1:8000/api/v1/storage/status>
- agregaty dobowe: `/api/v1/history/aggregate?from=UNIX_OD&to=UNIX_DO`
- eksport CSV: <http://127.0.0.1:8000/api/v1/history/export>

Obraz karty znajduje się w `build/sd_image.bin`. Ponowne uruchomienie QEMU nie kasuje zapisanej historii.

## Jak płyną dane

```text
sample_source_adc ─┐
                   ├─> kolejka ramek ─> measurement_core ─> snapshot_store ─> API ─> web_ui
sample_source_sim ─┘                         │
                                             ├─> history_store (krótki bufor RAM)
                                             └─> sd_store (historia trwała na SD)

sieć ─> time_service (SNTP/TZ) ─> znaczniki czasu zapisu SD
```

W buildzie wybierane jest dokładnie jedno źródło próbek. Silnik pomiarowy nie wie, czy dane pochodzą z ADC, czy z generatora.

## Aktualna struktura projektu

ESP-IDF naturalnie dzieli aplikację na komponenty. Każdy katalog w `components` jest osobną biblioteką z własnym plikiem `CMakeLists.txt`.

| Katalog | Odpowiedzialność |
|---|---|
| `main/` | Uruchomienie aplikacji, zadania FreeRTOS i połączenie komponentów. |
| `components/measurement_core/` | Czysta matematyka: offsety, RMS, P/S/Q, PF, Hz, energia i flagi jakości. |
| `components/sample_source/` | Wspólny kontrakt dostarczania ramek próbek. |
| `components/sample_source_adc/` | Rzeczywisty ADC continuous/DMA na ESP32. |
| `components/sample_source_sim/` | Deterministyczne próbki używane w QEMU i testach. |
| `components/board_config/` | Pinout płytki i ustawienia sprzętowe. |
| `components/snapshot_store/` | Ostatni spójny wynik pomiaru. |
| `components/history_store/` | Krótka historia w RAM oraz próbki oscyloskopu. |
| `components/sd_store/` | Kolejka i zapis CSV na karcie SD, agregacja oraz eksport. |
| `components/time_service/` | SNTP, czas UTC, reguła strefy TZ i jakość czasu. |
| `components/web_ui/` | Serwer HTTP, endpointy API i osadzone pliki HTML/CSS/JS. |
| `host_tests/` | Testy matematyki uruchamiane na komputerze bez ESP32. |
| `tools/` | Skrypty deweloperskie, między innymi uruchamianie QEMU. |
| `build/` | Pliki wygenerowane. Nie edytujemy ich ręcznie. |

Pliki `.hpp` są publicznym kontraktem komponentu. Pliki `.cpp` zawierają implementację. `main/app_main.cpp` powinien jedynie tworzyć zadania i łączyć gotowe moduły — nie powinien zawierać matematyki ani HTML.

## Zastosowana organizacja plików

Projekt używa układu `src + include` wewnątrz komponentów, zachowując poprawną architekturę ESP-IDF:

```text
energy-monitor-idf/
├── main/
│   ├── CMakeLists.txt
│   └── app_main.cpp                 # wyłącznie składanie i start aplikacji
├── components/
│   ├── measurement_core/
│   │   ├── include/energy/measurement_engine.hpp
│   │   ├── include/energy/measurement_types.hpp
│   │   ├── src/measurement_engine.cpp
│   │   ├── src/frequency_estimator.cpp
│   │   └── CMakeLists.txt
│   ├── sample_source/
│   │   └── include/energy/sample_source.hpp
│   ├── sample_source_adc/
│   │   ├── include/energy/adc_sample_source.hpp
│   │   └── src/adc_sample_source.cpp
│   ├── sample_source_sim/
│   │   ├── include/energy/simulated_sample_source.hpp
│   │   └── src/simulated_sample_source.cpp
│   ├── storage/
│   │   ├── include/energy/energy_store.hpp
│   │   ├── include/energy/history_store.hpp
│   │   └── src/*.cpp
│   ├── time_service/
│   │   ├── include/energy/time_service.hpp
│   │   └── src/time_service.cpp
│   ├── network_manager/
│   │   ├── include/energy/network_manager.hpp
│   │   └── src/*.cpp
│   ├── api_server/
│   │   ├── include/energy/api_server.hpp
│   │   └── src/*.cpp
│   └── web_ui/
│       └── assets/                  # HTML, CSS i JS bez kodu C++ serwera
├── tests/
├── tools/
└── README.md
```

Nie tworzymy jednego wielkiego katalogu `src` dla całej aplikacji. Podział `components/<moduł>/include/energy + src` daje czytelne klasy i nagłówki, a dodatkowo wymusza granice zależności pomiędzy modułami. Nagłówki projektu dołączamy zawsze jako `#include "energy/nazwa.hpp"`.

## Co dokładnie jest symulowane w webie

Frontend niczego nie symuluje i nie oblicza sztucznych wyników. Pobiera dane z `/api/v1` i tylko je formatuje oraz rysuje.

W wariancie QEMU symulowane są:

- surowe próbki sześciu kanałów `V1/I1/V2/I2/V3/I3`;
- przebiegi trzech faz przesunięte o 120°;
- domowy profil obciążenia osobny dla każdej fazy;
- szum i offset ADC;
- demonstracyjna przerwa próbek/zanik zasilania;
- fizyczny układ ADC — QEMU go nie emuluje, zastępuje go `sample_source_sim`.

Nie są symulowane:

- obliczenia RMS, mocy, PF, częstotliwości i energii — wykonuje je prawdziwy `measurement_core`;
- zadania, kolejki i planowanie FreeRTOS;
- serwer HTTP i API działające wewnątrz firmware;
- zapis i odczyt FAT/CSV — QEMU używa prawdziwego sterownika SD/MMC na emulowanym obrazie blokowym 4 GB;
- agregacja danych SD i eksport CSV;
- HTML/CSS/JavaScript strony;
- wykrywanie brakujących próbek oraz flagi jakości.

SNTP jest prawdziwym klientem sieciowym. Jeśli emulator nie uzyska odpowiedzi NTP, rekord otrzymuje jakość czasu `UNKNOWN`, a strona pokazuje czas względny zamiast wymyślonej daty. Na sprzęcie źródło `sample_source_sim` zostaje zastąpione przez `sample_source_adc`; pozostała ścieżka obliczeń i prezentacji pozostaje taka sama.

## Pinout rzeczywistej płytki

| Sygnał | GPIO | ESP32 ADC1 |
|---|---:|---:|
| L1 napięcie | 36 | kanał 0 |
| L1 prąd | 33 | kanał 5 |
| L2 napięcie | 39 | kanał 3 |
| L2 prąd | 32 | kanał 4 |
| L3 napięcie | 34 | kanał 6 |
| L3 prąd | 35 | kanał 7 |

## Testy hostowe

```powershell
cmake -S host_tests -B host_tests/build
cmake --build host_tests/build
ctest --test-dir host_tests/build --output-on-failure
```
