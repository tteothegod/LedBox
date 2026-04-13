#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <cstdint>
#include <vector>
#include "ws2811.h"
#include <opencv2/core.hpp>

#define LED_COUNT 210
#define STRIP_LENGTH 3.5 // meters
#define LED_DENSITY 60
#define GPIO_PIN 18
#define DMA_NUM 14

constexpr float wattsPerPixel = 0.2; // 0.2 Watts

class LEDStrip {
public:
    LEDStrip();
    LEDStrip(int count, int gpio_pin, int dmanum, int brightness = 255);
    LEDStrip(const LEDStrip& other);
    LEDStrip& operator=(const LEDStrip& other);
    LEDStrip(LEDStrip&& other) noexcept;
    LEDStrip& operator=(LEDStrip&& other) noexcept;
    ~LEDStrip();

    void setCount(int count);

    bool init();
    void show();
    void clear();
    void setPixel(int idx, uint32_t color);
    void updateStrip(const std::vector<cv::Vec3b>& colors, bool temporalSmoothOn = true);
    void updateStripWithGamma(const std::vector<cv::Vec3b>& colors, bool temporalSmoothOn = true);
    // Per-channel scale factors in range [0.0, 1.0]. Applied to incoming RGB before output/gamma.
    void setChannelScales(float r_scale, float g_scale, float b_scale);
    void setBrightness(int brightness);
    int size() const;
    void calibrateBrightness(float maxAmperage, float supplyVoltage);
    float temporalSmooth(float valueNew, float valueOld);


    int getBrightness() const;
    bool isInitialized() const;

    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b);
    // Return pointer to internal gamma table (256 entries)
    static const uint8_t* getGammaTable();

private:
    ws2811_t ledstring;
    bool is_initialized = false;
    static const uint8_t gamma8[];
    std::vector<cv::Vec3b> previousColor;
    float temporalAlpha;
    // Per-channel scale factors (0.0 - 1.0)
    float channelScaleR;
    float channelScaleG;
    float channelScaleB;
};

#endif // LED_STRIP_H
