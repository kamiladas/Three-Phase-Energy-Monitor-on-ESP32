#ifndef MEASUREMENT_DATA_H
#define MEASUREMENT_DATA_H

struct PhaseData {
    float voltageRMS;
    float currentRMS;
    float realPower;
    float apparentPower;
    float powerFactor;
    String time;
};

struct MeasurementData {
    PhaseData phase1;
    PhaseData phase2;
    PhaseData phase3;
    
};

#endif // MEASUREMENT_DATA_H
