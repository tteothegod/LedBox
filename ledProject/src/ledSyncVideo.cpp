#include <iostream>
#include <istream>
#include <vector>
#include <algorithm>
#include <cmath>

// OpenCV Headers - You were missing these!
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/saliency.hpp>

#include "led_strip.h"
#include "saveLoadConfig.hpp"



static constexpr const char* kPipe = "v4l2src device=/dev/video0 do-timestamp=true ! "
    "image/jpeg,width=1280,height=720,framerate=30/1 ! "
    "jpegdec ! "
    "videoconvert ! "
    "video/x-raw,format=BGR ! "
    "appsink max-buffers=1 drop=true sync=false";


std::vector<cv::Vec3b> transformIdealToStripVec(const std::vector<cv::Vec3b>& colors, int* corners) {
    std::vector<cv::Vec3b> outputStrip = colors;
    int n = static_cast<int>(colors.size());
    if (n == 0) return outputStrip;
    int shift = corners[0] % n;
    std::rotate(outputStrip.rbegin(), outputStrip.rbegin() + shift, outputStrip.rend());
    return outputStrip; 
}

std::vector<int> alignCornersToZero(int* corners, int totalSize) {
    std::vector<int> aligned(4);
    
    // The offset is the value of the first corner in your desired sequence
    int offset = corners[0];

    for (int i = 0; i < 4; ++i) {
        // Subtract the offset to bring corners[0] to 0.
        // We add totalSize before the modulo to handle cases where 
        // the subtraction results in a negative number (standard circular buffer logic).
        aligned[i] = (corners[i] - offset + totalSize) % totalSize;
    }

    return aligned;
}

cv::Vec3b meanBGR_u8(const cv::Mat& roi) {
    cv::Scalar m = cv::mean(roi);
    return cv::Vec3b(
        cv::saturate_cast<uchar>(m[0]),
        cv::saturate_cast<uchar>(m[1]),
        cv::saturate_cast<uchar>(m[2])
    );
}

