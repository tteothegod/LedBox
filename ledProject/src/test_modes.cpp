#include "test_modes.h"
#include <iostream>
#include <unistd.h>
#if OPENCV_BUILT
    #include <opencv2/opencv.hpp>
#endif
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

#include <opencv2/core/utility.hpp>
#include <opencv2/saliency.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/saliency/saliencyBaseClasses.hpp>
#include <opencv2/saliency/saliencySpecializedClasses.hpp>

using namespace cv;

#if OPENCV_BUILT
// Helper: cv::mean() returns cv::Scalar (double), but we need an 8-bit BGR triplet.
static inline cv::Vec3b meanBGR_u8(const cv::Mat& bgr)
{
    cv::Scalar m = cv::mean(bgr);
    return cv::Vec3b(
        cv::saturate_cast<uchar>(m[0]),
        cv::saturate_cast<uchar>(m[1]),
        cv::saturate_cast<uchar>(m[2])
    );
}
#endif

void TestModes::runBasicTest(LEDStrip& strip) {

    if (!strip.init()) {
        return;
    }
    std::cout << "Running basic LED test..." << std::endl;
    while (true) {
        // Red
        for (int i = 0; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(255, 0, 0));
        }
        strip.show();
        usleep(5000000);

        // Green
        for (int i = 0; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(0, 255, 0));
        }
        strip.show();
        usleep(5000000);

        // Blue
        for (int i = 0; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(0, 0, 255));
        }
        strip.show();
        usleep(5000000);
                for (int i = 0; i < strip.size(); ++i) {
            strip.setPixel(i, LEDStrip::Color(255, 255, 255));
        }
        strip.show();
        usleep(5000000);
    }
}

#if OPENCV_BUILT

void showLedPreview(const cv::Mat& frame,
                    const std::vector<cv::Vec3b>& ledColors,
                    int smallFrameWidth,
                    int smallFrameHeight,
                    int leds_horizontal,
                    int leds_vertical,
                    float topIdealPixelAmt,
                    float sideIdealPixelAmt) {
    int borderSize = 20;
    int total_leds = 2 * leds_horizontal + 2 * leds_vertical;
    cv::Mat vizMap(smallFrameHeight + 2 * borderSize, smallFrameWidth + 2 * borderSize, CV_8UC3, cv::Scalar(0, 0, 0));

    // Copy original small frame to the center
    cv::Rect roiCenter(borderSize, borderSize, smallFrameWidth, smallFrameHeight);
    frame.copyTo(vizMap(roiCenter));

    // Draw LEDs
    for (int i = 0; i < total_leds; ++i) {
        cv::Vec3b col = ledColors[i];
        cv::Scalar cvColor(col[0], col[1], col[2]);

        cv::Rect ledRect;
        if (i < leds_horizontal) {
            int idx = i;
            int x = std::round(idx * topIdealPixelAmt) + borderSize;
            int w = std::max(1, (int)std::round((idx + 1) * topIdealPixelAmt) - (int)std::round(idx * topIdealPixelAmt));
            ledRect = cv::Rect(x, 0, w, borderSize);
        } else if (i < leds_horizontal + leds_vertical) {
            int idx = i - leds_horizontal;
            int y = std::round(idx * sideIdealPixelAmt) + borderSize;
            int h = std::max(1, (int)std::round((idx + 1) * sideIdealPixelAmt) - (int)std::round(idx * sideIdealPixelAmt));
            ledRect = cv::Rect(smallFrameWidth + borderSize, y, borderSize, h);
        } else if (i < 2 * leds_horizontal + leds_vertical) {
            int idx = i - (leds_horizontal + leds_vertical);
            int x = smallFrameWidth + borderSize - std::round((idx + 1) * topIdealPixelAmt);
            int w = std::max(1, (int)std::round((idx + 1) * topIdealPixelAmt) - (int)std::round(idx * topIdealPixelAmt));
            ledRect = cv::Rect(x, smallFrameHeight + borderSize, w, borderSize);
        } else {
            int idx = i - (2 * leds_horizontal + leds_vertical);
            int y = smallFrameHeight + borderSize - std::round((idx + 1) * sideIdealPixelAmt);
            int h = std::max(1, (int)std::round((idx + 1) * sideIdealPixelAmt) - (int)std::round(idx * sideIdealPixelAmt));
            ledRect = cv::Rect(0, y, borderSize, h);
        }
        cv::rectangle(vizMap, ledRect, cvColor, cv::FILLED);
    }
    cv::imshow("LED Preview", vizMap);
};

