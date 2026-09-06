#pragma once

// This component is the single source of truth for the production PCB pinout.
// GPIO 34, 35, 36 and 39 are input-only, which is correct for these ADC inputs.

#define EM_L1_VOLTAGE_GPIO 36
#define EM_L1_CURRENT_GPIO 33
#define EM_L2_VOLTAGE_GPIO 39
#define EM_L2_CURRENT_GPIO 32
#define EM_L3_VOLTAGE_GPIO 34
#define EM_L3_CURRENT_GPIO 35

#define EM_L1_VOLTAGE_ADC1_CHANNEL 0
#define EM_L1_CURRENT_ADC1_CHANNEL 5
#define EM_L2_VOLTAGE_ADC1_CHANNEL 3
#define EM_L2_CURRENT_ADC1_CHANNEL 4
#define EM_L3_VOLTAGE_ADC1_CHANNEL 6
#define EM_L3_CURRENT_ADC1_CHANNEL 7
