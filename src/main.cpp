#include <Arduino.h>
// Rotary encoder setting
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <fix_fft.h>

// Load local libraries
#include "boardIO.cpp"
#include "definitions.hpp"
#include "pinouts.hpp"
#include "splash.hpp"
#include "version.hpp"

// Configuration
#define MAXDAC 4095
#define OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Rotary encoder object
Encoder encoder(ENC_PIN_1, ENC_PIN_2); // rotary encoder library setting
float oldPosition = -999;              // rotary encoder library setting
float newPosition = -999;              // rotary encoder library setting

// Menu / Switch variables
bool switchState = 1;
bool oldSwitchState = 1;
int menuMode = 1; // Menu mode for parameter editing
int oldMenuMode = 1;

byte param_select = 0;
byte param = 0;
byte param1 = 1;
bool param1_select = 0;
byte param2 = 1;
bool param2_select = 0;
// Add scale parameter
float scale = 1.0;

int rfrs = 0; // display refresh rate

bool trig = 0;     // external trig
bool old_trig = 0; // external trig , OFF -> ON detect

unsigned long trigTimer = 0;
unsigned long hideTimer = 0; // hide parameter count
bool hide = 0;               // 1=hide GUI,0=not hide

char data[128], im[128], cv[2][128]; // data and im are used for spectrum , cv is used for oscilo.

// Draw a horizontal dashed line
void DrawHDashedLine(int x0, int y0, int width, int color) {
    for (int i = 0; i < width; i += 4) {
        display.drawPixel(x0 + i, y0, color);
    }
}

// Draw a vertical dashed line
void DrawVDashedLine(int x0, int y0, int height, int color) {
    for (int i = 0; i < height; i += 4) {
        display.drawPixel(x0, y0 + i, color);
    }
}