void TestModes::runOpenCVTest(LEDStrip& strip, SetupParameters& params) {
    // Use a more robust GStreamer pipeline with decodebin to auto-detect format and use hardware decoding
    // std::string pipeline = "filesrc location=" + videoPath + " ! decodebin ! videoconvert ! appsink";
    std::cout << "Running OpenCV video test..." << std::endl;

std::string pipe =
    "v4l2src device=/dev/video0 do-timestamp=true ! "
    "image/jpeg,width=1280,height=720,framerate=30/1 ! "
    "jpegdec ! "
    "videoconvert ! "
    "video/x-raw,format=BGR ! "
    "appsink max-buffers=1 drop=true sync=false";

    cv::VideoCapture capture(pipe, cv::CAP_GSTREAMER);
    if (!capture.isOpened()) {
        std::cerr << "Error: Could not open video with GStreamer. Trying default backend." << std::endl;
        capture.open(0);
        if (!capture.isOpened()) {
            std::cerr << "Error: Could not open video file with any backend." << std::endl;
            return;
        }
    }
    if(capture.set(cv::CAP_PROP_BUFFERSIZE, 1)) {
        std::cout << "Successfully set buffer size." << std::endl;
    } else {
        std::cerr << "Warning: Could not set buffer size. May experience increased latency." << std::endl;
    }

    std::cout << "FOURCC: " << (int)capture.get(cv::CAP_PROP_FOURCC) << "\n";
    std::cout << "WxH: " << capture.get(cv::CAP_PROP_FRAME_WIDTH) << "x" << capture.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    if(capture.set(cv::CAP_PROP_CONVERT_RGB, 1) && capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y','U','Y','V'))) {
        std::cout << "Successfully set CAP_PROP_CONVERT_RGB to 1 and CAP_PROP_FOURCC to YUYV." << std::endl;
    } else {
        std::cerr << "Warning: Could not set CAP_PROP_CONVERT_RGB or CAP_PROP_FOURCC. Colors may be incorrect." << std::endl;
    }

    double fps = capture.get(cv::CAP_PROP_FPS);
    if (fps <= 0) {
        std::cerr << "Warning: Could not get video FPS. Defaulting to 30 FPS." << std::endl;
        fps = 30;
    }

    double videoWidth = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    double videoHeight = capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    if(videoWidth <= 0 || videoHeight <= 0) {
        std::cerr << "Warning: Could not get video width or height. shutting down process...";
        return;
    }

    params.configureLayout(videoWidth, videoHeight);

    int leds_horizontal = params.getLEDSHorizontal();
    int leds_vertical = params.getLEDSVertical();

    int total_leds = 2 * leds_horizontal + 2 * leds_vertical;
    strip.setCount(params.getLEDCount());

    if (strip.size() != total_leds) {
        std::cerr << "Error: LED strip size (" << strip.size()
                  << ") does not match the specified perimeter LED count (" << total_leds << ")." << std::endl;
        return;
    }

    if (!strip.init()) {
        std::cerr << "Error: Could not initialize LED strip." << std::endl;
        return;
    }
    std::cout << "LED strip initialized successfully." << std::endl;
    std::cout << "New LED count: " << strip.size() << std::endl;

    Ptr<saliency::StaticSaliencySpectralResidual> saliencyAlgorithm = makePtr<saliency::StaticSaliencySpectralResidual>();

    cv::Mat frame;

    // Define border thickness for sampling and visualization
    float border_percent = params.getBorderPercent(); // 25% by default
    float resize_factor = params.getResizeFactor(); // Resize to 25% of original size by default

    // TODO: Add interrupt handling to gracefully exit loop and clean up resources on Ctrl+C or window close
    while (true) {

        if (!capture.read(frame)) {
            std::cout << "Missed frame. ending program...";
            break; 
        }
        
        // Resize frame to 25% of original size using INTER_AREA for fast anti-aliased downsampling
        cv::Mat small_frame;
        cv::resize(frame, small_frame, cv::Size(), resize_factor, resize_factor, cv::INTER_AREA);
        int smallFrameWidth = small_frame.cols;
        int smallFrameHeight = small_frame.rows;
        float topIdealPixelAmt = (float) smallFrameWidth / leds_horizontal;
        float sideIdealPixelAmt = (float) smallFrameHeight / leds_vertical;
        int border_thickness = floor(smallFrameHeight * border_percent);

        cv::Mat gray_frame;
        cv::cvtColor(small_frame, gray_frame, cv::COLOR_BGR2GRAY);
        Mat weights;

        Mat saliencyMap;
        if (saliencyAlgorithm->computeSaliency(gray_frame, saliencyMap)) {
            // Convert to 32F explicitly to match our fast pointer math
            saliencyMap.convertTo(saliencyMap, CV_32F);
            weights = saliencyMap / 255.0f + 1.0f;
        } else {
            continue;
        }

        std::vector<cv::Vec3b> colors(total_leds);

        // Precompute ROIs for the LEDs
        std::vector<cv::Rect> rois(total_leds);
        for (int i = 0; i < leds_horizontal; ++i) {
            int x0 = std::clamp((int)std::round(i * topIdealPixelAmt), 0, smallFrameWidth);
            int x1 = std::clamp((int)std::round((i + 1) * topIdealPixelAmt), 0, smallFrameWidth);
            rois[i] = cv::Rect(x0, 0, std::max(1, x1 - x0), std::min(border_thickness, smallFrameHeight));
        }
        for (int i = 0; i < leds_vertical; ++i) {
            int y0 = std::clamp((int)std::round(i * sideIdealPixelAmt), 0, smallFrameHeight);
            int y1 = std::clamp((int)std::round((i + 1) * sideIdealPixelAmt), 0, smallFrameHeight);
            int x = std::max(0, smallFrameWidth - border_thickness);
            rois[leds_horizontal + i] = cv::Rect(x, y0, std::min(border_thickness, smallFrameWidth - x), std::max(1, y1 - y0));
        }
        for (int i = 0; i < leds_horizontal; ++i) {
            int x0 = std::clamp((int)std::round(smallFrameWidth - (i + 1) * topIdealPixelAmt), 0, smallFrameWidth);
            int x1 = std::clamp((int)std::round(smallFrameWidth - i * topIdealPixelAmt), 0, smallFrameWidth);
            if (x1 < x0) std::swap(x1, x0);
            int y = std::max(0, smallFrameHeight - border_thickness);
            rois[leds_horizontal + leds_vertical + i] = cv::Rect(x0, y, std::max(1, x1 - x0), std::min(border_thickness, smallFrameHeight - y));
        }
        for (int i = 0; i < leds_vertical; ++i) {
            int y0 = std::clamp((int)std::round(smallFrameHeight - (i + 1) * sideIdealPixelAmt), 0, smallFrameHeight);
            int y1 = std::clamp((int)std::round(smallFrameHeight - i * sideIdealPixelAmt), 0, smallFrameHeight);
            if (y1 < y0) std::swap(y1, y0);
            rois[2 * leds_horizontal + leds_vertical + i] = cv::Rect(0, y0, std::min(border_thickness, smallFrameWidth), std::max(1, y1 - y0));
        }

        // --- Calculate all LED colors in a single optimized loop using pointer access ---
        for (int i = 0; i < total_leds; ++i) {
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

        // cout all colors for debugging
        strip.updateStripWithGamma(colors);
        strip.show();
        // cv::waitKey(1);
    }
    // cv::destroyAllWindows();
}

void TestModes::runOpenCVTest2(LEDStrip& strip, SetupParameters& params) {
    // Use a more robust GStreamer pipeline with decodebin to auto-detect format and use hardware decoding
    // std::string pipeline = "filesrc location=" + videoPath + " ! decodebin ! videoconvert ! appsink";
    std::cout << "Running OpenCV video test..." << std::endl;

    std::string pipe =
        "v4l2src device=/dev/video0 ! "
        "image/jpeg,width=1280,height=720,framerate=30/1 ! "
        "jpegdec ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink drop=true sync=false";

    cv::VideoCapture capture(pipe, cv::CAP_GSTREAMER);
    if (!capture.isOpened()) {
        std::cerr << "Error: Could not open video with GStreamer. Trying default backend." << std::endl;
        capture.open(0);
        if (!capture.isOpened()) {
            std::cerr << "Error: Could not open video file with any backend." << std::endl;
            return;
        }
    }
    std::cout << "FOURCC: " << (int)capture.get(cv::CAP_PROP_FOURCC) << "\n";
    std::cout << "WxH: " << capture.get(cv::CAP_PROP_FRAME_WIDTH) << "x" << capture.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    if(capture.set(cv::CAP_PROP_CONVERT_RGB, 1) && capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y','U','Y','V'))) {
        std::cout << "Successfully set CAP_PROP_CONVERT_RGB to 1 and CAP_PROP_FOURCC to YUYV." << std::endl;
    } else {
        std::cerr << "Warning: Could not set CAP_PROP_CONVERT_RGB or CAP_PROP_FOURCC. Colors may be incorrect." << std::endl;
    }

    double fps = capture.get(cv::CAP_PROP_FPS);
    if (fps <= 0) {
        std::cerr << "Warning: Could not get video FPS. Defaulting to 30 FPS." << std::endl;
        fps = 30;
    }

    double videoWidth = capture.get(cv::CAP_PROP_FRAME_WIDTH);
    double videoHeight = capture.get(cv::CAP_PROP_FRAME_HEIGHT);

    if(videoWidth <= 0 || videoHeight <= 0) {
        std::cerr << "Warning: Could not get video width or height. shutting down process...";
        return;
    }

    params.configureLayout(videoWidth, videoHeight);

    int leds_horizontal = params.getLEDSHorizontal();
    int leds_vertical = params.getLEDSVertical();

    int total_leds = 2 * leds_horizontal + 2 * leds_vertical;
    strip.setCount(params.getLEDCount());

    if (strip.size() != total_leds) {
        std::cerr << "Error: LED strip size (" << strip.size()
                  << ") does not match the specified perimeter LED count (" << total_leds << ")." << std::endl;
        return;
    }

    if (!strip.init()) {
        std::cerr << "Error: Could not initialize LED strip." << std::endl;
        return;
    }
    std::cout << "LED strip initialized successfully." << std::endl;
    std::cout << "New LED count: " << strip.size() << std::endl;

    Ptr<saliency::StaticSaliencySpectralResidual> saliencyAlgorithm = makePtr<saliency::StaticSaliencySpectralResidual>();

    cv::Mat frame;

    // Define border thickness for sampling and visualization
    float border_percent = 0.25; // 25%
    float resize_factor = 0.25; // Resize to 25% of original size

    while (true) {

        if (!capture.read(frame)) {
            std::cout << "Video frame unavailable ending program...";
            break; // Break if looping also fails
        }

        // Resize frame for performance
        // Apply Gaussian blur before resizing to prevent aliasing  in saliency map
        float scaleSigma = 1/(2 * resize_factor);
        int kernelSize = 2 * ceil(3 * scaleSigma) + 1; // Ensure kernel size is odd
        cv::GaussianBlur(frame, frame, cv::Size(kernelSize, kernelSize), scaleSigma);

        // Resize frame to 25% of original size
        cv::Mat small_frame;
        cv::resize(frame, small_frame, cv::Size(), resize_factor, resize_factor, cv::INTER_LINEAR);
        int smallFrameWidth = small_frame.cols;
        int smallFrameHeight = small_frame.rows;
        float topIdealPixelAmt = (float) smallFrameWidth / leds_horizontal;
        float sideIdealPixelAmt = (float) smallFrameHeight / leds_vertical;
        int border_thickness = floor(smallFrameHeight * border_percent);

        std::vector<cv::Vec3b> colors(total_leds);

        // --- Apply colors to LEDs ---
        for (int i = 0; i < leds_horizontal; ++i) {

            int x0 = std::round(i * topIdealPixelAmt);
            int x1 = std::round((i + 1) * topIdealPixelAmt);
            x0 = std::clamp(x0, 0, smallFrameWidth);
            x1 = std::clamp(x1, 0, smallFrameWidth);
            int w = std::max(1, x1 - x0);
            int y = 0;
            int h = std::min(border_thickness, smallFrameHeight);

            cv::Rect top_roi(x0, y, std::min(w, smallFrameWidth - x0), h);
            cv::Mat roi_frame = small_frame(top_roi);

            colors[i] = meanBGR_u8(roi_frame);
        }

        // Right edge: split height into leds_vertical segments at x=width-border_thickness
        for (int i = 0; i < leds_vertical; ++i) {
            int y0 = std::round(i * sideIdealPixelAmt);
            int y1 = std::round((i + 1) * sideIdealPixelAmt);
            y0 = std::clamp(y0, 0, smallFrameHeight);
            y1 = std::clamp(y1, 0, smallFrameHeight);
            int h = std::max(1, y1 - y0);

            int x = std::max(0, smallFrameWidth - border_thickness);
            int w = std::min(border_thickness, smallFrameWidth - x);

            cv::Rect right_roi(x, y0, w, std::min(h, smallFrameHeight - y0));
            cv::Mat roi_frame = small_frame(right_roi);

            colors[leds_horizontal + i] = meanBGR_u8(roi_frame);
        }

        // Bottom edge: split width into leds_horizontal segments at y=height-border_thickness, scanning right->left
        for (int i = 0; i < leds_horizontal; ++i) {
            int x0 = std::round(smallFrameWidth - (i + 1) * topIdealPixelAmt);
            int x1 = std::round(smallFrameWidth - i * topIdealPixelAmt);
            x0 = std::clamp(x0, 0, smallFrameWidth);
            x1 = std::clamp(x1, 0, smallFrameWidth);
            if (x1 < x0) std::swap(x1, x0);
            int w = std::max(1, x1 - x0);

            int y = std::max(0, smallFrameHeight - border_thickness);
            int h = std::min(border_thickness, smallFrameHeight - y);

            cv::Rect bottom_roi(x0, y, std::min(w, smallFrameWidth - x0), h);
            cv::Mat roi_frame = small_frame(bottom_roi);

            colors[leds_horizontal + leds_vertical + i] = meanBGR_u8(roi_frame);
        }

        // Left edge: split height into leds_vertical segments at x=0, scanning bottom->top
        for (int i = 0; i < leds_vertical; ++i) {
            int y0 = std::round(smallFrameHeight - (i + 1) * sideIdealPixelAmt);
            int y1 = std::round(smallFrameHeight - i * sideIdealPixelAmt);
            y0 = std::clamp(y0, 0, smallFrameHeight);
            y1 = std::clamp(y1, 0, smallFrameHeight);
            if (y1 < y0) std::swap(y1, y0);
            int h = std::max(1, y1 - y0);

            int x = 0;
            int w = std::min(border_thickness, smallFrameWidth);

            cv::Rect left_roi(x, y0, w, std::min(h, smallFrameHeight - y0));
            cv::Mat roi_frame = small_frame(left_roi);

            colors[2 * leds_horizontal + leds_vertical + i] = meanBGR_u8(roi_frame);
        }

        // --- Visualization of Calculated LED Colors ---
        // showLedPreview(small_frame, colors, smallFrameWidth, smallFrameHeight, leds_horizontal, leds_vertical, topIdealPixelAmt, sideIdealPixelAmt);
        // ----------------------------------------------
        // cout all colors for debugging
        strip.updateStripWithGamma(colors);
        strip.show();
        // cv::waitKey(1);
    }
    cv::destroyAllWindows();
}

void TestModes::videoPropTest(LEDStrip& strip, SetupParameters& params) {
    std::cout << "Running video property test..." << std::endl;
    cv::VideoCapture capture(0, cv::CAP_GSTREAMER);
    if (!capture.isOpened()) {
        std::cerr << "Error: Could not open video with GStreamer. Trying default backend." << std::endl;
        capture.open(0);
        if (!capture.isOpened()) {
            std::cerr << "Error: Could not open video file with any backend." << std::endl;
            return;
        }
    }

    // Print as many properties as possible to see what might change when hdmi has video vs when it doesnt
    std::cout << "FOURCC: " << (int)capture.get(cv::CAP_PROP_FOURCC) << "\n";
    std::cout << "FPS: " << capture.get(cv::CAP_PROP_FPS) << "\n";
    std::cout << "Frame width: " << capture.get(cv::CAP_PROP_FRAME_WIDTH) << "\n";
    std::cout << "Frame height: " << capture.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";
    std::cout << "Is videoio open: " << capture.isOpened() << "\n";
    std::cout << "Backend: " << capture.getBackendName() << "\n";
    std::cout << "Buffer size: " << capture.get(cv::CAP_PROP_BUFFERSIZE) << "\n";
    std::cout << "Convert RGB: " << capture.get(cv::CAP_PROP_CONVERT_RGB) << "\n";


    // Print average brightness over 90 frames frames and any pixel that is non-zero to see if we are actually getting video frames or just black frames
    cv::Mat frame;
    cv::Mat sumFrame32f;
    constexpr int N = 90;
    for (int i = 0; i < N; ++i) {
        if (!capture.read(frame) || frame.empty()) {
            std::cerr << "Video frame unavailable ending program...\n";
            return;
        }

        cv::Mat frame32f;
        frame.convertTo(frame32f, CV_32FC3);

        if (sumFrame32f.empty()) {
            sumFrame32f = cv::Mat::zeros(frame32f.size(), frame32f.type());
        }

        cv::accumulate(frame32f, sumFrame32f);
    }

    cv::Mat meanFrame32f = sumFrame32f / static_cast<float>(N);
    cv::Scalar meanBrightness = cv::mean(meanFrame32f);

    // Count non-zero *channel bytes* in the last frame read (OK for "is it all black?")
    int nonZeroCount = cv::countNonZero(frame.reshape(1));

    std::cout << "Mean brightness (BGR): " << meanBrightness
              << " | Non-zero pixels: " << nonZeroCount << "\n";
}

void testStripLengthViaSoftware(LEDStrip& strip, SetupParameters& params) {
    // This is a software-only test to see if we can detect how long the LED strip is (in terms of how many LEDs) only via software and by interfacing with the LED strip
    // Done by: by monitoring the data signal and checking for responses from the LEDs, involves sending data to the strip and observing when no further data is received
    
    

}


#endif