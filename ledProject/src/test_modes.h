#ifndef TEST_MODES_H
#define TEST_MODES_H

#define OPENCV_BUILT 1

#include <iostream>
#include <cmath>
#include "led_strip.h"

class SetupParameters {
    public:
    SetupParameters()
        : stripLength(0), LEDDensity(0), LEDCount(0), LEDSHorizontal(0), LEDSVertical(0), border_percent(0.25), resize_factor(0.25) {}
    SetupParameters(float lengthOfStrip, float density)
        : stripLength(lengthOfStrip), LEDDensity(density), LEDCount(0), LEDSHorizontal(0), LEDSVertical(0), border_percent(0.25), resize_factor(0.25) {}

    void setLength(float lengthOfStrip) {
        if(lengthOfStrip < 1) {
            throw std::invalid_argument("Length of strip must be positive");
            return;
        }
        stripLength = lengthOfStrip;
    }
    void setDensity(float density) {
        if(density < 1) {
            throw std::invalid_argument("density of LED's must be positive");
            return;
        }
        LEDDensity = density;    
    }
    void setLEDCount(int count) {
        if(count < 1) {
            throw std::invalid_argument("LED count must be positive");
            return;
        }
        LEDCount = count;   
    }
    void setBorderPercent(float percent) {
        if(percent < 0 || percent > 0.5) {
            throw std::invalid_argument("Border percent must be between 0 and 0.5");
            return;
        }
        border_percent = percent;
    }
    void setResizeFactor(float factor) {
        if(factor <= 0 || factor > 1) {
            throw std::invalid_argument("Resize factor must be between 0 and 1");
            return;
        }
        resize_factor = factor;
    } 

    float getLength() { return stripLength; }
    float getDensity() { return LEDDensity; }
    float getLEDSHorizontal() { return LEDSHorizontal; }
    float getLEDSVertical() { return LEDSVertical; }
    int getLEDCount() { return LEDCount; }
    float getBorderPercent() { return border_percent; }
    float getResizeFactor() { return resize_factor; }


    void configureLayout(float videoWidth, float videoHeight) {
        // compute total LEDs by multiplying first, then round to nearest int
        LEDCount = static_cast<int>(std::round(stripLength * LEDDensity));
        
        // ensure total is even so it can evenly split across opposite edges
        if (LEDCount % 2 != 0) {
            LEDCount -= 1;
        }

        int halfCount = LEDCount / 2;

        // Distribute halfCount exactly between horizontal and vertical
        LEDSHorizontal = std::round((videoWidth / (videoWidth + videoHeight)) * halfCount);
        LEDSVertical = halfCount - LEDSHorizontal;
    }

    private:
    float stripLength;
    float LEDDensity;
    int LEDCount;
    float LEDSHorizontal;
    float LEDSVertical;
    float border_percent;
    float resize_factor;

};




namespace TestModes {
    void runBasicTest(LEDStrip& strip);
    void runOpenCVTest(LEDStrip& strip, SetupParameters& params);
    void runOpenCVTest2(LEDStrip& strip, SetupParameters& params);
    void videoPropTest(LEDStrip& strip, SetupParameters& params);

}

#endif // TEST_MODES_H
