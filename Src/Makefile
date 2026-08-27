SRC_DIR := $(ROOT)/Src

SKETCH = $(SRC_DIR)/esp32Webserver.ino

# Add a file here = it will be compiled
SRCS = $(SRC_DIR)/webServer.cpp \
       $(SRC_DIR)/webServerHelper.cpp \
       $(SRC_DIR)/AutoGen/autoGenWebServer.cpp \

CUSTOM_LIBS  = $(SRCS)
EXCLUDE_DIRS = $(SRC_DIR)

# Only include necessary ESP32 libraries (not all of them)
LIBS = $(ESP32_PATH)/libraries/WiFi \
       $(ESP32_PATH)/libraries/Wire \
       $(ESP32_PATH)/libraries/SPI \
       $(ROOT)/Libs/Adafruit_SSD1306 \
       $(ROOT)/Libs/Adafruit-GFX-Library \
       $(ROOT)/Libs/Adafruit_BusIO

# Exclude unnecessary features to reduce binary size
EXCLUDE_DIRS = $(SRC_DIR)|SPITFT|SPIDevice|NetworkClientSecure|ssl_client|WebServer

# Compiler optimization: Optimize for size (-Os) and disable debug
BUILD_EXTRA_FLAGS = -Os -DCORE_DEBUG_LEVEL=0

ESP_ROOT = $(ESP32_PATH)
CHIP = esp32

# Flash mode/freq — qio+80m matches standard ESP32 DevKit modules.
# The DIO default from boards.txt causes "partition 0 invalid magic number"
# on most generic boards because the bootloader header byte is wrong.
FLASH_MODE = qio
FLASH_SPEED = 80m

UPLOAD_PORT = /dev/ttyUSB0
UPLOAD_SPEED = 460800

BUILD_ROOT = $(ROOT)/build
MAIN_NAME = esp32Webserver
BUILD_DIR = $(BUILD_ROOT)/$(MAIN_NAME)

include $(ESP_MAKE_PATH)/makeEspArduino.mk
