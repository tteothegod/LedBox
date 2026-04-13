#include <iostream>
#include <fstream>
#include <wiringPi.h>
#include "led_strip.h"
#include "saveLoadConfig.hpp"

constexpr int MIN_CALIB_BRIGHTNESS = 32;
constexpr int MAX_LEDS = 600;

constexpr int GPIO_LEFT_BTN = 23;
constexpr int GPIO_ACTION_BTN = 24;
constexpr int GPIO_RIGHT_BTN = 25;

constexpr int MENU_MODES = 5;


int selectProgramMode(LEDStrip& strip) {
    std::cout << "Select program mode using the left and right buttons, then press the action button to confirm." << std::endl;
    int programSelIdx = 0;
    while(true) {
        int leftBtnState = digitalRead(GPIO_LEFT_BTN);
        int actionBtnState = digitalRead(GPIO_ACTION_BTN);
        int rightBtnState = digitalRead(GPIO_RIGHT_BTN);

        if(actionBtnState == LOW) {
            std::cout << "Action button pressed! Breaking loop at program index: " << programSelIdx << std::endl;
            delay(300); // Debounce delay
            break;
        }
        if(leftBtnState == LOW && rightBtnState == HIGH) {
            programSelIdx = std::max(0, programSelIdx - 1);
            std::cout << "Left button pressed! Current program index: " << programSelIdx << std::endl;
            delay(300); // Debounce delay
        }
        if(rightBtnState == LOW && leftBtnState == HIGH) {
            programSelIdx = std::min(MENU_MODES - 1, programSelIdx + 1);
            std::cout << "Right button pressed! Current program index: " << programSelIdx << std::endl;
            delay(300); // Debounce delay
        }

        int selected_index = programSelIdx % MAX_LEDS; // Wrap around if it exceeds MAX_LEDS
        // Number of visible menu modes (configurable)
        int visibleIndex = ((programSelIdx % MENU_MODES) + MENU_MODES) % MENU_MODES; // wrap into 0..MENU_MODES-1

        // Use every other LED as menu slots (even indices). Odd indices are gaps that can show a dim glow around the selected slot.
        for (int i = 0; i < strip.size(); ++i) {
            // Menu slots: even indices map to 0..MENU_MODES-1
            if ((i % 2 == 0) && (i / 2) < MENU_MODES) {
                int slot = i / 2;
                int distance = abs(slot - visibleIndex);
                if (distance == 0) {
                    strip.setPixel(i, LEDStrip::Color(0, 255, 0));   // Selected slot (Bright Green)
                } else {
                    strip.setPixel(i, LEDStrip::Color(20, 20, 20));  // Other menu slots (Very Dim)
                }
            }
            // Gaps between menu slots: odd indices — highlight if adjacent to the selected even index
            else if (i % 2 == 1) {
                int leftEven = i - 1;
                int rightEven = i + 1;
                bool adjacentToSelected = false;
                if (leftEven >= 0 && (leftEven / 2) == visibleIndex) adjacentToSelected = true;
                if (rightEven < strip.size() && (rightEven / 2) == visibleIndex) adjacentToSelected = true;
                if (adjacentToSelected) {
                    strip.setPixel(i, LEDStrip::Color(0, 60, 0));    // Dim green glow in the gap next to selected slot
                } else {
                    strip.setPixel(i, LEDStrip::Color(0, 0, 0));     // Background (Black)
                }
            }
            // Any other LEDs outside the visible menu range are off
            else {
                strip.setPixel(i, LEDStrip::Color(0, 0, 0));         // Background (Black)
            }
        }
        strip.show();
    }
    return programSelIdx;
}

