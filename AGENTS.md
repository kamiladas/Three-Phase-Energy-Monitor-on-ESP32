# Energy Monitor IDF — decyzje projektowe

Obowiązują również nadrzędne zasady projektu z `../AGENTS.md`. Ten plik zapisuje aktualny stan i decyzje potrzebne przy dalszej pracy oraz testach na sprzęcie.

## Źródło danych

- Frontend nigdy nie generuje sztucznych pomiarów. Pobiera wyłącznie DTO z `/api/v1`.
- QEMU używa deterministycznego `sample_source_sim`; build sprzętowy używa `sample_source_adc` z ADC continuous/DMA.
- Wyniki RMS, mocy, PF, częstotliwości i energii powstają w silniku pomiarowym. Web nie wykonuje matematyki pomiarowej, tylko agregację prezentacyjną otrzymanego zakresu.
- Krótki aktualny przebieg V/I pochodzi z bufora RAM i `/api/v1/waveform`. Nie zapisujemy stale surowych próbek 4000 ramek/s na SD.

## Statystyki i odpowiedzialność frontendu

Jedna zmiana filtra oznacza jedno żądanie zakresu historii. Po otrzymaniu danych rysowanie, nakładanie faz, legenda, dymki oraz wyszukiwanie punktu osiągnięcia energii odbywają się lokalnie w przeglądarce.

- dzień: maksymalnie 300 agregatów 5-minutowych, składanych w słupki godzinowe;
- tydzień: 167–169 agregatów godzinowych, składanych w 7 dni;
- miesiąc: maksymalnie 249 agregatów 3-godzinnych (miesiąc zmiany czasu), składanych w dni;
- rok: 12 agregatów miesięcznych, bez pionowego wyszukiwania czasu osiągnięcia kWh.

Pionowy dymek `kWh → czas osiągnięcia` działa dla dnia, tygodnia i miesiąca, dla L1/L2/L3 oraz sumy. Pokazuje pierwszy rzeczywiście otrzymany agregat osiągający poziom; nie interpoluje czasu. Klikalna legenda jest jedynym przełącznikiem widoczności serii.

Selektor statystyk udostępnia energię `[kWh]`, średnią moc czynną `[W]` i średnią częstotliwość `[Hz]`. Częstotliwości faz nie sumuje się.

## Historia SD

Aktualny zapis CSV jest etapem przejściowym. Docelowo `history_store` ma utrzymywać partycje i agregaty przyrostowo:

```text
/history/raw/YYYY/MM/DD.csv
/history/5min/YYYY/MM/DD.csv
/history/hour/YYYY/MM.csv
/history/day/YYYY.csv
/history/month/summary.csv
```

API czyta najmniejszy właściwy plik zamiast skanować całą historię. Nie dodajemy SQLite bez pomiaru wykazującego realną przewagę; preferowane są dopisywalne pliki, wersjonowany schemat, CRC/wykrywanie uszkodzeń i atomowa rotacja.

Build QEMU może utworzyć na emulowanej SD deterministyczny roczny zestaw godzinowy do testowania filtrów. Jest to fixture magazynu, a nie dane generowane w JavaScript, i musi pozostać wyłączony w buildzie produkcyjnym.

## Testy sprzętowe

- Nie zmieniać pinoutu i konfiguracji kanałów bez jawnej decyzji użytkownika.
- Najpierw porównać ADC/DMA z przyrządem referencyjnym na obciążeniu rezystancyjnym.
- Zmierzyć przesunięcie V/I, clipping, rzeczywistą częstotliwość ramek, utracone bloki i obciążenie zadań.
- Awaria SD, sieci lub UI nie może zatrzymać próbkowania ani integracji energii.
