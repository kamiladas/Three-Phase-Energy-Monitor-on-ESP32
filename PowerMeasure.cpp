// PowerMeter.cpp

#include "PowerMeasure.h"

PowerMeter::PowerMeter(int pinCurrent, int pinVoltage, float voltageCalibration, float currentCalibration, int numSamples, int volOffset, int ampOffset)
: PIN_CURRENT(pinCurrent), PIN_VOLTAGE(pinVoltage),
  VOLTAGE_CALIBRATION(voltageCalibration), CURRENT_CALIBRATION(currentCalibration),
  NUM_SAMPLES(numSamples), vol_offset(volOffset), amp_offset(ampOffset) {}

void PowerMeter::performMeasurement() {
    sumVoltage = 0;
    sumCurrent = 0;
    sumPower = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {

      
        float current = (analogRead(PIN_CURRENT) - amp_offset) * CURRENT_CALIBRATION;
        delay(0.3);
        float voltage = (analogRead(PIN_VOLTAGE) - vol_offset) * VOLTAGE_CALIBRATION; 
        
        
      
        
        

        sumVoltage += voltage * voltage;
        sumCurrent += current * current;
        sumPower += voltage * current;
       
        
    }

    voltageRMS = sqrt(sumVoltage / NUM_SAMPLES)-3;
    if(voltageRMS < 25) { voltageRMS = 0; }    


    currentRMS = sqrt(sumCurrent / NUM_SAMPLES);
    if(currentRMS < 0.11) { currentRMS = 0; } /// wartość graniczna 

    if (currentRMS<0.20 &&currentRMS!=0) { currentRMS=currentRMS*0.80765307200165014364171757499818; }               
    if (currentRMS<0.1688 &&currentRMS!=0) {currentRMS=(currentRMS/0.80765307200165014364171757499818)*0.78666666666666666666666666666667;}
    if (currentRMS<0.12 &&currentRMS!=0) {currentRMS=((currentRMS/0.80765307200165014364171757499818)*0.66666666666666666666666666666667*0.8999999993345453534354435)-0.00599945;}

    realPower = sumPower / NUM_SAMPLES; 
    ///pozostałe parametry sieciowe 
    apparentPower = abs(voltageRMS * (currentRMS)); 
    powerFactor = abs(realPower /  apparentPower);

    if (powerFactor>1) {powerFactor= 0.99;}
    if(voltageRMS == 0 || currentRMS == 0) {
        realPower = 0;
        powerFactor = 0;
    }
}

float PowerMeter::getVoltageRMS() {
    return voltageRMS;
}

float PowerMeter::getCurrentRMS() {
    return currentRMS;
}

float PowerMeter::getRealPower() {
    return realPower;
}

float PowerMeter::getApparentPower() {
    return apparentPower;
}

float PowerMeter::getPowerFactor() {
    return powerFactor;
}
