#include "led_strip.h"
#include <iostream>
#include <cstring>
#include <cmath>

LEDStrip::LEDStrip(int count, int gpio_pin, int dmanum, int brightness) {
    memset(&ledstring, 0, sizeof(ws2811_t));
    ledstring.freq = WS2811_TARGET_FREQ;
    ledstring.dmanum = dmanum;
    ledstring.channel[0].gpionum = gpio_pin;
    ledstring.channel[0].invert = 0;
    ledstring.channel[0].count = count;
    ledstring.channel[0].strip_type = WS2811_STRIP_BRG;
    ledstring.channel[0].brightness = brightness;


    previousColor = std::vector<cv::Vec3b>(count, cv::Vec3b(0,0,0));
    temporalAlpha = 0.25;
}

LEDStrip::~LEDStrip() {
    if (is_initialized) {
        ws2811_fini(&ledstring);
        is_initialized = false;
    }
}

void LEDStrip::setCount(int count) {
    ledstring.channel[0].count = count;
}

bool LEDStrip::init() {
    if (is_initialized) {
        ws2811_fini(&ledstring);
        is_initialized = false;
    }

    ws2811_return_t ret = ws2811_init(&ledstring);
    if (ret != WS2811_SUCCESS) {
        std::cerr << "ws2811_init failed: " << ws2811_get_return_t_str(ret) << std::endl;
        return false;
    }
    is_initialized = true;
    return true;
}

void LEDStrip::show() {
    ws2811_render(&ledstring);
}

void LEDStrip::clear() {
    for (int i = 0; i < ledstring.channel[0].count; ++i) {
        ledstring.channel[0].leds[i] = 0;
    }
    show();
}


void LEDStrip::setPixel(int idx, uint32_t color) {
    if (idx >= 0 && idx < ledstring.channel[0].count) {
        ledstring.channel[0].leds[idx] = color;
    }
}

float LEDStrip::temporalSmooth(float valueNew, float valueOld) {
    return valueNew * temporalAlpha + (1-temporalAlpha) * valueOld;
}

void LEDStrip::updateStrip(const std::vector<cv::Vec3b>& colors, bool temporalSmoothOn) {
    for (size_t i = 0; i < colors.size() && i < (size_t)ledstring.channel[0].count; ++i) {
        // colors are BGR (OpenCV)
        const cv::Vec3b& newColor = colors[i];
        cv::Vec3b& prevColor = previousColor[i];

        uint8_t outR, outG, outB;
        if (temporalSmoothOn) {
            outR = static_cast<uint8_t>(std::round(temporalSmooth(newColor[2], prevColor[2])));
            outG = static_cast<uint8_t>(std::round(temporalSmooth(newColor[1], prevColor[1])));
            outB = static_cast<uint8_t>(std::round(temporalSmooth(newColor[0], prevColor[0])));
        } else {
            outR = newColor[2];
            outG = newColor[1];
            outB = newColor[0];
        }

        ledstring.channel[0].leds[i] = ((uint32_t)outR << 16) | ((uint32_t)outG << 8) | (uint32_t)outB;
        prevColor = cv::Vec3b(outB, outG, outR);
    }
}

void LEDStrip::updateStripWithGamma(const std::vector<cv::Vec3b>& colors, bool temporalSmoothOn) {
    for (size_t i = 0; i < colors.size() && i < (size_t)ledstring.channel[0].count; ++i) {
        uint8_t newB = gamma8[colors[i][0]];
        uint8_t newG = gamma8[colors[i][1]];
        uint8_t newR = gamma8[colors[i][2]];

        cv::Vec3b& prevColor = previousColor[i];

        uint8_t outR, outG, outB;
        if (temporalSmoothOn) {
            outR = static_cast<uint8_t>(std::round(temporalSmooth(newR, prevColor[2])));
            outG = static_cast<uint8_t>(std::round(temporalSmooth(newG, prevColor[1])));
            outB = static_cast<uint8_t>(std::round(temporalSmooth(newB, prevColor[0])));
        } else {
            outR = newR;
            outG = newG;
            outB = newB;
        }

        ledstring.channel[0].leds[i] = ((uint32_t)outR << 16) | ((uint32_t)outG << 8) | (uint32_t)outB;
        previousColor[i] = cv::Vec3b(outB, outG, outR);
    }
}

void LEDStrip::setBrightness(int brightness) {
    if(brightness < 0 || brightness > 255) {
        std::cerr << "WARNING: Attempted to set light strip to a negative value or to a value larger than 255" << std::endl;
    }
    ledstring.channel[0].brightness = brightness;
    return;
}

int LEDStrip::size() const {
    return ledstring.channel[0].count;
}

uint32_t LEDStrip::Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

int LEDStrip::getBrightness() const {
    return ledstring.channel[0].brightness;
}

void LEDStrip::calibrateBrightness(float maxAmperage, float supplyVoltage) {
    float maxWattsSupplied = maxAmperage * supplyVoltage;
    float maxWattsPulled = this->size() * wattsPerPixel;
    float brightnessFactor = std::min(maxWattsSupplied / maxWattsPulled, 1.0f);
    setBrightness(static_cast<int>(std::round(brightnessFactor * 255)));
}



const uint8_t LEDStrip::gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,166,168,170,172,174,
  176,178,180,182,184,186,188,191,193,195,197,199,202,204,206,209,
  211,213,215,218,220,223,225,227,230,232,235,237,240,242,245,247
};
