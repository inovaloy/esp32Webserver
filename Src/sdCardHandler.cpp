#include "sdCardHandler.h"
#include "Arduino.h"
#include <SPI.h>

bool initSDCard(int csPin) {
    Serial.println("Initializing SD card...");

    if (!SD.begin(csPin)) {
        Serial.println("SD Card Mount Failed!");
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached!");
        return false;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

    return true;
}

char* readFileFromSD(const char* path, size_t* fileSize) {
    File file = SD.open(path);

    if (!file) {
        Serial.printf("Failed to open file: %s\n", path);
        if (fileSize) *fileSize = 0;
        return nullptr;
    }

    if (file.isDirectory()) {
        Serial.printf("Path is a directory, not a file: %s\n", path);
        file.close();
        if (fileSize) *fileSize = 0;
        return nullptr;
    }

    size_t size = file.size();
    if (size == 0) {
        Serial.printf("File is empty: %s\n", path);
        file.close();
        if (fileSize) *fileSize = 0;
        return nullptr;
    }

    // Allocate buffer for file content
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        Serial.printf("Failed to allocate memory for file: %s (size: %d)\n", path, size);
        file.close();
        if (fileSize) *fileSize = 0;
        return nullptr;
    }

    // Read file content
    size_t bytesRead = file.read((uint8_t*)buffer, size);
    file.close();

    if (bytesRead != size) {
        Serial.printf("Failed to read complete file: %s (read: %d, expected: %d)\n",
                      path, bytesRead, size);
        free(buffer);
        if (fileSize) *fileSize = 0;
        return nullptr;
    }

    buffer[size] = '\0'; // Null-terminate (useful for text files)

    if (fileSize) {
        *fileSize = size;
    }

    Serial.printf("Successfully read file: %s (size: %d bytes)\n", path, size);
    return buffer;
}

bool fileExistsOnSD(const char* path) {
    File file = SD.open(path);
    if (!file) {
        return false;
    }

    bool isFile = !file.isDirectory();
    file.close();
    return isFile;
}

void listSDDir(const char* dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = SD.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }

    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listSDDir(file.path(), levels - 1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
    root.close();
}
