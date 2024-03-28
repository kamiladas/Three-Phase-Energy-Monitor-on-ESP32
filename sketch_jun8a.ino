#include "ESP32CaptivePortal.h"
#include "FS.h"
#include "SPIFFS.h"
#include "SDLogger.h"
#include "PowerMeasure.h"
#include "PhaseConfig.h"
#include "MeasurementData.h"
//inicjacja karty sd
#define SD_MISO  19
#define SD_MOSI 23
#define SD_SCLK 18
#define SD_CS 5
SDLogger sdLogger(SD_MISO, SD_MOSI, SD_SCLK, SD_CS);

String lastHourlySaveTime = "1970-11-24 00:00:00";






ESP32CaptivePortal captivePortal;

TaskHandle_t myTask;


void setup() {

  Serial.begin(115200);
  delay(500);
  sdLogger.setup();
  delay(500);
 
  delay(500);
   if (!SPIFFS.begin(true)) {
    Serial.println("Błąd inicjalizacji SPIFFS");
    return;
  }

  delay(500);
  
  captivePortal.begin();
  delay(100);
   xTaskCreatePinnedToCore(
    loop_second,        // Funkcja zadania
    "loop_second",      // Nazwa zadania
    10000,              // Rozmiar stosu zadania (w słowach)
    NULL,               // Parametr przekazywany do zadania (brak w tym przypadku)
    0,                  // Priorytet zadania (im niższa wartość, tym wyższy priorytet)
    &myTask,            // Uchwyt do zadania
    0                   // Numer rdzenia, na którym ma być uruchomione zadanie (0 - CORE_0)
  );

}


String path;
PowerMeter phase1(L1_CURRENT, L1_VOLTAGE, VOLTAGE1_CALIBRATION,CURRENT1_CALIBRATION , NUM_SAMPLES, VOL1_OFFSET, AMP1_OFFSET);
PowerMeter phase2(L2_CURRENT, L2_VOLTAGE, VOLTAGE2_CALIBRATION,CURRENT2_CALIBRATION , NUM_SAMPLES, VOL2_OFFSET, AMP2_OFFSET);
PowerMeter phase3(L3_CURRENT, L3_VOLTAGE, VOLTAGE3_CALIBRATION,CURRENT3_CALIBRATION , NUM_SAMPLES, VOL3_OFFSET, AMP3_OFFSET);

MeasurementData data;
const int tempindex=20 ;
int tempcount=0;
bool setPath=true;
static float SumEnergy=0.00000f,Energy1=0.00000f,Energy2=0.00000f,Energy3=0.00000f;
MeasurementData temp1[tempindex];
MeasurementData temp2[tempindex];
MeasurementData temp3[tempindex];



void loop_second(void *parameter) {

while(1) {
           
            if(setPath){String path = "/main_data" + captivePortal.Generate_FlieName(true);
            captivePortal.setFPath(path);
            setPath=false;
        
                       }

            if(captivePortal.data_ready) {    
            String temp;
            temp=captivePortal.loadConfig("/GetEnery.json", "totalEnergy");                     
            String path = "/main_data" + captivePortal.Generate_FlieName(false);
            captivePortal.setFPath(path);            
            path = "/main_data" + captivePortal.Generate_FlieName(false);        
            savePhaseDataToSD(sdLogger, temp1, tempindex, path.c_str());                 
            captivePortal.data_ready = false;
        }             
            captivePortal.loop();
           
}
}



void loop() {
  
        String temp=captivePortal.getCurrentTime();
        phase1.performMeasurement();
        // Przypisanie danych do fazy 2
        data.phase1.voltageRMS = phase1.getVoltageRMS(); 
        data.phase1.currentRMS = phase1.getCurrentRMS();
        data.phase1.realPower = phase1.getRealPower();
        data.phase1.apparentPower = phase1.getApparentPower();
        data.phase1.powerFactor = phase1.getPowerFactor();
          
        Serial.println(data.phase1.powerFactor);

        data.phase1.time = captivePortal.getCurrentTime(); // Przypisanie wspólnego czasu
        captivePortal.setData(data);
        temp1[tempcount]=data;
      
      


      
        phase3.performMeasurement();
        data.phase3.voltageRMS = phase3.getVoltageRMS(); 
        data.phase3.currentRMS = phase3.getCurrentRMS();
        data.phase3.realPower = phase3.getRealPower();
        data.phase3.apparentPower = phase3.getApparentPower();
        data.phase3.powerFactor = phase3.getPowerFactor();
        data.phase3.time = captivePortal.getCurrentTime(); // Przypisanie wspólnego czasu
        captivePortal.setData(data);
        temp3[tempcount]=data;


        phase2.performMeasurement();
        

       
        data.phase2.voltageRMS = phase2.getVoltageRMS();       
        data.phase2.currentRMS = phase2.getCurrentRMS();
        data.phase2.realPower = phase2.getRealPower();
        data.phase2.apparentPower = phase2.getApparentPower();
        data.phase2.powerFactor = phase2.getPowerFactor();
        data.phase2.time = captivePortal.getCurrentTime(); // Przypisanie wspólnego czasu
        captivePortal.setData(data);
        temp2[tempcount]=data;
        tempcount=tempcount+1;


        if(tempcount>tempindex-1){
      
          tempcount=0;
          Serial.println("data ready true ");
          captivePortal.data_ready=true;
          
        }

        

        
      
}





