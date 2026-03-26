#include <iostream>
#include <fstream>
#include <cmath>
#include <sstream>

class SetupParameters {
    public:
    SetupParameters()
        : stripLength(0), LEDDensity(0), LEDCount(0), LEDSHorizontal(0), LEDSVertical(0), border_percent(0.25), resize_factor(0.25), brightness(255), cornerLEDLayout(new int[4]{-1, -1, -1, -1}), saliencyFactor(1/10) {}
    SetupParameters(float lengthOfStrip, float density)
        : stripLength(lengthOfStrip), LEDDensity(density), LEDCount(0), LEDSHorizontal(0), LEDSVertical(0), border_percent(0.25), resize_factor(0.25), brightness(128), cornerLEDLayout(new int[4]{-1, -1, -1, -1}), saliencyFactor(1/10) {}

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
    void setBrightness(int b) {
        if(b < 0 || b > 255) {
            throw std::invalid_argument("Brightness must be between 0 and 255");
            return;
        }
        brightness = b;
    }
    void setCornerLEDLayout(int* counts) {
        if (!counts) return;
        for(int i=0; i<4; ++i) {
            cornerLEDLayout[i] = counts[i];
        }
    }

    void setSaliencyFactor(float factor) {
        if(factor > 1 || factor < 0) {
            throw std::invalid_argument("Saliency factor must be between 0 and 1");
        }
        saliencyFactor = factor;
    }

    void setMaxAmperage(float a) {
        if (a < 0) {
            throw std::invalid_argument("Amperage must be a positive value");
        }
        maxAmperage = a;
    }

    void setSupplyVoltage(float v) {
        if (v < 0) {
            throw std::invalid_argument("Supply Voltage must be a positive value");
        }
        supplyVoltage = v;
    }

    float getLength() { return stripLength; }
    float getDensity() { return LEDDensity; }
    int getLEDSHorizontal() { return LEDSHorizontal; }
    int getLEDSVertical() { return LEDSVertical; }
    int getLEDCount() { 
        if (LEDCount > 0) return LEDCount;
        if (stripLength > 0 && LEDDensity > 0) {
            return static_cast<int>(std::round(stripLength * LEDDensity));
        }
        return 0;
    }
    float getBorderPercent() { return border_percent; }
    float getResizeFactor() { return resize_factor; }
    int getBrightness() { return brightness; }
    int* getCornerLEDLayout() { return cornerLEDLayout; }
    float getSaliencyFactor() { return saliencyFactor; }
    float getMaxAmperage() { return maxAmperage; }
    float getSupplyVoltage() { return supplyVoltage; }


    void configureLayout(float videoWidth, float videoHeight) {
        // compute total LEDs by multiplying first, then round to nearest int
        int totalLeds = getLEDCount();
        
        // ensure total is even so it can evenly split across opposite edges
        if (totalLeds % 2 != 0) {
            totalLeds -= 1;
        }
        if (LEDCount == 0) LEDCount = totalLeds;

        int halfCount = totalLeds / 2;

        // Distribute halfCount exactly between horizontal and vertical
        LEDSHorizontal = std::round((videoWidth / (videoWidth + videoHeight)) * halfCount);
        LEDSVertical = halfCount - LEDSHorizontal;
    }

    private:
    float stripLength;
    float LEDDensity;
    int LEDCount;
    int LEDSHorizontal;
    int LEDSVertical;
    float border_percent;
    float resize_factor;
    int brightness;
    int* cornerLEDLayout;
    float saliencyFactor;
    float maxAmperage;
    float supplyVoltage;

};

int loadConfig(SetupParameters& params, const std::string& path = "config.txt");

