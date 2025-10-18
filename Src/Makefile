# config.mk
# THIS_DIR := $(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST)))))
# ROOT := $(THIS_DIR)/..
SRC_DIR := $(ROOT)/Src
LIBS =  \
  $(ESP32_PATH)/libraries
#   $(ROOT)/ext_lib

UPLOAD_SPEED = 115200

ESP_ROOT = $(ESP32_PATH)
CHIP = esp32


SKETCH = $(SRC_DIR)/esp32Webserver.ino

UPLOAD_PORT = /dev/ttyUSB0
# BOARD = esp32

BUILD_ROOT = $(ROOT)/build
MAIN_NAME = esp32Webserver
BUILD_DIR = $(BUILD_ROOT)/$(MAIN_NAME)

include $(ROOT)/.temp/espMake/makeEspArduino.mk

# all:
# 	@echo "root: $(ROOT)"