void savePhaseDataToSD(SDLogger& logger, MeasurementData temp[], int size, const char* path) {

    bool fileExists = SD.exists(path);
    float t1,t2,t3;
 
    // Przygotowanie nagłówka tylko jeśli plik nie istnieje
    String dataString;
    if (!fileExists) {
        dataString = "Time Phase1, Voltage RMS Phase1, Current RMS Phase1, Real Power Phase1, Apparent Power Phase1, Power Factor Phase1, Time Phase2, Voltage RMS Phase2, Current RMS Phase2, Real Power Phase2, Apparent Power Phase2, Power Factor Phase2, Time Phase3, Voltage RMS Phase3, Current RMS Phase3, Real Power Phase3, Apparent Power Phase3, Power Factor Phase3\n";
    }
    
    for (int i = 0; i < size; i++) {
        
        dataString += temp[i].phase1.time + ", ";
        dataString += String(temp[i].phase1.voltageRMS) + ", ";
        dataString += String(temp[i].phase1.currentRMS) + ", ";
        dataString += String(temp[i].phase1.realPower) + ", ";
        dataString += String(temp[i].phase1.apparentPower) + ", ";
        dataString += String(temp[i].phase1.powerFactor) + ", ";

        dataString += temp[i].phase2.time + ", ";
        dataString += String(temp[i].phase2.voltageRMS) + ", ";
        dataString += String(temp[i].phase2.currentRMS) + ", ";
        dataString += String(temp[i].phase2.realPower) + ", ";
        dataString += String(temp[i].phase2.apparentPower) + ", ";
        dataString += String(temp[i].phase2.powerFactor) + ", ";

        dataString += temp[i].phase3.time + ", ";
        dataString += String(temp[i].phase3.voltageRMS) + ", ";
        dataString += String(temp[i].phase3.currentRMS) + ", ";
        dataString += String(temp[i].phase3.realPower) + ", ";
        dataString += String(temp[i].phase3.apparentPower) + ", ";
        dataString += String(temp[i].phase3.powerFactor) + "\n";

        if(i!=0)
     
        {   
          
            /// Serial.println(i);
             t1 = static_cast<float>(captivePortal.calculateTimeDifferenceInSeconds(temp[i-1].phase1.time, temp[i].phase1.time));
             t2 = static_cast<float>(captivePortal.calculateTimeDifferenceInSeconds(temp[i-1].phase2.time ,temp[i].phase2.time));
             t3 = static_cast<float>(captivePortal.calculateTimeDifferenceInSeconds(temp[i-1].phase3.time ,temp[i].phase3.time));
             
             Energy1+=(static_cast<float>(temp[i-1].phase1.realPower))*t1;
             Energy2+=(static_cast<float>(temp[i-1].phase2.realPower))*t2;
             Energy3+=(static_cast<float>(temp[i-1].phase3.realPower))*t3;
             SumEnergy =Energy1+Energy2+Energy3;

        }
          
    }
        String energy1Str = String(Energy1 / 1000 / 3600, 6);
        String energy2Str = String(Energy2 / 1000 / 3600, 6);
        String energy3Str = String(Energy3 / 1000 / 3600, 6);
        String sumEnergyStr = String(SumEnergy / 1000 / 3600, 6);
        
        

        bool savePhase1=captivePortal.saveConfig("/GetEnery.json","energyPhase1",energy1Str);
        bool savePhase2=captivePortal.saveConfig("/GetEnery.json","energyPhase2",energy2Str);
        bool savePhase3=captivePortal.saveConfig("/GetEnery.json","energyPhase3",energy3Str);
        bool totalEnergy=captivePortal.saveConfig("/GetEnery.json","totalEnergy",sumEnergyStr);
        logger.writeFile(path, dataString);
    
  
}

void updateDataValues() {
    // Odczytaj wartości jako String
    String energy1Str = captivePortal.loadConfig("/GetEnery.json", "energyPhase1");
    String energy2Str = captivePortal.loadConfig("/GetEnery.json", "energyPhase2");
    String energy3Str = captivePortal.loadConfig("/GetEnery.json", "energyPhase3");

    // Konwersja String na float lub przypisanie domyślnej wartości, jeśli ciąg jest pusty
    Energy1 = (energy1Str != "") ? energy1Str.toFloat() : 0;
    Energy2 = (energy2Str != "") ? energy2Str.toFloat() : 0;
    Energy3 = (energy3Str != "") ? energy3Str.toFloat() : 0;
}



