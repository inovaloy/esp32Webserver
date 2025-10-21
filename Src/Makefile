SRC_DIR := $(ROOT)/Src

SKETCH = $(SRC_DIR)/esp32Webserver_SD.ino

# Add a file here = it will be compiled
SRCS = $(SRC_DIR)/webServer.cpp \
       $(SRC_DIR)/webServerHelper.cpp \
       $(SRC_DIR)/sdCardHandler.cpp \
       $(SRC_DIR)/webServerSD.cpp \
#        $(SRC_DIR)/AutoGen/autoGenWebServer.cpp \

CUSTOM_LIBS  = $(SRCS)
# EXCLUDE_DIRS = $(SRC_DIR)

# Only include necessary ESP32 libraries (not all of them)
LIBS = $(ESP32_PATH)/libraries/WiFi \
       $(ESP32_PATH)/libraries/Wire \
       $(ESP32_PATH)/libraries/SPI \
       $(ESP32_PATH)/libraries/SD \
       $(ESP32_PATH)/libraries/FS \
       $(ESP32_PATH)/libraries \
       $(ROOT)/Libs/Adafruit_SSD1306 \
       $(ROOT)/Libs/Adafruit-GFX-Library \
       $(ROOT)/Libs/Adafruit_BusIO

# Exclude unnecessary features to reduce binary size
# EXCLUDE_DIRS = $(SRC_DIR)

# Compiler optimization: Optimize for size (-Os) and disable debug
# BUILD_EXTRA_FLAGS = -Os -DCORE_DEBUG_LEVEL=0

ESP_ROOT = $(ESP32_PATH)
CHIP = esp32

UPLOAD_PORT = /dev/ttyUSB0
UPLOAD_SPEED = 115200

BUILD_ROOT = $(ROOT)/build
MAIN_NAME = esp32Webserver
BUILD_DIR = $(BUILD_ROOT)/$(MAIN_NAME)

include $(ESP_MAKE_PATH)/makeEspArduino.mk
