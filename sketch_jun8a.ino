#include "ESP32CaptivePortal.h"
#include "FS.h"
#include "SPIFFS.h"
ESP32CaptivePortal captivePortal;
TaskHandle_t myTask;


void setup() {
   if (!SPIFFS.begin(true)) {
    Serial.println("Błąd inicjalizacji SPIFFS");
    return;
  }
  Serial.println("SPIFFS zainicjalizowany pomyślnie");
  delay(100);
  captivePortal.begin();

   xTaskCreatePinnedToCore(
    loop_second,        // Funkcja zadania
    "loop_second",      // Nazwa zadania
    10000,              // Rozmiar stosu zadania (w słowach)
    NULL,               // Parametr przekazywany do zadania (brak w tym przypadku)
    1,                  // Priorytet zadania (im niższa wartość, tym wyższy priorytet)
    &myTask,            // Uchwyt do zadania
    0                   // Numer rdzenia, na którym ma być uruchomione zadanie (0 - CORE_0)
  );
 
}

void loop_second(void *parameter) {
  while (1) { 


       delay(5000);
       //captivePortal.readConfig("/config.json");
      // captivePortal.measureData(1);
       Serial.println("\n");      
     }

}

void loop() {
  captivePortal.loop();
  // Twój kod do podglądu danych licznika energii
}

