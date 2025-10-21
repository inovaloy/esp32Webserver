THIS_DIR      = $(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST)))))
TEMP_DIR      = $(THIS_DIR)/.temp
ESP32_PATH    = $(TEMP_DIR)/esp32
ESP_MAKE_PATH = $(TEMP_DIR)/espMake
ROOT          = $(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST)))))

PARAMS        = TEMP_DIR=$(TEMP_DIR) \
				ESP32_PATH=$(ESP32_PATH) \
				ESP_MAKE_PATH=$(ESP_MAKE_PATH) \
				ROOT=$(ROOT) \


all: help


autogen:
	python3 Scripts/compileHtml.py
	python3 Scripts/updateWebServer.py


sd-prepare:
	python3 Scripts/prepareSDCard.py


build: autogen
	cd Src && make ${PARAMS} && cd ..


flash: build
	cd Src && make flash ${PARAMS} && cd ..


configure:
	mkdir -p $(TEMP_DIR)
	@if [ -d "$(ESP32_PATH)" ]; then \
		echo "esp32 folder exists in .temp, updating submodules..."; \
		cd $(ESP32_PATH) && \
		git submodule update --init && \
		cd tools && \
		python3 get.py; \
	else \
		echo "esp32 folder does not exist in .temp, cloning..."; \
		cd $(TEMP_DIR) && \
		git clone https://github.com/espressif/arduino-esp32.git esp32 && \
		cd esp32 && \
		git submodule update --init && \
		cd tools && \
		python3 get.py; \
	fi
	@if [ -d "$(ESP_MAKE_PATH)" ]; then \
		echo "espMake folder exists in .temp, updating..."; \
		cd $(ESP_MAKE_PATH) && \
		git pull; \
	else \
		echo "espMake folder does not exist in .temp, cloning..."; \
		cd $(TEMP_DIR) && \
		git clone https://github.com/plerup/makeEspArduino.git espMake; \
	fi


clean:
	cd Src && make clean ${PARAMS} && cd ..
	rm -rf ${ROOT}/Src/AutoGen/
	rm -rf ${ROOT}/build/
	rm -rf ${TEMP_DIR}/AutoGen/


help:
	@echo "Available targets:"
	@echo "  autogen    - Generate necessary files for the web server"
	@echo "  sd-prepare - Prepare HTML files for SD card deployment"
	@echo "  build      - Compile the sketch"
	@echo "  flash      - Upload the compiled sketch to the ESP32 board"
	@echo "  configure  - Set up the ESP32 Arduino environment"
	@echo "  clean      - Clean up the build artifacts"
	@echo "  help       - Show this help message"

.PHONY: all autogen build flash configure clean help
