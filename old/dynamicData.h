#ifndef DYNAMIC_DATA_H
#define DYNAMIC_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations
void updateSensorData();
char* getSystemInfoJson();
char* getSensorDataJson();
char* getNetworkInfoJson();
char* getControlStatusJson();
bool handleDeviceControl(const char* device, const char* action);

#ifdef __cplusplus
}
#endif

#endif // DYNAMIC_DATA_H