void HandleDisplay() {
    oldMenuMode = menuMode;

    // OLED parameter hide while no operation
    if (hideTimer + 5000 >= millis()) { //
        hide = 1;
    } else {
        hide = 0;
    }

    // LFO mode--------------------------------------------------------------------------------------------
    if (menuMode == 1) {
        param = constrain(param, 1, 4); // Adjusted to include scale parameter
        param1 = constrain(param1, 1, 8);
        param2 = constrain(param2, 1, 8);

        display.clearDisplay();

        // Read trace data
        for (int k = 0; k < 2; k++) {
            for (int i = 126 / (9 - param1); i >= 0; i--) {
                display.drawLine(127 - (i * (9 - param1)), 63 - cv[k][i] - (param2 + (k * 7) - 1) * 4, 127 - (i + 1) * (9 - param1), 63 - cv[k][(i + 1)] - (param2 + (k * 7) - 1) * 4, WHITE); // right to left
                cv[k][i + 1] = cv[k][i];
                if (i == 0) {
                    long CVIn = analogRead(CV_IN_PINS[k]);
                    SetPin(k + 2, CVIn);
                    cv[k][0] = CVIn / 16 * scale;
                }
            }
        }
    }

    // wave mode---------------------------------------------------------------------------------------------
    else if (menuMode == 2) {
        param = constrain(param, 1, 4);
        param1 = constrain(param1, 1, 8);
        param2 = constrain(param2, 1, 6);

        // store data
        if (param1 > 5) { // for mid frequency
            for (int i = 127; i >= 0; i--) {
                cv[0][i] = analogRead(CV_1_IN_PIN) / 16 * scale;
                delayMicroseconds((param1 - 5) * 20);
            }
            rfrs++;
            if (rfrs >= (param2 - 1) * 2) {
                rfrs = 0;
                display.clearDisplay();
                for (int i = 127; i >= 1; i--) {
                    display.drawLine(127 - i, 63 - cv[0][i - 1], 127 - (i + 1), 63 - cv[0][(i)], WHITE);
                }
            }
        }

        else if (param1 <= 5) { // for high frequency
            for (int i = 127 / (6 - param1); i >= 0; i--) {
                cv[0][i] = analogRead(CV_1_IN_PIN) / 16 * scale;
            }
            rfrs++;
            if (rfrs >= (param2 - 1) * 2) {
                rfrs = 0;
                display.clearDisplay();
                for (int i = 127 / (6 - param1); i >= 1; i--) {
                    display.drawLine(127 - i * (6 - param1), 63 - cv[0][i - 1], 127 - (i + 1) * (6 - param1), 63 - cv[0][(i)], WHITE);
                }
            }
        }
    }

    // shot mode---------------------------------------------------------------------------------------------
    else if (menuMode == 3) {
        param = constrain(param, 1, 4);
        param1 = constrain(param1, 1, 4);
        old_trig = trig;
        trig = digitalRead(CLK_IN_PIN);

        //    trig detect
        if (old_trig == 0 && trig == 1) {
            for (int i = 10; i <= 127; i++) {
                cv[0][i] = analogRead(CV_1_IN_PIN) / 16 * scale;
                delayMicroseconds(100000 * param1); // 100000 is magic number
            }
            for (int i = 0; i < 10; i++) {
                cv[0][i] = 32;
            }
        }

        display.clearDisplay();
        for (int i = 126; i >= 1; i--) {
            display.drawLine(i, 63 - cv[0][i], (i + 1), 63 - cv[0][(i + 1)], WHITE);
        }
    }

    // spectrum analyze mode---------------------------------------------------------------------------------------------
    else if (menuMode == 4) {
        param = constrain(param, 1, 3);
        param1 = constrain(param1, 1, 4); // high freq sence
        param2 = constrain(param2, 1, 8); // noise filter

        for (byte i = 0; i < 128; i++) {
            int spec = analogRead(CV_1_IN_PIN);
            data[i] = spec / 4 - 128;
            im[i] = 0;
        };
        fix_fft(data, im, 7, 0);
        display.clearDisplay();
        for (byte i = 0; i < 64; i++) {
            int level = sqrt(data[i] * data[i] + im[i] * im[i]);
            ;
            if (level >= param2) {
                display.fillRect(i * 2, 63 - (level + i * (param1 - 1) / 8), 2, (level + i * (param1 - 1) / 8), WHITE); // i * (param1 - 1) / 8 is high freq amp
            }
        }
    }

    // Display Grid
    if (menuMode != 4) {
        DrawVDashedLine(0, 0, 64, WHITE);
        DrawVDashedLine(16, 0, 64, WHITE);
        DrawVDashedLine(32, 0, 64, WHITE);
        DrawVDashedLine(48, 0, 64, WHITE);
        DrawVDashedLine(64, 0, 64, WHITE);
        DrawVDashedLine(80, 0, 64, WHITE);
        DrawVDashedLine(96, 0, 64, WHITE);
        DrawVDashedLine(112, 0, 64, WHITE);
        DrawVDashedLine(127, 0, 64, WHITE);
        DrawHDashedLine(0, 0, 128, WHITE);
        DrawHDashedLine(0, 16, 128, WHITE);
        DrawHDashedLine(0, 32, 128, WHITE);
        DrawHDashedLine(0, 48, 128, WHITE);
        DrawHDashedLine(0, 63, 128, WHITE);
    }

    // Display overlay menu with params
    if (hide == 1) {
        String t1;
        String t2;
        String t3;
        String t4;
        // Display an overlay box with the mode parameters
        display.fillRect(30, 8, 68, 44, BLACK);
        display.drawLine(30, 8, 98, 8, WHITE);
        display.drawLine(30, 8, 30, 52, WHITE);
        display.drawLine(30, 52, 98, 52, WHITE);
        display.drawLine(98, 8, 98, 52, WHITE);
        switch (menuMode) {
        case 1:
            t1 = "Mode: " + String(menuMode);
            t2 = "Hori: " + String(param1);
            t3 = "Offs: " + String(param2);
            t4 = "Vert: " + String(scale, 2);
            break;
        case 2:
            t1 = "Mode: " + String(menuMode);
            t2 = "Hori: " + String(param1);
            t3 = "Refr: " + String(param2);
            t4 = "Vert: " + String(scale, 2);
            break;
        case 3:
            t1 = "Mode: " + String(menuMode);
            t2 = "Hori: " + String(param1);
            t3 = "Trig ";
            t4 = "Vert: " + String(scale, 2);
            break;
        case 4:
            t1 = "Mode: " + String(menuMode);
            t2 = "High: " + String(param1);
            t3 = "Filt " + String(param2);
            t4 = "";
            break;
        }
        // display
        if (param == 1) {
            display.drawLine(34, 20, 64, 20, WHITE);
        } else if (param == 2) {
            display.drawLine(34, 30, 64, 30, WHITE);
        } else if (param == 3) {
            display.drawLine(34, 40, 64, 40, WHITE);
        } else if (param == 4) {
            display.drawLine(34, 50, 64, 50, WHITE);
        }

        display.setTextColor(WHITE);
        if (param_select == 1) {
            display.setTextColor(BLACK, WHITE);
        }
        display.setCursor(34, 12);
        display.print(t1);

        display.setTextColor(WHITE);
        if (param_select == 2) {
            display.setTextColor(BLACK, WHITE);
        }
        display.setCursor(34, 22);
        display.print(t2);

        display.setTextColor(WHITE);
        if (param_select == 3) {
            display.setTextColor(BLACK, WHITE);
        }
        display.setCursor(34, 32);
        display.print(t3);

        display.setTextColor(WHITE);
        if (param_select == 4) {
            display.setTextColor(BLACK, WHITE);
        }
        display.setCursor(34, 42);
        display.print(t4);
    }

    display.display();
}