int calibBrightness(LEDStrip& strip, int initialBrightness) {
    std::cout << "Calibrating brightness..." << std::endl;
    int working_brightness_value = initialBrightness;
    strip.setBrightness(working_brightness_value);

    while (true) {
        int leftBtnState = digitalRead(GPIO_LEFT_BTN);
        int actionBtnState = digitalRead(GPIO_ACTION_BTN);
        int rightBtnState = digitalRead(GPIO_RIGHT_BTN);

        if(actionBtnState == LOW) {
            std::cout << "Action button pressed! Breaking loop with working brightness: " << working_brightness_value << std::endl;
            delay(300);
            break;
        }
        if(leftBtnState == LOW && rightBtnState == HIGH) {
            if (working_brightness_value == 0) {
                // Flash red at max brightness
                uint8_t last_brightness = strip.getBrightness();
                strip.setBrightness(255);
                for(int i = 0; i < strip.size(); ++i) strip.setPixel(i, LEDStrip::Color(255, 0, 0));
                strip.show();
                delay(300);
                strip.setBrightness(last_brightness);
            } else {
                working_brightness_value = std::max(0, working_brightness_value - 5);
                strip.setBrightness(working_brightness_value);
            }
            std::cout << "Left button pressed! Current working brightness: " << working_brightness_value << std::endl;
            delay(300); // Debounce delay
        }
        if(rightBtnState == LOW && leftBtnState == HIGH) {
            if (working_brightness_value == 255) {
                // Flash red at max brightness
                uint8_t last_brightness = strip.getBrightness();
                strip.setBrightness(255);
                for(int i = 0; i < strip.size(); ++i) strip.setPixel(i, LEDStrip::Color(255, 0, 0));
                strip.show();
                delay(300);
                strip.setBrightness(last_brightness);
            } else {
                working_brightness_value = std::min(255, working_brightness_value + 5);
                strip.setBrightness(working_brightness_value);
            }
            std::cout << "Right button pressed! Current working brightness: " << working_brightness_value << std::endl;
            delay(300); // Debounce delay
        }

        // Always show full white; the brightness is handled by the strip's global brightness setting.
        for( int i = 0; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(255, 255, 255));
        }
        strip.show();
    }
    return working_brightness_value;
}

int calibrateLEDIdx(LEDStrip& strip, int initValue) {
    std::cout << "Calibrating LED count..." << std::endl;
    int workingIdx = initValue;
    while (true) {
        int leftBtnState = digitalRead(GPIO_LEFT_BTN);
        int actionBtnState = digitalRead(GPIO_ACTION_BTN);
        int rightBtnState = digitalRead(GPIO_RIGHT_BTN);

        if(actionBtnState == LOW) {
            std::cout << "Action button pressed! Breaking loop with working index: " << workingIdx << std::endl;
            delay(300); // Debounce delay
            break;
        }
        if(leftBtnState == LOW && rightBtnState == HIGH) {
            workingIdx = std::max(0, workingIdx - 1);
            std::cout << "Left button pressed! Current working index: " << workingIdx << std::endl;
            delay(300); // Debounce delay
        }
        if(rightBtnState == LOW && leftBtnState == HIGH) {
            workingIdx = std::min(strip.size() - 1, workingIdx + 1);
            std::cout << "Right button pressed! Current working index: " << workingIdx << std::endl;
            delay(300); // Debounce delay
        }

        for( int i = 0; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(0, 0, 0)); // Clear all LEDs
        }
        for (int i = 0; i < workingIdx; ++i) {
            strip.setPixel(i, LEDStrip::Color(255, 255, 255)); // Set the first 'workingLength' LEDs to white
        }
        strip.setPixel(workingIdx, LEDStrip::Color(255, 0, 0)); // Set the last LED to red
        strip.show();
    }
    return workingIdx;
}

float calibratePercentage(LEDStrip& strip, int cornerZeroIndex, float initPercentage = 0.0f) {
    std::cout << "Calibrating Percentage ..." << std::endl;
    float percentIncrement = 1.0f / (static_cast<float>(cornerZeroIndex) + 1.0f);
    float workingPercentage = initPercentage;
    while (true) {
        int leftBtnState = digitalRead(GPIO_LEFT_BTN);
        int actionBtnState = digitalRead(GPIO_ACTION_BTN);
        int rightBtnState = digitalRead(GPIO_RIGHT_BTN);

        if(actionBtnState == LOW) {
            std::cout << "Action button pressed! Breaking loop with percentage: " << workingPercentage * 100 << std::endl;
            delay(300); // Debounce delay
            break;
        }
        if(leftBtnState == LOW && rightBtnState == HIGH) {
            workingPercentage = std::max(0.0f, workingPercentage - percentIncrement);
            std::cout << "Left button pressed! Current working percentage: " << workingPercentage * 100.0f << std::endl;
            delay(300); // Debounce delay
        }
        if(rightBtnState == LOW && leftBtnState == HIGH) {
            workingPercentage = std::min(1.0f, workingPercentage + percentIncrement);
            std::cout << "Right button pressed! Current working percentage: " << workingPercentage * 100.0f << std::endl;
            delay(300); // Debounce delay
        }

        int currentIndex = static_cast<int>(std::round(workingPercentage * (cornerZeroIndex + 1)));

        for (int i = 0; i < currentIndex; ++i) {
            strip.setPixel(i, LEDStrip::Color(0, 255, 0)); // Set the first 'workingLength' LEDs to white
        }
        for (int i = currentIndex + 1; i < cornerZeroIndex; ++i) {
            strip.setPixel(i, LEDStrip::Color(128, 128, 128)); // Set the first 'workingLength' LEDs to white
        }
        for( int i = cornerZeroIndex + 1; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(0, 0, 0)); // Clear all LEDs
        }
        strip.setPixel(currentIndex, LEDStrip::Color(255, 0, 0)); // Highlight current index in red
        strip.show();
    }

    return workingPercentage;
}

