CC = g++
# Compiler flags (no linker flags here)
FLAGS = -Ofast -march=native -Wall

# rpi ws281x directory
RPI_WS281X_DIR = $(HOME)/rpi_ws281x

# Use pkg-config to get OpenCV flags correctly
OPENCV_FLAGS = $(shell pkg-config --cflags --libs opencv4)

# Base libraries for the LED lights and OpenCV
LIBS = -L$(RPI_WS281X_DIR) -lws2811 -lwiringPi -lopencv_saliency $(OPENCV_FLAGS)

# --- Target Definitions ---
TARGET_LSV = ledSyncVideo
TARGET_CALIBRATE = calibrate

# --- Source File Definitions ---
# Common source files used by multiple targets
COMMON_SRC = src/led_strip.cpp src/saveLoadConfig.cpp

# Target-specific source files
SRC_LSV = src/ledSyncVideo.cpp
SRC_CALIBRATE = src/calibrate.cpp

# --- Object File Definitions ---
OBJ_COMMON = $(COMMON_SRC:.cpp=.o)
OBJ_LSV = $(SRC_LSV:.cpp=.o)
OBJ_CALIBRATE = $(SRC_CALIBRATE:.cpp=.o)

# --- Build Rules ---
all: $(TARGET_LSV) $(TARGET_CALIBRATE)

# Rule to link the ledSyncVideo app
$(TARGET_LSV): $(OBJ_LSV) $(OBJ_COMMON)
	$(CC) $(FLAGS) -o $(TARGET_LSV) $^ $(LIBS)

# Rule to link the calibrate app
$(TARGET_CALIBRATE): $(OBJ_CALIBRATE) $(OBJ_COMMON)
	$(CC) $(FLAGS) -o $(TARGET_CALIBRATE) $^ $(LIBS)

# Generic rule to compile .cpp files into .o object files
%.o: %.cpp
	$(CC) $(FLAGS) $(OPENCV_FLAGS) -I$(RPI_WS281X_DIR) -c $< -o $@

# --- Run and Clean Commands ---
run-lsv: $(TARGET_LSV)
	./$(TARGET_LSV)

run-calibrate: $(TARGET_CALIBRATE)
	./$(TARGET_CALIBRATE)

clean:
	rm -f $(TARGET_LSV) $(TARGET_CALIBRATE) *.o
