#include <iostream>
#include <fstream>
#include "led_strip.h"

using namespace std;

// gamma[256] contains your gamma8 table; accept pointer to table
float inverse_gamma_scale(uint8_t H, const uint8_t* gamma) {
    if (H <= gamma[0]) return 0.0f;
    if (H >= gamma[255]) return 1.0f;

    // find i where gamma[i] <= H <= gamma[i+1]
    int i = 0;
    while (i < 255 && gamma[i+1] < H) ++i;

    if (gamma[i+1] == gamma[i]) {
        // flat region: return i/255
        return (float)i / 255.0f;
    } else {
        float x = i + (float)(H - gamma[i]) / (float)(gamma[i+1] - gamma[i]);
        if (x < 0.0f) x = 0.0f;
        if (x > 255.0f) x = 255.0f;
        return x / 255.0f; // channelScale
    }
}


int main() {
    LEDStrip strip;
    if (!strip.init()) {
        cerr << "Failed to initialize LED strip." << endl;
        return 1;
    }

    cout << "Running basic LED test..." << endl;
    while (true) {
        int r = 0, g = 0, b = 0;
        cout << "(R, G, B) = ";
        cin >> r >> g >> b;

        for (int i = 0; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(r, g, b));
        }
        strip.show();
        const uint8_t* gamma = LEDStrip::getGammaTable();
        int channelR = round(inverse_gamma_scale(r, gamma) * 255);
        int channelG = round(inverse_gamma_scale(g, gamma) * 255);
        int channelB = round(inverse_gamma_scale(b, gamma) * 255);
        cout << "Gamma correction" << endl;
        
        cout << "Gamma-corrected channel scales for input (" << r << ", " << g << ", " << b << "): "
             << "R: " << channelR << ", "
             << "G: " << channelG << ", "
             << "B: " << channelB << endl;
    }

    return 0;
}