int ledSyncVideo() {

    std::cout << "Running main led sync video program..." << std::endl;
    SetupParameters params;
    if (!loadConfig(params)) {
        std::cerr << "Error: ledSyncVideo failed to load config. shutting down process..." << std::endl;
        return -1;
    }
    std::cout << "Config loaded successfully." << std::endl;

    // Defaults for runtime image processing; adjust if you add getters to SetupParameters
    float resize_factor = params.getResizeFactor();
    float border_percent = params.getBorderPercent();
    float saliencyFactor = params.getSaliencyFactor(); 
    // TODO: Implement saliency factor

    /******** Set up video capture ********/
    cv::VideoCapture capture(kPipe, cv::CAP_GSTREAMER);
    if (!capture.isOpened()) {
        std::cerr << "Error: Could not open video with GStreamer. Trying default backend." << std::endl;
        capture.open(0);
        if (!capture.isOpened()) {
            std::cerr << "Error: Could not open video file with any backend." << std::endl;
            return -1;
        }
    }

    // TODO: change this to having all of the positions saved in config 
    //       and just read them in instead of calculating them on the fly every frame. would be more efficient and also allow for more complex layouts in the future
    /******** Get video dimensions and configure led layout ********/
    double videoWidth = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    double videoHeight = capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "Frame WxH: " << capture.get(cv::CAP_PROP_FRAME_WIDTH) << "x" << capture.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    if(videoWidth <= 0 || videoHeight <= 0) {
        std::cerr << "Warning: Could not get video width or height. attempting to get values...";
        cv::Mat probe;
        if (capture.read(probe) && !probe.empty()) {
            videoWidth = probe.cols;
            videoHeight = probe.rows;
            std::cerr << "Got size from probe frame: " << videoWidth << "x" << videoHeight << std::endl;
        } else {
            std::cerr << "Error: Unable to read probe frame for size. shutting down process..." << std::endl;
            return -1;
        }
        return -1 ;
    }

    int* layout = params.getCornerLEDLayout();
    if((params.getLength() <= 0.0f || params.getDensity() <= 0.0f) 
    && ( params.getLEDCount() <= 0 )||( layout == nullptr || layout[0] == -1 || layout[1] == -1 || layout[2] == -1 || layout[3] == -1)) {
        std::cerr << "Error: Invalid configuration values." << std::endl;
        return -1;
    }    
    if(params.getLEDCount() > 0) {
        std::cout << "Using LED count from config: " << params.getLEDCount() << std::endl;
    } else {
        std::cout << "Using LED count calculated from length and density: " << params.getLength() * params.getDensity() << std::endl;
        params.configureLayout(videoWidth, videoHeight); // Default to video dimensions if count isn't specified but length and density are
    }

    LEDStrip strip(params.getLEDCount(), GPIO_PIN, DMA_NUM, params.getBrightness());
    strip.configureBrightness(params.getMaxAmperage(), params.getSupplyVoltage());

    /******** Initialize LED strip ********/
    if (!strip.init()) {
        std::cerr << "Error: Could not initialize LED strip." << std::endl;
        return -1;
    }
    std::cout << "LED strip initialized successfully." << std::endl;


    //******** Program initialization ********/
    cv::Ptr<cv::saliency::StaticSaliencySpectralResidual> saliencyAlgorithm = cv::makePtr<cv::saliency::StaticSaliencySpectralResidual>();
    cv::Mat frame;
    // Define border thickness for sampling and visualization


    int smallFrameWidth = (int) std::round(videoWidth * resizeFactor);
    int smallFrameHeight = (int) std::round(videoHeight * resizeFactor);
    int border_thickness = floor(smallFrameHeight * border_percent);

    std::vector<int> frameCorners = alignCornersToZero(layout, strip.size());
    // Define LED counts for each side based on frameCorners
    int leds_top = frameCorners[1] - frameCorners[0];
    int leds_right = frameCorners[2] - frameCorners[1];
    int leds_bottom = frameCorners[3] - frameCorners[2];
    int leds_left = strip.size() - frameCorners[3];

    // Ideal pixel amounts per LED for each side
    float topIdealPixelAmt = (float)smallFrameWidth / leds_top;
    float rightIdealPixelAmt = (float)smallFrameHeight / leds_right;
    float bottomIdealPixelAmt = (float)smallFrameWidth / leds_bottom;
    float leftIdealPixelAmt = (float)smallFrameHeight / leds_left;

    while (true) {
        if (!capture.read(frame)) {
            std::cout << "Video frame unavailable ending program...";
            break; // Break if looping also fails
        }
        // Resize frame to 25% of original size using INTER_AREA for fast anti-aliased downsampling
        cv::Mat small_frame;
        cv::resize(frame, small_frame, cv::Size(), resize_factor, resize_factor, cv::INTER_AREA);

        cv::Mat gray_frame;
        cv::cvtColor(small_frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::Mat weights;

        cv::Mat saliencyMap;
        if (saliencyAlgorithm->computeSaliency(gray_frame, saliencyMap)) {
            // Convert to 32F explicitly to match our fast pointer math
            saliencyMap.convertTo(saliencyMap, CV_32F);
            weights = saliencyMap * (10.0f * saliencyFactor/ 255.0f) + 1.0f; // Scale saliency to a reasonable range and add a base weight of 1.0 to ensure non-salient areas still contribute
        } else {
            continue;
        }

        int num_leds = strip.size();

        std::vector<cv::Vec3b> colors(num_leds);

        // Precompute ROIs for the LEDs based on frameCorners
        std::vector<cv::Rect> rois(num_leds);

        // Top edge
        for (int i = 0; i < leds_top; ++i) {
            int x0 = std::clamp((int)std::round(i * topIdealPixelAmt), 0, smallFrameWidth);
            int x1 = std::clamp((int)std::round((i + 1) * topIdealPixelAmt), 0, smallFrameWidth);
            rois[frameCorners[0] + i] = cv::Rect(x0, 0, std::max(1, x1 - x0), std::min(border_thickness, smallFrameHeight));
        }

        // Right edge
        for (int i = 0; i < leds_right; ++i) {
            int y0 = std::clamp((int)std::round(i * rightIdealPixelAmt), 0, smallFrameHeight);
            int y1 = std::clamp((int)std::round((i + 1) * rightIdealPixelAmt), 0, smallFrameHeight);
            int x = std::max(0, smallFrameWidth - border_thickness);
            rois[frameCorners[1] + i] = cv::Rect(x, y0, std::min(border_thickness, smallFrameWidth - x), std::max(1, y1 - y0));
        }

        // Bottom edge
        for (int i = 0; i < leds_bottom; ++i) {
            int x0 = std::clamp((int)std::round(smallFrameWidth - (i + 1) * bottomIdealPixelAmt), 0, smallFrameWidth);
            int x1 = std::clamp((int)std::round(smallFrameWidth - i * bottomIdealPixelAmt), 0, smallFrameWidth);
            if (x1 < x0) std::swap(x1, x0);
            int y = std::max(0, smallFrameHeight - border_thickness);
            rois[frameCorners[2] + i] = cv::Rect(x0, y, std::max(1, x1 - x0), std::min(border_thickness, smallFrameHeight - y));
        }

        // Left edge
        for (int i = 0; i < leds_left; ++i) {
            int y0 = std::clamp((int)std::round(smallFrameHeight - (i + 1) * leftIdealPixelAmt), 0, smallFrameHeight);
            int y1 = std::clamp((int)std::round(smallFrameHeight - i * leftIdealPixelAmt), 0, smallFrameHeight);
            if (y1 < y0) std::swap(y1, y0);
            rois[frameCorners[3] + i] = cv::Rect(0, y0, std::min(border_thickness, smallFrameWidth), std::max(1, y1 - y0));
        }

        // --- Calculate all LED colors in a single optimized loop using pointer access ---
        for (int i = 0; i < num_leds; ++i) {
            cv::Rect roi = rois[i];
            
            cv::Mat roi_frame = small_frame(roi);
            cv::Mat roi_saliency = weights(roi);

            double total_saliency = 0;
            double wb = 0, wg = 0, wr = 0;

            int r_rows = roi_frame.rows;
            int r_cols = roi_frame.cols;
            
            // Check if both mats are continuous to flatten matrix iterations into a 1D loop
            if (roi_frame.isContinuous() && roi_saliency.isContinuous()) {
                r_cols *= r_rows;
                r_rows = 1;
            }

            for (int row = 0; row < r_rows; ++row) {
                // Highly efficient raw pointer arithmetic replacing CPU-heavy `.at<...>` calls
                const cv::Vec3b* ptr_color = roi_frame.ptr<cv::Vec3b>(row);
                const float* ptr_sal = roi_saliency.ptr<float>(row);
                
                for (int col = 0; col < r_cols; ++col) {
                    float sal = ptr_sal[col];
                    total_saliency += sal;
                    
                    const cv::Vec3b& c = ptr_color[col];
                    wb += c[0] * sal;
                    wg += c[1] * sal;
                    wr += c[2] * sal;
                }
            }

            if (total_saliency > 0) {
                colors[i] = cv::Vec3b(
                    cv::saturate_cast<uchar>(std::round(wb / total_saliency)),
                    cv::saturate_cast<uchar>(std::round(wg / total_saliency)),
                    cv::saturate_cast<uchar>(std::round(wr / total_saliency))
                );
            } else {
                colors[i] = meanBGR_u8(roi_frame);
            }
        }
        // // --- Visualization of Calculated LED Colors ---
        // showLedPreview(small_frame, colors, smallFrameWidth, smallFrameHeight, leds_horizontal, leds_vertical, topIdealPixelAmt, sideIdealPixelAmt);
        // ----------------------------------------------

        std::vector<cv::Vec3b> stripColors = transformIdealToStripVec(colors, layout);
        strip.updateStripWithGamma(transformIdealToStripVec(colors, layout));
        strip.show();
    }

    return 0;
}

int main() {
    try {
        return ledSyncVideo();
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
}
