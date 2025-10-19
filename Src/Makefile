SRC_DIR := $(ROOT)/Src
LIBS =  \
  $(ESP32_PATH)/libraries \
  $(ROOT)/Libs/Adafruit_SSD1306 \
  $(ROOT)/Libs/Adafruit-GFX-Library \
  $(ROOT)/Libs/Adafruit_BusIO \


UPLOAD_SPEED = 115200

ESP_ROOT = $(ESP32_PATH)
CHIP = esp32


SKETCH = $(SRC_DIR)/esp32Webserver.ino

UPLOAD_PORT = /dev/ttyUSB0


BUILD_ROOT = $(ROOT)/build
MAIN_NAME = esp32Webserver
BUILD_DIR = $(BUILD_ROOT)/$(MAIN_NAME)

include $(ESP_MAKE_PATH)/makeEspArduino.mk
