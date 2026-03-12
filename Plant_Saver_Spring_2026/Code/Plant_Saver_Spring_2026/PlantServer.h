#ifndef PLANTSERVER_H
#define PLANTSERVER_H

/*--------- Required Libraries for PlantServer ---------*/
// Install via Arduino IDE Library Manager:
// - ESPAsyncWebServer (by me-no-dev)
// - AsyncTCP (by me-no-dev)
// - ArduinoJson (by Benoit Blanchon)

/*--------- WiFi Configuration ---------*/
extern const char* ssid;
extern const char* password;

/*--------- Function Declarations ---------*/
void setupWiFi();
void setupWebServer();
void initializePlantServer();
void handlePlantServer();
bool loadPlantsFromJSON(const char* filename);

#endif