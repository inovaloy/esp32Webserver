#ifndef SD_CARD_HANDLER_H
#define SD_CARD_HANDLER_H

#include <FS.h>
#include <SD.h>

// Initialize SD card
bool initSDCard(int csPin = 5);

// Read file content from SD card
// Returns nullptr if file doesn't exist or can't be read
// Caller is responsible for freeing the returned buffer
char* readFileFromSD(const char* path, size_t* fileSize);

// Check if file exists on SD card
bool fileExistsOnSD(const char* path);

// List all files in SD card directory
void listSDDir(const char* dirname, uint8_t levels = 0);

#endif // SD_CARD_HANDLER_H
