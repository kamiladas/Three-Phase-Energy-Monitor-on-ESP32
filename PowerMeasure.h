

#ifndef PowerMeter_h
#define PowerMeter_h

#include <Arduino.h>

class PowerMeter {
public:
    PowerMeter(int pinCurrent, int pinVoltage, float voltageCalibration, float currentCalibration, int numSamples, int volOffset, int ampOffset);
    void performMeasurement();
    float getVoltageRMS();
    float getCurrentRMS();
    float getRealPower();
    float getApparentPower();
    float getPowerFactor();

private:
    int PIN_CURRENT;
    int PIN_VOLTAGE;
    float VOLTAGE_CALIBRATION;
    float CURRENT_CALIBRATION;
    int NUM_SAMPLES;
    int vol_offset;
    int amp_offset;
    float sumVoltage, sumCurrent, sumPower;
    float voltageRMS, currentRMS, realPower, apparentPower, powerFactor;
};

#endif
