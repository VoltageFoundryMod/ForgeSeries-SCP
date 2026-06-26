#pragma once

#include <Adafruit_MCP4725.h>
#include <Arduino.h>
#include <Wire.h>

#include "pinouts.hpp"

// Add prototypes for functions defined in this file
void InitIO();
void InternalDAC(int intDAC_OUT);
void MCP(int MCP_OUT);
void DACWrite(int pin, int value);
void PWM1(int duty1);
void PWM2(int duty2);
void PWMWrite(int pin, int value);
void SetPin(int pin, int value);

// Create the MCP4725 object
Adafruit_MCP4725 dac;
#define DAC_RESOLUTION (12)

// Handle IO devices initialization
void InitIO() {
    // ADC settings. These increase ADC reading stability but at the cost of cycle time. Takes around 0.7ms for one reading with these
    REG_ADC_AVGCTRL |= ADC_AVGCTRL_SAMPLENUM_1;
    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM_128 | ADC_AVGCTRL_ADJRES(4);

    // Configure ADC for SAMD21
    analogReference(AR_DEFAULT); // Set reference voltage
    analogWriteResolution(10);
    analogReadResolution(12);

    // Configure Timer/Counter for SAMD21
    // TcCount16 *TC = (TcCount16 *)TC3;                                                        // Use TC3
    // TC->CTRLA.reg &= ~TC_CTRLA_ENABLE;                                                       // Disable TC
    // TC->CTRLA.reg = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_WAVEGEN_MFRQ | TC_CTRLA_PRESCALER_DIV1; // Configure TC
    // TC->CTRLA.reg |= TC_CTRLA_ENABLE;                                                        // Enable TC
    // while (TC->STATUS.bit.SYNCBUSY)
    //     ; // Wait for synchronization

    // Set ADC clock prescaler to 8 for SAMD21
    // ADC->CTRLB.reg = (ADC->CTRLB.reg & ~ADC_CTRLB_PRESCALER_Msk) | ADC_CTRLB_PRESCALER_DIV8;
    // while (ADC->STATUS.bit.SYNCBUSY)
    //     ; // Wait for synchronization

    pinMode(LED_BUILTIN, OUTPUT); // LED
    pinMode(CLK_IN_PIN, INPUT);   // CLK in
    for (int i = 0; i < NUM_CV_INS; i++) {
        pinMode(CV_IN_PINS[i], INPUT); // CV in
    }
    pinMode(ENCODER_SW, INPUT_PULLUP); // push sw
    for (int i = 0; i < NUM_OUTS; i++) {
        pinMode(OUT_PINS[i], OUTPUT); // Gate out
    }

    // Initialize the DAC
    if (!dac.begin(0x60)) { // 0x60 is the default I2C address for MCP4725
        Serial.println("MCP4725 not found!");
        while (1)
            ;
    }
    Serial.println("MCP4725 initialized.");
    MCP(0);                       // Set the DAC output to 0
    InternalDAC(0);               // Set the internal DAC output to 0
    digitalWrite(OUT_PIN_1, LOW); // Initialize the output pins to low
    digitalWrite(OUT_PIN_2, LOW); // Initialize the output pins to low
}

// Handle DAC Outputs
void InternalDAC(int value) {
    analogWrite(DAC_INTERNAL_PIN, value / 4); // "/4" -> 12bit to 10bit
}

void MCP(int value) {
    dac.setVoltage(value, false);
}

// Write to DAC pins indexed by 0
void DACWrite(int pin, int value) {
    switch (pin) {
    case 0: // Internal DAC
        InternalDAC(value);
        break;
    case 1: // MCP DAC
        MCP(value);
        break;
    default:
        // Handle invalid pin case if necessary
        break;
    }
}

// Write to PWM pins indexed by 0
void PWMWrite(int pin, int value) {
    switch (pin) {
    case 0: // PWM 1
        pwm(OUT_PINS[0], 46000, value);
        break;
    case 1: // PWM 2
        pwm(OUT_PINS[1], 46000, value);
        break;
    default:
        // Handle invalid pin case if necessary
        break;
    }
}

// Set the output pins. Value can be from 0 to 4095 (12bit).
// For pins 0 and 1, 0 is low and anything else is high
void SetPin(int pin, int value) {
    switch (pin) {
    case 0: // Gate Output 1
        digitalWrite(OUT_PINS[0], value > 0 ? LOW : HIGH);
        break;
    case 1: // Gate Output 2
        digitalWrite(OUT_PINS[1], value > 0 ? LOW : HIGH);
        break;
    case 2: // Internal DAC Output
        DACWrite(0, value);
        break;
    case 3: // MCP DAC Output
        DACWrite(1, value);
        break;
    default:
        // Handle invalid pin case if necessary
        break;
    }
}
