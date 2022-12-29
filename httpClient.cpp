#include <WiFi.h>
#include <HTTPClient.h>

void getHttpClient()
{
    if ((WiFi.status() == WL_CONNECTED)) //Check the current connection status
    {

        HTTPClient http;
        http.begin("https://ialoy.com/admin/login"); //Specify the URL
        int httpCode = http.GET();

        if (httpCode > 0) //Check for the returning code
        {
            String payload = http.getString();
            Serial.println(httpCode);
            Serial.println(payload);
        }

        else
        {
            Serial.println("Error on HTTP request");
        }

        http.end(); //Free the resources
    }
}
