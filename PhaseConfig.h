// Config.h
#ifndef Config_h
#define Config_h

// Definicja pinów
const int L1_VOLTAGE = 36;
const int L1_CURRENT = 33;


const int L2_VOLTAGE = 39;
const int L2_CURRENT = 32;

const int L3_VOLTAGE = 34;
const int L3_CURRENT = 35;

// Stałe kalibracyjne
const float VOLTAGE1_CALIBRATION = 0.98881355932203389830508474576271;
const float CURRENT1_CALIBRATION = 0.00508410745669093648004017072558;

const float VOLTAGE2_CALIBRATION = 1.03125;
const float CURRENT2_CALIBRATION = 0.00508410745669093648004017072558;

const float VOLTAGE3_CALIBRATION = 0.95881355932203389830508474576271;
const float CURRENT3_CALIBRATION = 0.00508410745669093648004017072558;

// Offsety
const int VOL1_OFFSET = 1945;
const int AMP1_OFFSET = 1947;

const int VOL2_OFFSET = 1945;
const int AMP2_OFFSET = 1955;

const int VOL3_OFFSET = 1947;
const int AMP3_OFFSET = 1947;




// Liczba próbek
const int NUM_SAMPLES = 500;

#endif
