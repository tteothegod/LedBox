#include <iostream>
#include <istream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <csignal>
// OpenCV Headers - You were missing these!
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/saliency.hpp>

#include "led_strip.h"
#include "saveLoadConfig.hpp"

std::atomic<bool> run{true};

LEDStrip strip;

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

cv::Mat generateLedPreview(const cv::Mat& small_frame, const std::vector<cv::Vec3b>& colors, int smallFrameWidth, int smallFrameHeight, int ledsTop, int ledsRight, int ledsBottom, int ledsLeft) {
    constexpr float paddingFactor = 0.1;
    int sidePadding= std::round(smallFrameWidth * paddingFactor);
    int topPadding = std::round(smallFrameHeight * paddingFactor);

    int previewFrameWidth = smallFrameWidth + sidePadding * 2;
    int previewFrameHeight = smallFrameHeight + topPadding * 2;
    // cv::Mat constructor takes (rows, cols) which is (height, width)
    cv::Mat preview(previewFrameHeight, previewFrameWidth, CV_8UC3, cv::Scalar(0, 0, 0));

    float topIdealPixel = (float)smallFrameWidth / ledsTop;
    float rightIdealPixel = (float)smallFrameHeight / ledsRight;
    float bottomIdealPixel = (float)smallFrameWidth / ledsBottom;
    float leftIdealPixel = (float)smallFrameHeight / ledsLeft;

    for(int i = 0; i < ledsTop; ++i) { // For every top led
        int x0 = std::clamp((int)std::round(sidePadding + i * topIdealPixel), 0, previewFrameWidth);
        int x1 = std::clamp((int)std::round(sidePadding + (i + 1) * topIdealPixel), 0, previewFrameWidth); 
        cv::rectangle(preview, cv::Point(x0, 0), cv::Point(x1, topPadding), colors[i], -1); // formatted at x0 x1 y0 y1, (RGB)
    }
    for(int i = 0; i < ledsRight; ++i) {
        int y0 = std::clamp((int)std::round(topPadding + i * rightIdealPixel), 0, previewFrameHeight);
        int y1 = std::clamp((int)std::round(topPadding + (i + 1) * rightIdealPixel), 0, previewFrameHeight); 
        int x0 = std::max(0, previewFrameWidth - sidePadding);
        cv::rectangle(preview, cv::Point(x0, y0), cv::Point(previewFrameWidth, y1), colors[ledsTop + i], -1); // TODO: review bounds are correct to avoid seg fault
    }
    for(int i = 0; i < ledsBottom; ++i) {
        int x0 = std::clamp((int)std::round(sidePadding + (ledsBottom - 1 - i) * bottomIdealPixel), 0, previewFrameWidth);
        int x1 = std::clamp((int)std::round(sidePadding + (ledsBottom - i) * bottomIdealPixel), 0, previewFrameWidth);
        int y0 = std::max(0, previewFrameHeight - topPadding);
        cv::rectangle(preview, cv::Point(x0, y0), cv::Point(x1, previewFrameHeight), colors[ledsTop + ledsRight + i], -1);
    }
    for(int i = 0; i < ledsLeft; ++i) {
        int y0 = std::clamp((int)std::round(topPadding + (ledsLeft - 1 - i) * leftIdealPixel), 0, previewFrameHeight);
        int y1 = std::clamp((int)std::round(topPadding + (ledsLeft - i) * leftIdealPixel), 0, previewFrameHeight);
        cv::rectangle(preview, cv::Point(0, y0), cv::Point(sidePadding, y1), colors[ledsTop + ledsRight + ledsBottom + i], -1);
    }

    cv::Mat smallFrameRGB;
    // The input small_frame is BGR, but OpenCV rectangle colors are BGR, so we don't need to convert!
    // The colors vector is also BGR from the mean calculation.
    cv::Rect roi(sidePadding, topPadding, smallFrameWidth, smallFrameHeight);
    small_frame.copyTo(preview(roi));
    return preview;
}

void sigTerminate(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nSIGINT received, exiting gracefully..." << std::endl;
    } else {
        std::cout << "\nSIGTERM received, exiting gracefully..." << std::endl;
    }
    run.store(false);
}

int ledSyncVideo(bool savePicture) {

    struct sigaction sa;
    memset(&sa, 0 , sizeof(sa));
    sa.sa_handler = sigTerminate;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // TODO: Figure out a better way of doing this
    int frameCounter = 1;

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

    strip = LEDStrip(params.getLEDCount(), GPIO_PIN, DMA_NUM, params.getBrightness());
    strip.calibrateBrightness(params.getMaxAmperage(), params.getSupplyVoltage());

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


    int smallFrameWidth = (int) std::round(videoWidth * resize_factor);
    int smallFrameHeight = (int) std::round(videoHeight * resize_factor);
    int border_thickness = floor(smallFrameHeight * border_percent);

    std::vector<int> frameCorners = alignCornersToZero(layout, strip.size());
    // Define LED counts for each side based on frameCorners
    int leds_top = frameCorners[1] - frameCorners[0];
    int leds_right = frameCorners[2] - frameCorners[1];
    int leds_bottom = frameCorners[3] - frameCorners[2];
    int leds_left = strip.size() - frameCorners[3];
    std::cout << "LEDs per side: top=" << leds_top << " right=" << leds_right << " bottom=" << leds_bottom << " left=" << leds_left << std::endl;

    // Ideal pixel amounts per LED for each side
    float topIdealPixelAmt = (float)smallFrameWidth / leds_top;
    float rightIdealPixelAmt = (float)smallFrameHeight / leds_right;
    float bottomIdealPixelAmt = (float)smallFrameWidth / leds_bottom;
    float leftIdealPixelAmt = (float)smallFrameHeight / leds_left;

    while (run.load()) {
        if (!capture.read(frame)) {
            std::cout << "Video frame unavailable ending program...";
            break; // Break if looping also fails
        }
        frameCounter++; // Increment the frame counter

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
            // Use modulo to wrap around for the final LEDs on the left edge
            int roi_index = (frameCorners[3] + i) % num_leds;
            rois[roi_index] = cv::Rect(0, y0, std::min(border_thickness, smallFrameWidth), std::max(1, y1 - y0));
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
        if(savePicture && frameCounter % 30 == 0) { // Every 30 frames (approx. 1 second)
            cv::Mat preview = generateLedPreview(small_frame, colors, smallFrameWidth, smallFrameHeight, leds_top, leds_right, leds_bottom, leds_left);
            if (!cv::imwrite("preview.jpg", preview)) {
                std::cerr << "Warning: Failed to save preview.jpg" << std::endl;
            }
        }

        std::vector<cv::Vec3b> stripColors = transformIdealToStripVec(colors, layout);
        strip.updateStripWithGamma(stripColors);
        strip.show();
    }
    
    // Clean up pixels cleanly after the loop finishes instead of interrupting the middle of a frame
    if (strip.isInitialized()) {
        strip.clear();
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    try {
        bool savePicture = false;
        if (argc > 1) { // Check if there's at least one argument
            std::string arg1(argv[1]);
            if (arg1 == "--save-picture") {
                savePicture = true;
                std::cout << "Save picture flag enabled. Previews will be saved to preview.jpg" << std::endl;
            }
        }
        return ledSyncVideo(savePicture);
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
}
