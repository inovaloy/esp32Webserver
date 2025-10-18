#include "SD_MMC.h"

void readSdCard()
{
    if (!SD_MMC.begin())
    {
        Serial.println("Failed to mount card");
        return;
    }

    File file = SD_MMC.open("/abc.txt", FILE_WRITE);

    if (!file)
    {
        Serial.println("Opening file to write failed");
        return;
    }

    if (file.print("Test file write"))
    {
        Serial.println("File write success");
    }
    else
    {
        Serial.println("File write failed");
    }

    file.close();

    File file2 = SD_MMC.open("/abc.txt", FILE_READ);

    if (!file2)
    {
        Serial.println("Opening file to read failed");
        return;
    }

    Serial.println("File Content:");

    while (file2.available())
    {
        Serial.write(file2.read());
    }

    file2.close();

    if(SD_MMC.mkdir("/test_folder"))
    {
        Serial.println("Folder created");
            File file = SD_MMC.open("/test_folder/test111.txt", FILE_WRITE);

            if (!file)
            {
                Serial.println("Opening file to write failed 12");
                return;
            }

            if (file.print("Looks great"))
            {
                Serial.println("File write success 12");
            }
            else
            {
                Serial.println("File write failed 12");
            }
    }
    else
    {
        Serial.println("Folder NOT created");
    }
}
