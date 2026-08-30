SRC_DIR := $(ROOT)/src

SKETCH = $(SRC_DIR)/esp32DeviceHub.ino

# Add a file here = it will be compiled
SRCS = $(SRC_DIR)/webServer.cpp \
       $(SRC_DIR)/webServerHelper.cpp \
       $(SRC_DIR)/autoGen/autoGenWebServer.cpp \

CUSTOM_LIBS  = $(SRCS)
EXCLUDE_DIRS = $(SRC_DIR)

# Only include necessary ESP32 libraries (not all of them)
LIBS = $(ESP32_PATH)/libraries/WiFi \
       $(ESP32_PATH)/libraries/Wire \
       $(ESP32_PATH)/libraries/SPI \
       $(ROOT)/libs/Adafruit_SSD1306 \
       $(ROOT)/libs/Adafruit-GFX-Library \
       $(ROOT)/libs/Adafruit_BusIO

# Exclude unnecessary features to reduce binary size
EXCLUDE_DIRS = $(SRC_DIR)|SPITFT|SPIDevice|NetworkClientSecure|ssl_client|WebServer

# Compiler optimization: Optimize for size (-Os) and disable debug
BUILD_EXTRA_FLAGS = -Os -DCORE_DEBUG_LEVEL=0

ESP_ROOT = $(ESP32_PATH)
CHIP = esp32

UPLOAD_PORT = /dev/ttyUSB0
UPLOAD_SPEED = 115200

BUILD_ROOT = $(ROOT)/build
MAIN_NAME = esp32DeviceHub
BUILD_DIR = $(BUILD_ROOT)/$(MAIN_NAME)

include $(ESP_MAKE_PATH)/makeEspArduino.mk