// // Handle encoder button click
void HandleEncoderClick() {
    oldSwitchState = switchState;
    switchState = digitalRead(ENCODER_SW);
    // select mode by push sw
    if (oldSwitchState == 0 && switchState == 1 && param_select == param) {
        param_select = 0;
        hideTimer = millis();
    } else if (oldSwitchState == 0 && switchState == 1 && param == 1) {
        param_select = param;
        hideTimer = millis();
    } else if (oldSwitchState == 0 && switchState == 1 && param == 2) {
        param_select = param;
        hideTimer = millis();
    } else if (oldSwitchState == 0 && switchState == 1 && param == 3) {
        param_select = param;
        hideTimer = millis();
    } else if (oldSwitchState == 0 && switchState == 1 && param == 4) {
        param_select = param;
        hideTimer = millis();
    }
    menuMode = constrain(menuMode, 1, 4);
    param = constrain(param, 1, 4);

    // initial settin when mode change.
    if (oldMenuMode != menuMode) {
        switch (menuMode) {
        case 1:
            param1 = 2; // time
            param2 = 1; // offset
            // pinMode(6, INPUT);      // no active 2pole filter
            // analogWrite(3, 0);      // offset = 0V
            // Set ADC clock prescaler to 8 for SAMD21
            // ADC->CTRLB.reg = (ADC->CTRLB.reg & ~ADC_CTRLB_PRESCALER_Msk) | ADC_CTRLB_PRESCALER_DIV8;
            // while (ADC->STATUS.bit.SYNCBUSY)
            //     ; // Wait for synchronization
            break;

        case 2:
            param1 = 3; // time
            param2 = 3; // offset
            // analogWrite(3, 127);    // offset = 2.5V
            // pinMode(6, INPUT);      // no active 2pole filter
            // Set ADC clock prescaler to 8 for SAMD21
            // ADC->CTRLB.reg = (ADC->CTRLB.reg & ~ADC_CTRLB_PRESCALER_Msk) | ADC_CTRLB_PRESCALER_DIV8;
            // while (ADC->STATUS.bit.SYNCBUSY)
            //     ; // Wait for synchronization
            break;

        case 3:
            param1 = 2; // time
            // analogWrite(3, 127);    // offset = 2.5V
            // pinMode(6, INPUT);      // no active 2pole filter
            // Set ADC clock prescaler to 8 for SAMD21
            // ADC->CTRLB.reg = (ADC->CTRLB.reg & ~ADC_CTRLB_PRESCALER_Msk) | ADC_CTRLB_PRESCALER_DIV8;
            // while (ADC->STATUS.bit.SYNCBUSY)
            //     ; // Wait for synchronization
            break;

        case 4:
            param1 = 2; // high freq amp
            param2 = 3; // noise filter
            // analogWrite(3, 127);    // offset = 2.5V
            // pinMode(6, OUTPUT);     // active 2pole filter
            // digitalWrite(6, LOW);   // active 2pole filter
            // Set ADC clock prescaler to 64 for SAMD21
            // ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV64;
            // while (ADC->STATUS.bit.SYNCBUSY)
            //     ; // Wait for synchronization

            break;
        }
    }
}

void HandleEncoderPosition() {
    newPosition = encoder.read();
    if ((newPosition - 3) / 4 > oldPosition / 4) {
        oldPosition = newPosition;
        hideTimer = millis();
        switch (param_select) {
        case 0:
            param--;
            break;

        case 1:
            menuMode--;
            break;

        case 2:
            param1--;
            break;

        case 3:
            param2--;
            break;
        case 4:
            scale = scale * 0.9;
            if (scale > 0.91 && scale < 1.09) {
                scale = 1.0;
            }
            break;
        }
    } else if ((newPosition + 3) / 4 < oldPosition / 4) {
        oldPosition = newPosition;
        hideTimer = millis();
        switch (param_select) {
        case 0:
            param++;
            break;

        case 1:
            menuMode++;
            break;

        case 2:
            param1++;
            break;

        case 3:
            param2++;
            break;
        case 4:
            scale = scale * 1.1;
            if (scale > 0.91 && scale < 1.09) {
                scale = 1.0;
            }
            break;
        }
    }
}

void setup() {
    // Initialize serial port
    Serial.begin(115200);

    // Initialize I/O (DAC, pins, etc.)
    InitIO();

    // Initialize OLED display with address 0x3C for 128x64
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Don't proceed, loop forever
    }
    Wire.setClock(1000000);
    display.clearDisplay();
    display.setTextWrap(false);

    display.clearDisplay();
    display.drawBitmap(30, 0, VFM_Splash, 68, 64, 1);
    display.display();
    delay(2000);
    // Print module name in the middle of the screen
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(4, 20);
    display.print("ForgeView");
    display.setTextSize(1);
    display.setCursor(80, 54);
    display.print("V" VERSION);
    display.display();
    delay(1500);
}

// // Handle IO without the display
void HandleIO() {
    HandleEncoderClick();

    HandleEncoderPosition();
}

// // Main loop
void loop() {
    HandleIO();

    HandleDisplay();
}
