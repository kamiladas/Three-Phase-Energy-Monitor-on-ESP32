unsigned long timeout = 3000000;


void setup() {
  Serial.begin(115200);    // Inicjalizacja portu szeregowego
  Serial.println("Serial started");
}

void loop() {
  float rmsVoltage = measureVoltageRMS()-4.7; // Wywołanie funkcji pomiaru napięcia RMS
  Serial.print("RMS Voltage: ");
  Serial.print(rmsVoltage, 2); // Wyświetlenie napięcia RMS z zaokrągleniem do 3 miejsc po przecinku
  Serial.print(" V\t");
  Serial.print("Freq: \t");
  Serial.print(measureFrequency(),0);
  Serial.print("Hz");
  Serial.print("\n");
}

float measureVoltageRMS()
{
  bool timeoutOccurred = false; // Flaga oznaczająca wystąpienie timeoutu
  unsigned long periodSum = 0;
  int sampleCount = 200; // liczba próbek do uśrednienia
  float GetMax = 0;

  for (int i = 0; i < sampleCount; i++)
  {
    unsigned long startWaitTime = micros();
    // Poczekaj na zmianę stanu z niskiego na wysoki

    while (analogRead(32) > 2200)
    {
      if (micros() - startWaitTime > timeout)
      {
        timeoutOccurred = true; // Ustaw flagę timeoutu
        break;
      }
      else
      {
        delay(2);
        if (analogRead(32) > GetMax)
        {
          GetMax = analogRead(32);
        }
        if (analogRead(32) > GetMax)
        {
          GetMax = analogRead(32);
        }
      }
    }

    if (timeoutOccurred)
    {
      break; // Przerwij pętlę, jeśli wystąpił timeout
    }

    // Poczekaj na zmianę stanu z wysokiego na niski
    while (analogRead(32) < 2100)
    {
      if (micros() - startWaitTime > timeout)
      {
        timeoutOccurred = true; // Ustaw flagę timeoutu
        break;
      }
    }

    if (timeoutOccurred)
    {
      break; // Przerwij pętlę, jeśli wystąpił timeout
    }

    periodSum += GetMax;
  }

  if (timeoutOccurred)
  {
    return 0.0; // Zwróć zero w przypadku timeoutu
  }

  unsigned long averagePeriod = periodSum / sampleCount;
  return averagePeriod*0.10260869565217391304347826086957;
}


float measureFrequency()
{
  unsigned long startTime = micros();
  unsigned long endTime;
  unsigned long periodSum = 0;
  int sampleCount = 10; // liczba próbek do uśrednienia
  bool timeoutFlag = false;

  for (int i = 0; i < sampleCount; i++)
  {
    // Poczekaj na zmianę stanu z niskiego na wysoki
    unsigned long startWaitTime = micros();
    while (analogRead(32) < 2200)
    {
      delay(2);
      if (micros() - startWaitTime > timeout)
      {
        timeoutFlag = true;
        break;
      }
    }

    if (timeoutFlag)
      break;

    // Poczekaj na zmianę stanu z wysokiego na niski
    startWaitTime = micros();
    while (analogRead(32) > 2100)
    {
      if (micros() - startWaitTime > timeout)
      {
        timeoutFlag = true;
        break;
      }
    }

    if (timeoutFlag)
      break;

    endTime = micros();
    unsigned long period = endTime - startTime;
    periodSum += period;
    startTime = endTime;
  }

  if (timeoutFlag)
    return 0;

  // Obliczanie średniej wartości okresu  
  unsigned long averagePeriod = periodSum / sampleCount;

  // Obliczanie częstotliwości
  float measuredFrequency = 1000000.0 / averagePeriod;

  return measuredFrequency - 3;
}