int* calibrateLEDLayout(LEDStrip& strip, int* prevLEDLayout) {
    std::cout << "Calibrating LED layout..." << std::endl;
    int* corners = new int[4];
    bool calibratedBefore = false;
    if(prevLEDLayout != nullptr) {
        calibratedBefore = prevLEDLayout[0] != -1 || prevLEDLayout[1] != -1 || prevLEDLayout[2] != -1 || prevLEDLayout[3] != -1;
    }
    for( int i = 0; i < 4; ++i) {
        int prevIdx = (calibratedBefore) ? prevLEDLayout[i] : (i == 0) ? 0 : corners[i - 1]; // Start from the last LED for the first corner, then use the previous corner's index
        corners[i] = calibrateLEDIdx(strip, prevIdx); // Reuse the LED idx calibration function to set each corner's LED count
    }
    return corners;
}

int calibrate() {
    delay(2000); // Short delay to allow system to stabilize
    // WiringPi setup
    if (wiringPiSetupGpio() == -1) {
        std::cerr << "Error: Failed to setup wiringPi." << std::endl;
        return 1;
    }
    pinMode(GPIO_LEFT_BTN, INPUT);
    pinMode(GPIO_ACTION_BTN, INPUT);
    pinMode(GPIO_RIGHT_BTN, INPUT);

    pullUpDnControl(GPIO_LEFT_BTN, PUD_UP); // All wired to ground, so use internal pull-up resistors
    pullUpDnControl(GPIO_ACTION_BTN, PUD_UP);
    pullUpDnControl(GPIO_RIGHT_BTN, PUD_UP);

    SetupParameters params;

    // Load config and initialize LED strip
    if(loadConfig(params)) {
        std::cout << "Config loaded successfully." << std::endl;
    } else {
        std::cerr << "Error: ledSyncVideo failed to load config. shutting down process..." << std::endl;
    }
    int brightness = std::max(params.getBrightness(), MIN_CALIB_BRIGHTNESS);
    LEDStrip strip = LEDStrip(MAX_LEDS, GPIO_PIN, DMA_NUM, brightness);
    if (!strip.init()) {
        std::cerr << "Error: Could not initialize LED strip." << std::endl;
        return -1;
    }

    // Select program mode using buttons
    int programSelIdx = selectProgramMode(strip);
    
    switch (programSelIdx) {
        case 0:
            // Exit Program
            break;
        case 1:
            // Calibrate Brightness
            params.setBrightness(calibBrightness(strip, brightness));
            break;
        case 2:
            // Calibrate LED count
            params.setLEDCount(calibrateLEDIdx(strip, params.getLEDCount()-1) + 1); // +1 because the calibration function returns the index of the last lit LED, which is one less than the count
            break;
        case 3:
            // Calibrate LED layout (corner positions)
            params.setCornerLEDLayout(calibrateLEDLayout(strip, params.getCornerLEDLayout()));
            break;
        case 4:
            // Calibrate saliency factor (percentage of LEDs to light up based on saliency)
            params.setSaliencyFactor(calibratePercentage(strip, params.getCornerLEDLayout()[0], params.getSaliencyFactor()));
            break;

        case 5:
            // Calibrate power limits (max amperage and supply voltage)
            std::cout << "Calibrating max amperage..." << std::endl;
            params.setMaxAmperage(calibratePercentage(strip, params.getCornerLEDLayout()[0], params.getMaxAmperage()));
            std::cout << "Calibrating supply voltage..." << std::endl;
            params.setSupplyVoltage(calibratePercentage(strip, params.getCornerLEDLayout()[0], params.getSupplyVoltage()));
            break;
        default:
            std::cout << "Invalid program index selected. Exiting." << std::endl;
            return -1;
    }
    for( int i = 0; i < strip.size(); ++i) {
        strip.setPixel(i, LEDStrip::Color(0, 0, 0)); // Set all LEDs to the current working brightness
    }
    strip.show();
    if(saveConfig(params)) {
        std::cout << "Config saved successfully." << std::endl;
    } else {
        std::cerr << "Error: Failed to save config." << std::endl;
    }
    // TODO: save config after calibration so that it can be loaded in the future without needing to recalibrate every time
    return 0;
}

int main() {
    try {
        return calibrate();
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
}
