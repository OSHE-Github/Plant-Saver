#include "Arduino.h"
#include "PlantSaverClasses.h"
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
/*------------------------------------------------------ Object Instantiation -----------------------------------------------------*/

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  // Create OLED display object

/*------------------------------------------------------------------- Error Class ------------------------------------------------------------------*/

// Initialization
Error::Error()
  : _errorList{}, highestPriority{}, _flashCt{}, _indicatorOn{}, _startTime{} {
  _flashDuration = 100;
}

// Check for presence of a specific error
int Error::getError(int errorStatus) {
  return _errorList[errorStatus];
}

// Add a new error to the list, update highest priority error
void Error::addError(int errorStatus) {
  if (!_errorList[errorStatus]) {
    _errorList[errorStatus] = errorStatus;
    if (errorStatus > highestPriority) {
      highestPriority = errorStatus;
    }
  }
}

// Clear the presence of a specific error
void Error::clearError(int errorStatus) {
  _errorList[errorStatus] = noError;
  highestPriority = 0;
  for (int i = errorStatus; i > 0; i--) {
    if (_errorList[i]) {
      highestPriority = _errorList[i];
      return;
    }
  }
}

// Flash the indicator LED a number of times equal to the highest priority error code
void Error::indicateError() {
  if (highestPriority == noError) {
    digitalWrite(ERROR_IND_PIN, LOW);
    _indicatorOn = 0;
    return;
  }
  unsigned long currentTime = millis();
  if (_flashCt < highestPriority) {
    if (!_indicatorOn && currentTime - _startTime >= 600) {
      digitalWrite(ERROR_IND_PIN, HIGH);
      _startTime = currentTime;
      _indicatorOn = 1;
    } else if (_indicatorOn && currentTime - _startTime >= 600) {
      digitalWrite(ERROR_IND_PIN, LOW);
      _startTime = currentTime;
      _flashCt++;
      _indicatorOn = 0;
    }
  } else {
    if (currentTime - _startTime >= 3000) {
      _startTime = currentTime;
      _flashCt = 0;
    }
  }
}

/*---------------------------------------------------------- Plant Class ----------------------------------------------------------*/

// Initialization
Plant::Plant(Error& errorRef)
  : commonName{}, scientificName{}, lightReq{}, waterReq{}, hardiness{},
    avgLightQuadrants{}, error(errorRef) {
  id = 0;
  avgLight = 0;
  avgWater = 0;
  avgHumidity = 0;
  avgTemp = 0;
  plantPulled = 0;
}

// Take average of sensor readings
float Plant::getAvgReading(JsonDocument& sensorDoc) {
  float avg = 0;
  int numReadings = sensorDoc["numReadings"];
  if (numReadings > 0) {
    JsonArray readings = sensorDoc["readings"];
    for (int i = 0; i < numReadings; i++) {
      avg = avg + (float)readings[i];
    }
    avg = avg / (float)numReadings;
  } else {
    avg = 0; // Prevent divide by 0 errors
  }
  return avg;
}

//
// TODO: Calibration File
//

// Map light requirements to thresholds, then check average reading
//
// TODO: Re-vamp to work w/ solar cells
//
void Plant::lightCheck(int threshold[2]) {
  threshold[0] = -1;
  threshold[1] = -1;
  int lightReqLowHigh[2] = { 0 };  // [0] = low value, [1] = high value
  lightReqLowHigh[0] = lightReq[0];
  lightReqLowHigh[1] = (lightReq[1] != 0) ? lightReq[1] : lightReq[0];
  for (int i = 0; i < 2; i++) {
    switch (lightReqLowHigh[i]) {
      case fullShade:
        threshold[i] = LIGHT_MIN + 1075 * i;  // 0 to 1075 lux
        break;
      case partialSun:
        threshold[i] = 1075 + 9675 * i;  // 1075 to 10750 lux
        break;
      case fullSun:
        threshold[i] = 10750 + (LIGHT_MAX - 10750) * i;  // 10750+ , TODO: Top end?
    }
  }
}

// Map hardiness zones to temperature thresholds, then check average reading
void Plant::tempCheck(int threshold[2]) {
  threshold[0] = -1;
  threshold[1] = -1;
  int hardinessLowHigh[2];  // [0] = low value, [1] = high value
  hardinessLowHigh[0] = hardiness[0];
  hardinessLowHigh[1] = (hardiness[1] != 0) ? hardiness[1] : hardiness[0];
  for (int i = 0; i < 2; i++) {
    switch (hardinessLowHigh[i]) {
      case 2:
        threshold[i] = 26 + 4 * i;
        break;
      case 3:
        threshold[i] = 32 + 4 * i;
        break;
      case 4:
        threshold[i] = 39 + 4 * i;
        break;
      case 5:
        threshold[i] = 45 + 3 * i;
        break;
      case 6:
        threshold[i] = 50 + 4 * i;
        break;
      case 7:
        threshold[i] = 54 + 3 * i;
        break;
      case 8:
        threshold[i] = 61 + 3 * i;
        break;
      case 9:
        threshold[i] = 64 + 4 * i;
        break;
      case 10:
        threshold[i] = 68 + 4 * i;
        break;
      case 11:
        threshold[i] = 75 + 4 * i;
        break;
      case 12:
        threshold[i] = 80 + 20 * i;
        break;
      case 13:
        threshold[i] = 80 + (TEMP_MAX - 80) * i;
    }
  }
}

// Map water requirements and humidity readings to thresholds, then check average readings
// Water thresholds are created to work with inverted reading values, such that dry < wet
// This is due to the fact that capacitance is inversely proportional to water saturation
void Plant::waterCheck(int threshold[2]) {
  threshold[0] = -1;
  threshold[1] = -1;
  int waterReqLowHigh[2];  // [0] = low value, [1] = high value
  waterReqLowHigh[0] = waterReq[0];
  waterReqLowHigh[1] = (waterReq[1] != 0) ? waterReq[1] : waterReq[0];
  for (int i = 0; i < 2; i++) {
    switch (waterReqLowHigh[i]) {
      case dry:
        threshold[i] = WATER_MIN + 1795 * i; // 0 to 1795 (raw range 2300 to 4095)
        break;
      case moist:
        threshold[i] = 1795 + 650 * i; // 1795 to 2445 (raw range 1650 to 2300)
        break;
      case wet:
        threshold[i] = 2445 + 650 * i; // 2445 to 3095 (raw range 1000 to 1650)
        break;
      case water:
        threshold[i] = 3095 + 4095*i; // 3095 to 4095 (raw range 0 to 1000)
        break;
    }
  }
}

// Check average humidity values against static thresholds
void Plant::humidityCheck(int threshold[2]) {
  threshold[0] = 30;
  threshold[1] = 60;
  // TODO: What do?
}

// Calculate the average light out of the four quadrants
void Plant::calcAvgLight() {
  avgLight = (avgLightQuadrants[0] + avgLightQuadrants[1] + avgLightQuadrants[2] + avgLightQuadrants[3]) / 4.0;
}

// Pull data from the plant file of the active plant's folder and parse it into a plant object
void Plant::pullPlant() {
  JsonDocument plantDoc = readSDFile(PLANT_PATH);
  if (plantDoc.isNull()) {
    error.addError(fileOperation);
    return;
  }
  id = plantDoc["id"];
  const char* jsonCommonName = plantDoc["commonName"];
  snprintf(commonName, NUM_CHARS_NAME, "%s", jsonCommonName);
  const char* jsonScientificName = plantDoc["scientificName"];
  snprintf(scientificName, NUM_CHARS_NAME, "%s", jsonScientificName);
  lightReq[0] = plantDoc["lightReq"][0];
  lightReq[1] = plantDoc["lightReq"][1];
  waterReq[0] = plantDoc["waterReq"][0];
  waterReq[1] = plantDoc["waterReq"][1];
  hardiness[0] = plantDoc["hardiness"][0];
  hardiness[1] = plantDoc["hardiness"][1];
  avgLight = plantDoc["avgLight"];
  for (int i = 0; i < NUM_SOLAR_PANELS; i++) {
    avgLightQuadrants[i] = plantDoc["avgLightQuadrants"][i];
  }
  avgWater = plantDoc["avgWater"];
  avgHumidity = plantDoc["avgHumidity"];
  avgTemp = plantDoc["avgTemp"];
  plantPulled = 1;
  plantDoc.clear();
}

// Take data from a plant object and push it into the plant file
void Plant::pushPlant() {
  JsonDocument plantDoc;
  plantDoc["id"] = id;
  plantDoc["commonName"] = commonName;
  plantDoc["scientificName"] = scientificName;
  JsonArray jsonLightReq = plantDoc["lightReq"].to<JsonArray>();
  jsonLightReq.add(lightReq[0]);
  jsonLightReq.add(lightReq[1]);
  JsonArray jsonWaterReq = plantDoc["waterReq"].to<JsonArray>();
  jsonWaterReq.add(waterReq[0]);
  jsonWaterReq.add(waterReq[1]);
  JsonArray jsonHardiness = plantDoc["hardiness"].to<JsonArray>();
  jsonHardiness.add(hardiness[0]);
  jsonHardiness.add(hardiness[1]);
  plantDoc["avgLight"] = avgLight;
  JsonArray jsonLightQuadrants = plantDoc["avgLightQuadrants"].to<JsonArray>();
  for (int i = 0; i < NUM_SOLAR_PANELS; i++) {
    jsonLightQuadrants.add(avgLightQuadrants[i]);
  }
  plantDoc["avgWater"] = avgWater;
  plantDoc["avgHumidity"] = avgHumidity;
  plantDoc["avgTemp"] = avgTemp;
  int pushJsonError = pushJsonDoc(plantDoc, PLANT_PATH);
  if (pushJsonError) {
    error.addError(pushJsonError);
  }
  plantDoc.clear();
}

/*---------------------------------------------------------- Sensor Reading Class ----------------------------------------------------------*/

// Initialization
SensorReading::SensorReading(Error& errorRef)
  : lightReadings{}, error(errorRef) {
  tempReading = 0;
  waterReading = 0;
  humidityReading = 0;
  lightReading = 0;
}


// Populate temperature data
void SensorReading::addTemp(float reading, Plant& plant) {
  tempReading = (reading * 1.8) + 32;
  parseNewData(tempReading, plant.avgTemp, TEMP_PATH, plant);
}

// Populate water data
void SensorReading::addWater(int reading, Plant& plant){
  waterReading = reading;
  parseNewData(waterReading, plant.avgWater, WATER_PATH, plant);
}

// Populate humidity data
void SensorReading::addHumidity(float reading, Plant& plant){
  humidityReading = reading;
  parseNewData(humidityReading, plant.avgHumidity, HUMIDITY_PATH, plant);
}

// Populate light data
void SensorReading::addLight(int reading1, int reading2, int reading3, int reading4, Plant& plant){
  //lightReading = (0.6 * reading) / (LTR390_GAIN * INTEGRATION_TIME);  // Lux = 0.6*ALS_DATA/(Gain*integration time(ms))
  lightReadings[0] = reading1;
  lightReadings[1] = reading2;
  lightReadings[2] = reading3;
  lightReadings[3] = reading4;
  parseNewData(lightReadings[0], plant.avgLightQuadrants[0], LIGHT_QUADRANT1_PATH, plant);
  parseNewData(lightReadings[1], plant.avgLightQuadrants[1], LIGHT_QUADRANT2_PATH, plant);
  parseNewData(lightReadings[2], plant.avgLightQuadrants[2], LIGHT_QUADRANT3_PATH, plant);
  parseNewData(lightReadings[3], plant.avgLightQuadrants[3], LIGHT_QUADRANT4_PATH, plant);
  plant.calcAvgLight();
}

// Pull data associated with a sensor reading type and add a new entry
// Update average reading in the process
template <typename readingType>
void SensorReading::parseNewData(readingType newReading, float& avgReading, char fileName[MAX_CHARS_FILENAME], Plant& plant) {
  if(!SD.exists(fileName)) {
    newFile(fileName);
  }
  JsonDocument sensorDoc = readSDFile(fileName);
  if (sensorDoc.isNull()) {
    error.addError(fileOperation);
    return;
  }
  addSensorReading(sensorDoc, newReading);
  avgReading = plant.getAvgReading(sensorDoc);
  int pushJsonError = pushJsonDoc(sensorDoc, fileName);
  if (pushJsonError) {
    error.addError(pushJsonError);
  }
  sensorDoc.clear();
}

// Add new sensor data to the JsonDocument.
// Each sensor reading array is treated as a circular buffer
template <typename readingType>
void SensorReading::addSensorReading(JsonDocument& sensorDoc, readingType reading) {
  int startIndex = sensorDoc["startIndex"];
  sensorDoc["readings"][startIndex] = reading;
  startIndex = (startIndex + 1) % MAX_SENSOR_READINGS;
  sensorDoc["startIndex"] = startIndex;
  int numReadings = sensorDoc["numReadings"];
  if (numReadings < MAX_SENSOR_READINGS) {
    sensorDoc["numReadings"] = numReadings + 1;
  }
}

// Remove data from a sensor readings file
void SensorReading::clearFile(char fileName[MAX_CHARS_FILENAME]) {
  if(!SD.exists(fileName)) {
    newFile(fileName); // Create file if it doesn't exist
  }
  JsonDocument emptyDoc;
  emptyDoc["startIndex"] = 0;
  emptyDoc["numReadings"] = 0;
  JsonArray readings = emptyDoc["readings"].to<JsonArray>();
  int pushJsonError = pushJsonDoc(emptyDoc, fileName);
  emptyDoc.clear();
  if (pushJsonError) {
    error.addError(pushJsonError);
    return;
  }
}

void SensorReading::newFile(char fileName[MAX_CHARS_FILENAME]) {
  File newFile = SD.open(fileName, FILE_WRITE);
  if (!newFile) {
    error.addError(fileOperation);
    return;
  }
  newFile.close();
  JsonDocument newDoc;
  newDoc["startIndex"] = 0;
  newDoc["numReadings"] = 0;
  JsonArray readings = newDoc["readings"].to<JsonArray>();
  int pushError = pushJsonDoc(newDoc, fileName);
  newDoc.clear();
  if (pushError) {
    error.addError(fileOperation);
  }
}

/*----------------------------------------------------------- Container Class --------------------------------------------------------------*/

// Initialization
Container::Container()
  : activePlant(error), error(), header(error), sensorReading(error), interface(error, activePlant) {
  activeMode = startupMode;
}

// Clear out data associated with the existing user plant (apart from average readings)
// Create a new user plant from selected DB plant data
// TODO: Call on plant selection from webapp
void Container::newUserPlant() {
  sensorReading.clearFile(TEMP_PATH); // Clear all existing data
  sensorReading.clearFile(WATER_PATH);
  sensorReading.clearFile(HUMIDITY_PATH);
  sensorReading.clearFile(LIGHT_QUADRANT1_PATH);
  sensorReading.clearFile(LIGHT_QUADRANT2_PATH);
  sensorReading.clearFile(LIGHT_QUADRANT3_PATH);
  sensorReading.clearFile(LIGHT_QUADRANT4_PATH);
  JsonDocument plantDoc;
  int pullError = pullPlant(PLANT_DB_PATH, interface.displayPlantIDs[interface.displayMap[interface.displayMap[3]]], plantDoc);
  if (pullError) {
    error.addError(pullError);
    return;
  }
  activePlant.id = plantDoc["id"];
  const char* commonName = plantDoc["name"];
  snprintf(activePlant.commonName, NUM_CHARS_NAME, "%s", commonName);
  const char* scientificName = plantDoc["scientific_name"];
  snprintf(activePlant.scientificName, NUM_CHARS_NAME, "%s", scientificName);
  //TODO: This order isn't set. write some code to find the ordering based on key 0,1,2 pair names first
  const char* keyPairs[3] = {plantDoc["data"][0]["key"], plantDoc["data"][1]["key"], plantDoc["data"][2]["key"]};
  int hardinessInd = 0;
  int lightInd = 0;
  int waterInd = 0;
  for (int i = 0; i < 3; i++) {
    if (strcmp(keyPairs[i], "USDA Hardiness zone") == 0) {
      hardinessInd = i;
    } else if (strcmp(keyPairs[i], "Light requirement") == 0) {
      lightInd = i;
    } else if (strcmp(keyPairs[i], "Water requirement") == 0) {
      waterInd = i;
    }
  }
  JsonArray jsonHardinessVals = plantDoc["data"][hardinessInd]["value"];
  JsonArray jsonLightReqs = plantDoc["data"][lightInd]["value"];
  JsonArray jsonWaterReqs = plantDoc["data"][waterInd]["value"];
  activePlant.hardiness[0] = jsonHardinessVals[0];  // Only need first and last elements of each
  activePlant.hardiness[1] = (jsonHardinessVals.size() > 1) ? jsonHardinessVals[jsonHardinessVals.size() - 1] : 0;
  activePlant.lightReq[0] = jsonLightReqs[0];
  activePlant.lightReq[1] = (jsonLightReqs.size() > 1) ? jsonLightReqs[jsonLightReqs.size() - 1] : 0;
  activePlant.waterReq[0] = jsonWaterReqs[0];
  activePlant.waterReq[1] = (jsonWaterReqs.size() > 1) ? jsonWaterReqs[jsonWaterReqs.size() - 1] : 0;
  plantDoc.clear();
  header.plantSelected = 1;
}

/*-------------------------------------------------------------- Header Class --------------------------------------------------------------*/

// Initialization
Header::Header(Error& errorRef) : error(errorRef) {
  numDBPlants = 0;
  lightThreshold = 0;
  tempThreshold = 0;
  waterThreshold = 0;
  humidityThreshold = 0;
  plantSelected = 0;
  headerPulled = 0;
}

// Pull in the header data from the SD and parse it into a header object
void Header::pullHeader() {
  JsonDocument headerDoc;
  char fileName[12] = HEADER_PATH;
  headerDoc = readSDFile(fileName);
  if (headerDoc.isNull()) {
    error.addError(fileOperation);
    return;
  }
  numDBPlants = headerDoc["numDBPlants"];
  plantSelected = headerDoc["plantSelected"];
  lightThreshold = headerDoc["lightThreshold"];
  tempThreshold = headerDoc["tempThreshold"];
  waterThreshold = headerDoc["waterThreshold"];
  humidityThreshold = headerDoc["humidityThreshold"];
  headerDoc.clear();
  headerPulled = 1;
}

// Take data from the header object and push it back into the header file
void Header::pushHeader() {
  JsonDocument headerDoc;
  headerDoc["numDBPlants"] = numDBPlants;
  headerDoc["plantSelected"] = plantSelected;
  headerDoc["lightThreshold"] = lightThreshold;
  headerDoc["tempThreshold"] = tempThreshold;
  headerDoc["waterThreshold"] = waterThreshold;
  headerDoc["humidityThreshold"] = humidityThreshold;
  char fileName[MAX_CHARS_FILENAME] = HEADER_PATH;
  int pushJsonError = pushJsonDoc(headerDoc, fileName);
  if (pushJsonError) {
    error.addError(jsonError);
  }
  headerDoc.clear();
}

/*----------------------------------------------------------------- Interface Class ----------------------------------------------------------------*/

// Initialization
Interface::Interface(Error& errorRef, Plant& plantRef)
  : activeMenu{}, selectedPlantIndex{}, numSelectCandidates{}, displayPlantIDs{}, displayPlantNames{}, indexer{},
    displayIndices{0, 1, 2}, displayMap{0, 1, 2, 0}, query{'A','A','A','A','A', '\0'}, error(errorRef), activePlant(plantRef) {
    numMenus = 4; // Starts at 4, if data cached then 5
    screenFocus = false;
  }

// Initialize the display
bool Interface::begin(uint8_t vcs, uint8_t addr) {
  bool began = display.begin(vcs, addr);
  if (began) {
    display.clearDisplay();
    display.display();
  }
  return began;
}

// Build and display the main menu
//
// TODO: Need to truncate names to not spill over
// Try truncation, scrolling?
void Interface::displayMainMenu() {
  int startX = 5;
  int padX = 15;
  int padY = 1;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char formattedName[NUM_CHARS_NAME] = {};
  truncateText(activePlant.commonName, NUM_CHARS_NAME, 1, SCREEN_WIDTH, formattedName);
  display.setCursor(horizontalCenterText(formattedName, NUM_CHARS_NAME, 1), 0);
  display.println(formattedName);
  int activeColor = SSD1306_WHITE;
  // Water
  if (screenFocus && indexer == 0) {
    display.fillRect(0, Y_SCALE + padY - 2, SCREEN_WIDTH - REC_WIDTH - 2, 15, SSD1306_WHITE);
  } 
  display.drawBitmap(startX + 2, Y_SCALE + padY - 1, waterBmp, 7, 13, SSD1306_INVERSE);
  display.setTextColor(SSD1306_INVERSE);
  display.setCursor(startX + padX, Y_SCALE + padY + 3 - 1);
  int waterInv = (WATER_MAX - activePlant.avgWater); // Invert raw water readings for display
  int displayWater = int((float(waterInv) / float(WATER_MAX))*100.0);
  display.printf("%d%%", displayWater);
  int waterThresh[2] = {-1, -1};
  activePlant.waterCheck(waterThresh);
  displayRecommendation(0, WATER_MIN, WATER_MAX, waterThresh, waterInv);
  // Light
  if (screenFocus && indexer == 1) {
    display.fillRect(0, Y_SCALE + 13 + padY*2 - 1, SCREEN_WIDTH - REC_WIDTH - 2, 15, SSD1306_WHITE);
  }
  display.drawBitmap(startX, Y_SCALE + 13 + padY*2, lightBmp, 11, 11, SSD1306_INVERSE);
  display.setCursor(startX + padX, Y_SCALE + 13 + padY*2 + 2);
  display.printf("%.0f lx", activePlant.avgLight);
  int lightThresh[2] = {-1, -1};
  activePlant.lightCheck(lightThresh);
  displayRecommendation(1, LIGHT_MIN, LIGHT_MAX, lightThresh, activePlant.avgLight);
  // Temp
  if (screenFocus && indexer == 2) {
    display.fillRect(0, Y_SCALE + 13 + 11 + padY*3 - 1, SCREEN_WIDTH - REC_WIDTH - 2, 15, SSD1306_WHITE);
  }
  display.drawBitmap(startX + 1, Y_SCALE + 13 + 11 + padY*3, tempBmp, 8, 13, SSD1306_INVERSE);
  display.setCursor(startX + padX, Y_SCALE + 13 + 11 + padY*3 + 3);
  char tempReport[5] = {};
  int tempCharCt = snprintf(tempReport, 5, "%.0f", activePlant.avgTemp);
  display.print(tempReport);
  display.drawBitmap(startX + padX + tempCharCt*X_SCALE + 1, Y_SCALE + 13 + 11 + padY*3 + 3, degFBmp, 8, 8, SSD1306_INVERSE);
  int tempThresh[2] = {-1, -1};
  activePlant.tempCheck(tempThresh);
  displayRecommendation(2, TEMP_MIN, TEMP_MAX, tempThresh, activePlant.avgTemp);
  // Humidity
  if (screenFocus && indexer == 3) {
    display.fillRect(0, Y_SCALE + 13 + 11 + 13 + padY*4 + 3 + 1 - 1, SCREEN_WIDTH - REC_WIDTH - 2, 15, SSD1306_WHITE);
  }
  display.setCursor(startX + 1, Y_SCALE + 13 + 11 + 13 + padY*4 + 3 + 1);
  display.print("RH");
  display.setCursor(startX + padX + 1, Y_SCALE + 13 + 11 + 13 + padY*4 + 3 + 1);
  display.printf("%.0f%%", activePlant.avgHumidity);
  int humidityThresh[2] = {-1, -1};
  activePlant.humidityCheck(humidityThresh);
  displayRecommendation(3, HUMIDITY_MIN, HUMIDITY_MAX, humidityThresh, activePlant.avgHumidity);
  display.display();
  activeMenu = mainMenu;
}

// Display additional data about one of the four measured characteristics
void Interface::displayDataMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int thresh[2] = {-1, -1};
  char title[15] = {};
  char units[20] = {};
  char avg[50] = {};
  char min[50] = {};
  char max[50] = {};
  switch (indexer) {
    case 0:
      snprintf(title, 15, "Water");
      snprintf(units, 20, "%% Saturation");
      snprintf(avg, 50, "Avg: %.0f%%", (((float)WATER_MAX - activePlant.avgWater)/(float)WATER_MAX)*100.0);
      activePlant.waterCheck(thresh);
      snprintf(min, 50, "Min: %.0f%%", (((float)thresh[0])/(float)WATER_MAX)*100.0);
      snprintf(max, 50, "Max: %.0f%%", (((float)thresh[1])/(float)WATER_MAX)*100.0);
      break;
    case 1:
      {
      snprintf(title, 15, "Light");
      snprintf(units, 20, "Lux");
      snprintf(avg, 50, "Avg: %.0f Lux", activePlant.avgLight);
      activePlant.lightCheck(thresh);
      snprintf(min, 50, "Min: %d Lux", thresh[0]);
      snprintf(max, 50, "Max: %d Lux", thresh[1]);
      int textX = horizontalCenterText("Highest Dir.", 50, 1);
      int maxSolarIndex = 0;
      for (int i = 1; i < NUM_SOLAR_PANELS; i++) {
        if (activePlant.avgLightQuadrants[i] > activePlant.avgLightQuadrants[maxSolarIndex]) {
          maxSolarIndex = i;
        }
      }
      const unsigned char* dirBmp;
      switch (maxSolarIndex) { // TODO - ordering here
        case 0:
          dirBmp = lightArrowUpBmp;
          break;
        case 1:
          dirBmp = lightArrowLeftBmp;
          break;
        case 2:
          dirBmp = lightArrowDownBmp;
          break;
        case 3:
          dirBmp = lightArrowRightBmp;
      }
      display.drawBitmap(textX - 7 - 5, SCREEN_HEIGHT - 7 - 2, dirBmp, 7, 7, 1);
      display.setCursor(textX, SCREEN_HEIGHT - 7 -2);
      display.print("Highest Dir.");
      display.drawBitmap((int)(SCREEN_WIDTH/2) + ((int)(SCREEN_WIDTH/2) - textX) + 5, SCREEN_HEIGHT - 7 - 2, dirBmp, 7, 7, 1);
      break;
      }
    case 2:
      snprintf(title, 15, "Temperature");
      snprintf(units, 20, "Degrees F");
      snprintf(avg, 50, "Avg: %.0f", activePlant.avgTemp);
      activePlant.tempCheck(thresh);
      snprintf(min, 50, "Min: %d F", thresh[0]);
      snprintf(max, 50, "Max: %d F", thresh[1]);
      break;
    case 3:
      snprintf(title, 15, "Humidity");
      snprintf(units, 20, "%% Rel. Humidity");
      snprintf(avg, 50, "Avg: %.0f", activePlant.avgHumidity);
      activePlant.humidityCheck(thresh);
      snprintf(min, 50, "Min: %d%%", thresh[0]);
      snprintf(max, 50, "Max: %d%%", thresh[1]);
  }
  display.setCursor(horizontalCenterText(title, 15, 1), 0);
  display.println(title);
  display.setCursor(horizontalCenterText(units, 20, 1), Y_SCALE + 1);
  display.print(units);
  display.setCursor(horizontalCenterText(avg, 50, 1), Y_SCALE*2 + 2);
  display.print(avg);
  display.setCursor(horizontalCenterText(min, 50, 1), Y_SCALE*3 + 3);
  display.print(min);
  display.setCursor(horizontalCenterText(max, 50, 1), Y_SCALE*4 + 4);
  display.print(max);
  display.display();
  activeMenu = dataMenu;
}

// Build and display the info menu
//
// TODO: Make this display requirements (in readable form) or smth
//
void Interface::displayInfoMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char truncatedName[NUM_CHARS_NAME] = {};
  truncateText(activePlant.commonName, NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, truncatedName);
  display.setCursor(horizontalCenterText(truncatedName, NUM_CHARS_NAME, 1), 0);
  display.print(truncatedName);
  char splitSciName[NUM_CHARS_NAME] = {};
  int nLinesSciName = splitLines(activePlant.scientificName, NUM_CHARS_NAME, 1, SCREEN_WIDTH, splitSciName);
  if (nLinesSciName == 1) {
    display.setCursor(horizontalCenterText(activePlant.scientificName, NUM_CHARS_NAME, 1), Y_SCALE);
  } else {
    display.setCursor(0, Y_SCALE);
  }
  display.print(splitSciName);
  char waterReq1[NUM_CHARS_REQ] = {};
  if (parseWaterReq(activePlant.waterReq[0], NUM_CHARS_REQ, waterReq1)) {
    display.setCursor(0, Y_SCALE*3);
    display.print("Water:");
    display.setCursor(X_SCALE*7, Y_SCALE*3);
    display.print(waterReq1);
  }
  char waterReq2[NUM_CHARS_REQ] = {};
  if (parseWaterReq(activePlant.waterReq[1], NUM_CHARS_REQ, waterReq2)) {
    display.setCursor(0, Y_SCALE*3);
    display.print("to");
    display.setCursor(X_SCALE*3, Y_SCALE*4);
    display.print(waterReq2);
  }
  char lightReq1[NUM_CHARS_REQ] = {};
  if (parseLightReq(activePlant.lightReq[0], NUM_CHARS_REQ, lightReq1)) {
    display.setCursor(0, Y_SCALE*5);
    display.print("Light:");
    display.setCursor(X_SCALE*8, Y_SCALE*5);
    display.print(lightReq1);
  }
  char lightReq2[NUM_CHARS_REQ] = {};
  if (parseLightReq(activePlant.lightReq[1], NUM_CHARS_REQ, lightReq2)) {
    display.setCursor(0, Y_SCALE*6);
    display.print("to");
    display.setCursor(X_SCALE*3, Y_SCALE*6);
    display.print(lightReq2);
  }
  char hardinessStr[22] = {};
  if (activePlant.hardiness[0] != 0 && activePlant.hardiness[1] != 0) {
    display.setCursor(0, Y_SCALE*7);
    snprintf(hardinessStr, 22, "Hardiness: %d - %d", activePlant.hardiness[0], activePlant.hardiness[1]);
    display.print(hardinessStr);
  } else if (activePlant.hardiness[0] != 0 && activePlant.hardiness[1] == 0) {
    display.setCursor(0, Y_SCALE*7);
    snprintf(hardinessStr, 22, "Hardiness: %d", activePlant.hardiness[0]);
    display.print(hardinessStr);
  } else if (activePlant.hardiness[0] == 0 && activePlant.hardiness[1] != 0) {
    display.setCursor(0, Y_SCALE*7);
    snprintf(hardinessStr, 22, "Hardiness: %d", activePlant.hardiness[1]);
    display.print(hardinessStr);
  }
  display.display();
  activeMenu = infoMenu;
}

// Build and display query input menu
void Interface::displayInputMenu() {
  int dx = int(SCREEN_WIDTH / (NUM_CHARS_QUERY + 1));
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(horizontalCenterText("Select Name", 12, 1), 3);
  display.print("Select Name");
  display.setTextSize(2);
  int startY = int((SCREEN_HEIGHT/2) - ((Y_SCALE*2)/2)) + 6;
  int startX = dx - int((2*X_SCALE)/2);
  for (int i = 0; i < 5; i++) {
    display.setCursor(startX + (dx*i), startY);
    display.print(query[i]);
  }
  if(screenFocus) {
    display.drawBitmap(startX + (dx*indexer) - 3, startY - 8 - 6, upArrowBmp, 16, 8, 1);
    display.drawBitmap(startX + (dx*indexer) - 3, startY + Y_SCALE*2 + 6, downArrowBmp, 16, 8, 1);
  }
  display.display();
  activeMenu = inputMenu;
}

// Build and display menu for selecting candidates from a search
// 
// TODO: is ugly. make pretty. add scrollbar?
//
void Interface::displaySelectMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  if (numSelectCandidates < 1) {
    display.setTextSize(2);
    display.setCursor(horizontalCenterText("NO RESULTS", 11, 2), 30);
    display.setTextColor(SSD1306_WHITE);
    display.println("NO RESULTS");
    display.display();
    activeMenu = selectMenu;
    return;
  }
  int y1 = 0;
  int y2 = Y_SCALE * 3;
  int y3 = Y_SCALE * 6;
  char splitName1[NUM_CHARS_NAME] = {};
  char splitName2[NUM_CHARS_NAME] = {};
  char splitName3[NUM_CHARS_NAME] = {};
  int nLines1 = 1;
  int nLines2 = 1;
  int nLines3 = 1;
  display.fillRect(0, 22, SCREEN_WIDTH, 20, SSD1306_WHITE);
  display.setTextColor(SSD1306_INVERSE);
  switch (displayMap[3]) {
    case 0:
      if (numSelectCandidates > 0) {
        nLines1 = splitLines(displayPlantNames[displayMap[0]], NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, splitName1);
        y2 = (nLines1 == 1) ? y2 + int(Y_SCALE/2) : y2;
        display.setCursor(0, y2);
        display.print(splitName1);
      }
      if (numSelectCandidates > 1) {
        nLines2 = splitLines(displayPlantNames[displayMap[1]], NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, splitName2);
        y3 = (nLines2 == 1) ? y3 + int(Y_SCALE/2) : y3;
        display.setCursor(0, y3);
        display.print(splitName2);
      }
      break;
    case 1:
      if (numSelectCandidates > 0) {
        nLines1 = splitLines(displayPlantNames[displayMap[0]], NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, splitName1);
        y1 = (nLines1 == 1) ? y1 + int(Y_SCALE/2) : y1;
        display.setCursor(0, y1);
        display.print(splitName1);
      }
      if (numSelectCandidates > 1) {
        nLines2 = splitLines(displayPlantNames[displayMap[1]], NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, splitName2);
        y2 = (nLines2 == 1) ? y2 + int(Y_SCALE/2) : y2;
        display.setCursor(0, y2);
        display.print(splitName2);
      }
      if (numSelectCandidates > 2) {
        nLines3 = splitLines(displayPlantNames[displayMap[2]], NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, splitName3);
        y3 = (nLines3 == 1) ? y3 + int(Y_SCALE/2) : y3;
        display.setCursor(0, y3);
        display.print(splitName3);
      }
      break;
    case 2:
      nLines2 = splitLines(displayPlantNames[displayMap[1]], NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, splitName2);
      y1 = (nLines2 == 1) ? y1 + int(Y_SCALE/2) : y1;
      display.setCursor(0, y1);
      display.print(splitName2);
      nLines3 = splitLines(displayPlantNames[displayMap[2]], NUM_CHARS_NAME, 1, SCREEN_WIDTH - 10, splitName3);
      y2 = (nLines3 == 1) ? y2 + int(Y_SCALE/2) : y2;
      display.setCursor(0, y2);
      display.print(splitName3);
      break;
  }
  display.display();
  activeMenu = selectMenu;
  numMenus = 5;
}

// Report active error & potential solutions
void Interface::displayErrorScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int startY = Y_SCALE*2;
  char line1[40] = " ";
  char line2[40] = " ";
  char line3[40] = " ";
  char line4[40] = " ";
  switch (error.highestPriority) {
    case lightSensorInit:
      //
      // TODO: can this even happen anymore?
      //
      break;
    case tempSensorInit:
      snprintf(line1, 40, "Temp. sensor Error:");
      snprintf(line2, 40, "Attempt power reset");
      snprintf(line3, 40, "Check DHT20 wiring");
      break;
    case moistureSensorInit:
      //
      // TODO: can this even happen?
      //
      break;
    case jsonError:
      snprintf(line1, 40, "File parsing Error:");
      snprintf(line2, 40, "Attempt power reset");
      snprintf(line3, 40, "Re-seat micro SD");
      snprintf(line4, 40, "Check file system");
      break;
    case fileOperation:
      snprintf(line1, 40, "File access error:");
      snprintf(line2, 40, "Attempt power reset");
      snprintf(line3, 40, "Re-seat micro SD");
      snprintf(line4, 40, "Reload file system");
      break;
    case SDInit:
      snprintf(line1, 40, "Micro SD init failed:");
      snprintf(line2, 40, "Re-seat micro SD");
      snprintf(line3, 40, "Attempt power reset");
      snprintf(line4, 40, "Test other micro SD");
      break;
    default:
      snprintf(line1, 40, "Unknown Error:");
      snprintf(line2, 40, "Attempt power reset");
  }
  display.setCursor(horizontalCenterText(line1, 40, 1), startY);
  display.print(line1);
  display.setCursor(horizontalCenterText(line2, 40, 1), startY + Y_SCALE);
  display.print(line2);
  display.setCursor(horizontalCenterText(line3, 40, 1), startY + 2*Y_SCALE);
  display.print(line3);
  display.setCursor(horizontalCenterText(line4, 40, 1), startY + 3*Y_SCALE);
  display.print(line4);
  display.display();
}

// Temporary loading screen for collecting plant data
//
// TODO: It could have more character to it. Maybe something like (plant image) -> (file image)
void Interface::displayLoadingScreen() {
  display.clearDisplay();
  display.fillCircle(64, 13, 2, 1);
  display.fillCircle(53, 19, 2, 1);
  display.fillCircle(75, 19, 2, 1);
  display.fillCircle(75, 30, 2, 1);
  display.fillCircle(64, 36, 2, 1);
  display.fillCircle(53, 30, 2, 1);
  display.setTextColor(1);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(horizontalCenterText("Loading...", 20, 1), 48);
  display.print("Loading...");
  display.display();

}

// Add a recommendation to the main menu
void Interface::displayRecommendation(int recInd, int min, int max, int threshold[2], float val) {
  int startY = Y_SCALE + 1 + recInd*(REC_HEIGHT + 3);
  int startX = SCREEN_WIDTH - REC_WIDTH - 1;
  if (threshold[0] < 0 || threshold[1] < 0) {
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(startX, startY + int((REC_HEIGHT - Y_SCALE)/2));
    display.print("No Data");
    return;
  }
  int endX = SCREEN_WIDTH - 1;
  float res = float(REC_WIDTH)/float(max - min);
  // Outer bounds
  display.drawLine(startX, startY, startX, startY + REC_HEIGHT, SSD1306_WHITE);
  display.drawLine(endX, startY, endX, startY + REC_HEIGHT, SSD1306_WHITE);
  // Thresholds
  int threshMinX = int(res*threshold[0]) + startX;
  int threshMaxX = int(res*threshold[1]) + startX;
  display.drawLine(threshMinX, startY + (REC_HEIGHT - THRESH_MARKER_HEIGHT), threshMinX, startY + REC_HEIGHT, SSD1306_WHITE);
  display.drawLine(threshMaxX, startY + (REC_HEIGHT - THRESH_MARKER_HEIGHT), threshMaxX, startY + REC_HEIGHT, SSD1306_WHITE);
  // Marker
  int markerX = int(res*val) + startX;
  int markerY = startY + (REC_HEIGHT - THRESH_MARKER_HEIGHT - 8 - 1);
  if (markerX < startX + 2) { // Clip position
    markerX = startX + 2;
  } else if (markerX > endX - 6) { 
    markerX = endX - 6;
  }
  display.drawBitmap(markerX, markerY, plantMarkerBmp, 5, 8, 1);
  // Arrow (if outside of bounds)
  int arrowY = markerY + 1;
  if (val < threshold[0] && (markerX - startX) > int(REC_WIDTH/2)) { // Increase, left side
    display.drawBitmap(markerX - 7, arrowY, arrowRightBmp, 5, 5, 1);
  } else if (val < threshold[0] && (markerX - startX) <= int(REC_WIDTH/2)) { // Increase, right side
    display.drawBitmap(markerX + 7, arrowY, arrowRightBmp, 5, 5, 1);
  } else if (val > threshold[1] && (markerX - startX) > int(REC_WIDTH/2)) { // Decrease, left side
    display.drawBitmap(markerX - 7, arrowY, arrowLeftBmp, 5, 5, 1);
  } else if (val > threshold[1] && (markerX - startX) <= int(REC_WIDTH/2)) { // Decrease, right side
    display.drawBitmap(markerX + 7, arrowY, arrowLeftBmp, 5, 5, 1);
  }
}

// Increment/Decrement the current query position alphabetically
void Interface::indexQueryPos(bool upDir) {
  uint8_t pos = indexer;
  if (!upDir) { // Down
    if (query[pos] >= 65 && query[pos] < 90) { // A-Y
      query[pos]++;
    } else if (query[pos] == 90) { // 'Z'
      query[pos] = '_';
    } else if (query[pos] == 95) { // '_'
      query[pos] = '\0';
    } else { // Default to 'A'
      query[pos] = 'A';
    } 
  } else {
    if (query[pos] > 65 && query[pos] <= 90) { // A-Y
      query[pos]--;
    } else if (query[pos] == 65) { // 'A'
      query[pos] = '\0';
    } else if (query[pos] == 95) { // '_'
      query[pos] = 'Z';
    } else if (query[pos] == 0) { // ''
      query[pos] = '_';
    } else { // Default to 'A'
      query[pos] = 'A';
    }
  }
}

// Scroll the selection interface down by "rolling" upwards
// Mappings are cycled, then the bottom position is replaced
void Interface::scrollSelectDown() {
  uint8_t top = 0; // for readability
  uint8_t mid = 1;
  uint8_t bot = 2;
  uint8_t act = 3;
  if (numSelectCandidates == 0) { // Nothing to scroll
    return;
  }
  if (numSelectCandidates < NUM_CANDIDATES_SHOWN) { // Not enough candidates to scroll
    displayMap[act] = (displayMap[act] + 1) % numSelectCandidates;
    return;
  }
  if (displayMap[act] == top) { // At top
    displayMap[act] = mid; // Move active pos. to middle
  } else if (displayMap[act] == mid && displayIndices[displayMap[bot]] < (numSelectCandidates - 1)) { // Inside list (middle active)
    int newBottomIndex = displayIndices[displayMap[bot]] + 1;
    for (int i = 0; i < NUM_CANDIDATES_SHOWN; i++) {
      displayMap[i] = (displayMap[i] + 1) % NUM_CANDIDATES_SHOWN; // Cycle mappings "upward"
    }
    displayIndices[displayMap[bot]] = newBottomIndex;
    pullCachedData(displayIndices[displayMap[bot]], displayPlantNames[displayMap[bot]], displayPlantIDs[displayMap[bot]]);
  } else if (displayMap[act] == 1 && displayIndices[displayMap[bot]] >= (numSelectCandidates - 1)) { // Bottom of list reached
    displayMap[act] = bot; // Move active pos. to bottom
  } else if (displayMap[act] == bot) { // At bottom
    displayIndices[0] = 0; // Reset indices
    displayIndices[1] = 1;
    displayIndices[2] = 2;
    displayMap[top] = top; // Reset mappings
    displayMap[mid] = mid;
    displayMap[bot] = bot;
    displayMap[act] = top; // Move active pos. to top
    pullCachedData(displayIndices[displayMap[top]], displayPlantNames[displayMap[top]], displayPlantIDs[displayMap[top]]); // Pull data
    pullCachedData(displayIndices[displayMap[mid]], displayPlantNames[displayMap[mid]], displayPlantIDs[displayMap[mid]]);
    pullCachedData(displayIndices[displayMap[bot]], displayPlantNames[displayMap[bot]], displayPlantIDs[displayMap[bot]]);
  }
}

// Scroll the selection interface down by "rolling" downwards
// Mappings are cycled, then the top position is replaced
void Interface::scrollSelectUp() {
  uint8_t top = 0; // for readability
  uint8_t mid = 1;
  uint8_t bot = 2;
  uint8_t act = 3;
  if (numSelectCandidates == 0) { // Nothing to scroll
    return;
  }
  if (numSelectCandidates < act) { // Not enough candidates to scroll
    displayMap[act] = (displayMap[act] + 1) % numSelectCandidates;
    return;
  }
  if (displayMap[act] == 0) { // At top
    displayIndices[top] = numSelectCandidates - 3; // Max out indices
    displayIndices[mid] = numSelectCandidates - 2;
    displayIndices[bot] = numSelectCandidates - 1;
    displayMap[top] = top; // Reset mappings
    displayMap[mid] = mid;
    displayMap[bot] = bot;
    displayMap[act] = bot; // Move active pos. to bottom
    pullCachedData(displayIndices[displayMap[top]], displayPlantNames[displayMap[top]], displayPlantIDs[displayMap[top]]); // Pull data
    pullCachedData(displayIndices[displayMap[mid]], displayPlantNames[displayMap[mid]], displayPlantIDs[displayMap[mid]]);
    pullCachedData(displayIndices[displayMap[bot]], displayPlantNames[displayMap[bot]], displayPlantIDs[displayMap[bot]]);
  } else if (displayMap[act] == mid && displayIndices[displayMap[top]] > 0) { // Inside list (middle active)
    int newTopIndex = displayIndices[displayMap[top]] - 1;
    for (int i = 0; i < NUM_CANDIDATES_SHOWN; i++) {
      displayMap[i] = displayMap[i] > top ? (displayMap[i] - 1) : bot; // Cycle mappings "downward"
    }
    displayIndices[displayMap[top]] = newTopIndex;
    pullCachedData(displayIndices[displayMap[top]], displayPlantNames[displayMap[top]], displayPlantIDs[displayMap[top]]);
  } else if (displayMap[act] == 1 && displayIndices[displayMap[top]] <= 0) { // Top of list reached
    displayMap[act] = top; // Move active pos. to top
  } else if (displayMap[act] == bot) { // At bottom
    displayMap[act] = mid; // Move active position to middle
  }
}

// Find plant names in the database similar to a query string
// Results are sorted by similarity to query
// Sorted results are cached in a temporary file which is deleted before sleeping
void Interface::queryDBPlants() {
  // Step 1 -> Find # of candidates
  numSelectCandidates = 0;
  File dbFile = SD.open(PLANT_DB_PATH, FILE_READ);
  if (!dbFile) {
    error.addError(fileOperation);
    return;
  }
  while (dbFile.available()) {
    bool found = dbFile.find("\"name\":");
    if (!found) {
      break;
    }
    if (dbFile.peek() == ' ') { // Skip spaces
      dbFile.seek(dbFile.position() + 1);
    }
    dbFile.seek(dbFile.position() + 1); // Skip beginning quote
    uint8_t diffCt = 0;
    char name[NUM_CHARS_NAME] = {};
    uint8_t nameCharCt = dbFile.readBytesUntil('\"', name, NUM_CHARS_NAME);
    if (nameCharCt > 0) {
      diffCt = compareToQuery(name, query);
      numSelectCandidates = diffCt < QUERY_DIFF_THRESH ? numSelectCandidates + 1 : numSelectCandidates;
    }
  }
  dbFile.close();
  if (numSelectCandidates < 1) { 
    return;
  }
  uint8_t candidateDiffs[numSelectCandidates] = {};
  int candidateIDs[numSelectCandidates] = {};

  // Step 2 -> Find each candidate name, ID, and difference count
  dbFile = SD.open(PLANT_DB_PATH, FILE_READ);
  File tmpGatherFile = SD.open(TMP_GATHER_PATH, FILE_WRITE);
  if (!dbFile || !tmpGatherFile) {
    dbFile.close();
    tmpGatherFile.close();
    error.addError(fileOperation);
    return;
  }
  int processCt = 0;
  while (dbFile.available() && processCt < numSelectCandidates){
    bool found = dbFile.find("id\":");
    if (!found) {
      break;
    }
    if (dbFile.peek() == ' ') { // Skip spaces
      dbFile.seek(dbFile.position() + 1);
    }
    char idChar[MAX_DIGITS_ID + 1] = {}; // Max 999,999
    uint8_t idDigitCt = dbFile.readBytesUntil(',', idChar, MAX_DIGITS_ID + 1);
    dbFile.find("\"name\":");
    if (dbFile.peek() == ' ') { // Skip spaces
      dbFile.seek(dbFile.position() + 1);
    }
    dbFile.seek(dbFile.position() + 1); // Skip beginning quote
    char name[NUM_CHARS_NAME] = {};
    uint8_t nameCharCt = dbFile.readBytesUntil('\"', name, NUM_CHARS_NAME);
    if (idDigitCt > 0 && nameCharCt > 0) {
      int id = strtol(idChar, NULL, 10);
      uint8_t diffCt = compareToQuery(name, query);
      if (diffCt < QUERY_DIFF_THRESH) {
        char newTmpLine[70] = {};
        snprintf(newTmpLine, 70, "<%d-%s>", id, name);
        tmpGatherFile.println(newTmpLine); // Copy over data to the collection file for caching
        candidateDiffs[processCt] = diffCt;
        candidateIDs[processCt] = id;
        processCt++;
      }
    }
  }
  dbFile.close();
  tmpGatherFile.close();

  // Step 3 -> Sort by difference count (lowest first)
  bool sorted = 0;
  if (numSelectCandidates < 2) {
    sorted = 1;
  }
  while (!sorted) {
    sorted = 1;
    for (int i = 1; i < numSelectCandidates; i++) {
      if (candidateDiffs[i] < candidateDiffs[i - 1]) {
        uint8_t swapDiff = candidateDiffs[i];
        int swapID = candidateIDs[i];
        candidateDiffs[i] = candidateDiffs[i - 1];
        candidateIDs[i] = candidateIDs[i - 1];
        candidateDiffs[i - 1] = swapDiff;
        candidateIDs[i - 1] = swapID;
        sorted = 0;
      }
    }
  }

  // Step 4 -> Re-order names based on ID order, cache results
  tmpGatherFile = SD.open(TMP_GATHER_PATH, FILE_READ);
  File tmpSortFile = SD.open(TMP_SORT_PATH, FILE_WRITE);
  if (!tmpGatherFile || !tmpSortFile) {
    tmpGatherFile.close();
    tmpSortFile.close();
    error.addError(fileOperation);
    return;
  }
  for (int i = 0; i < numSelectCandidates; i++) {
    tmpGatherFile.seek(0);
    char searchChar[MAX_DIGITS_ID + 3] = {};
    snprintf(searchChar, MAX_DIGITS_ID + 3, "<%d-", candidateIDs[i]);
    tmpGatherFile.find(searchChar);
    char name[NUM_CHARS_NAME] = {};
    tmpGatherFile.readBytesUntil('>', name, NUM_CHARS_NAME);
    char newLine[70] = {};
    snprintf(newLine, 70, "<%d-%d-%s>", i, candidateIDs[i], name);
    tmpSortFile.println(newLine); // Re-cache sorted list to reduce memory load
  }
  tmpGatherFile.close();
  tmpSortFile.close();
  SD.remove(TMP_GATHER_PATH);
  // Step 5 -> pull first three plants to display
  for (int i = 0; i < 3; i++) {
    if ((i + 1) > numSelectCandidates) {
      break;
    }
    pullCachedData(i, displayPlantNames[i], displayPlantIDs[i]);
  }
}

// Pull plant name & id from cache file
void Interface::pullCachedData(int index, char name[], int& id) {
  File file = SD.open(TMP_SORT_PATH, FILE_READ);
  if (!file) {
    error.addError(fileOperation);
    file.close();
    return;
  }
  char searchTerm[MAX_DIGITS_ID + 3] = {};
  snprintf(searchTerm, MAX_DIGITS_ID + 3, "<%d-", index);
  bool found = file.find(searchTerm);
  if (!found) {
    error.addError(fileOperation);
    file.close();
    return;
  }
  char idChar[MAX_DIGITS_ID + 1] = {};
  uint8_t idDigitCt = file.readBytesUntil('-', idChar, MAX_DIGITS_ID + 1);
  char tmpName[NUM_CHARS_NAME] = {};
  int nameCharCt = file.readBytesUntil('>', tmpName, NUM_CHARS_NAME);
  if(idDigitCt > 0 && nameCharCt > 0) {
    id = strtol(idChar, NULL, 10);
    memcpy(name, tmpName, NUM_CHARS_NAME);
  } else {
    error.addError(fileOperation);
    file.close();
    return;
  }
  file.close();
}

// Cycle through available screens
void Interface::nextScreen(bool plantSelected) {
  screenFocus = 0;
  if (activeMenu == dataMenu) {
    activeMenu = mainMenu;
  }
  activeMenu = (activeMenu + 1) % (numMenus);
  activeMenu = activeMenu == 0 ? 1 : activeMenu;
  switch (activeMenu) {
    case noMenu:
      displayMainMenu();
      break;
    case mainMenu:
      displayMainMenu();
      break;
    case infoMenu:
      if (plantSelected) { 
        displayInfoMenu();
      } else {
        displayInputMenu();
      }
      break;
    case inputMenu:
      displayInputMenu();
      break;
    case selectMenu:
      displaySelectMenu();
      break;
  }
}

// Set all pixels to 0 and send a display off command
void Interface::displayOff() {
  activeMenu = 0;
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
}

/*-------------------------------------------------------------- Standalone Functions --------------------------------------------------------------*/

// Returns a JsonDocument contianing the deserialized file contents
JsonDocument readSDFile(char fileName[]) {
  JsonDocument doc;
  File file = SD.open(fileName, FILE_READ);
  if (!file) {
    return doc;
  }
  DeserializationError jsonDeserializationError = deserializeJson(doc, file);
  file.close();
  return doc;
}

// Write the contents of a JsonDocument to a file
int pushJsonDoc(JsonDocument doc, char fileName[MAX_CHARS_FILENAME]) {
  int error = noError;
  if (!SD.exists(fileName)) {
    return fileOperation;
  }
  File file = SD.open(fileName, FILE_WRITE);
  if (!file) {
    return fileOperation;
  }
  serializeJson(doc, file);
  file.close();
  return error;
}

// Helper for querying
// Compare a candidate name to the query string
uint8_t compareToQuery(char candidate[], char query[]) {
  uint8_t diffCt = 0;
  for (int i = 0; i < NUM_CHARS_QUERY; i++) {
    char testChar = candidate[i];
    testChar = testChar > 96 && testChar < 123 ? testChar - 32 : testChar;
    diffCt = testChar != query[i] ? diffCt + 1 : diffCt;
  }
  return diffCt;
}

// Helper to grab a plant from the database
// Pulls data into the passed JsonDocument
// TODO: Should probably rename this function
int pullPlant(char fileName[], int id, JsonDocument& doc) {
  File file = SD.open(fileName, FILE_READ);
  char searchID1[20] = {};
  uint8_t search1CharCt = snprintf(searchID1, 20, "{\"id\": %d", id); // TODO - if possible, make space-tolerant
  bool found = file.find(searchID1);
  if (!found) {
    file.close();
    return fileOperation;
  }
  unsigned long marker1 = file.position() - search1CharCt;
  uint8_t interListTerminalCt = 8;
  uint8_t endListTerminalCt = 2;
  found = file.findUntil("{\"id\":", "}]}");
  unsigned long marker2 = file.position();
  marker2 = found ? marker2 - interListTerminalCt : marker2 - endListTerminalCt;
  const int len = marker2 - marker1 + 1;
  char raw[len] = {};
  file.seek(marker1);
  file.readBytes(raw, len - 1);
  file.close();
  DeserializationError error = deserializeJson(doc, raw);
  if (error) {
    return jsonError;
  }
  return noError;
}

// Standalone helper for centering text horizontally
int horizontalCenterText(char text[], int bufferLen, int fontSize) {
  int len = strlen(text);
  int startPt = int((SCREEN_WIDTH/2) - ((fontSize*X_SCALE*len)/2));
  startPt = startPt >= 0 ? startPt : 0;
  return startPt;
}

// Standalone text formatting tool
// textLen is maximum length of text buffer
void truncateText(char text[], int textLen, int textSize, int widthPx, char truncatedText[]) {
  memset(truncatedText, 0, textLen * sizeof(truncatedText[0]));
  int len = strlen(text);
  if (len*X_SCALE*textSize <= widthPx ) {
    snprintf(truncatedText, textLen, "%s", text);
    return;
  } else {
    int numChars = int(widthPx/(textSize*X_SCALE)) - 2;
    if (numChars <= 0 || numChars > textLen) {
      return;
    }
    snprintf(truncatedText, numChars, "%s", text);
    truncatedText[numChars - 1] = '.'; // TODO - Why this not working?
    truncatedText[numChars] = '.';
  }
}

// Standalone text formatting tool to split text into two lines if necessary
// Returns number of lines 
int splitLines(char text[], int textLen, int textSize, int widthPx, char splitText[]) {
  memset(splitText, 0, textLen * sizeof(splitText[0]));
  int len = 0;
  int spaceInd = 0;
  for (int i = 0; i < textLen; i++) {
    if (text[i] == ' ') {
      spaceInd = i;
    }
    if (text[i] == '\0') {
      len = i;
      break;
    }
    else {
      len = i + 1;
    }
  }
  if (len*X_SCALE*textSize <= widthPx) {
    snprintf(splitText, textLen, "%s", text);
    return 1;
  }
  if (spaceInd > 0) { // Has space, replace w/ newline to split
    snprintf(splitText, textLen, "%s", text);
    splitText[spaceInd] = '\n';
    return 2;
  }
  if (len < textLen - 1) { // Room in the buffer for a newline
    int numCharsLine = int(widthPx/(textSize*X_SCALE));
    for (int i = 0; i < numCharsLine; i++) {
      splitText[i] = text[i];
    }
    splitText[numCharsLine] = '\n';
    for (int i = numCharsLine + 1; i < len; i++) {
      splitText[i] = text[i - 1];
    }
    return 2;
  }
  snprintf(splitText, textLen, "%s", text);
  return 2; // Otherwise just let the dang display library handle it.
}

// Parse a water requirement int into a string
bool parseWaterReq(int waterReq, int bufferLen, char reqStr[]) {
  switch (waterReq) {
    case water:
      snprintf(reqStr, bufferLen, "%s", "water");
      break;
    case wet:
      snprintf(reqStr, bufferLen, "%s", "wet");
      break;
    case moist:
      snprintf(reqStr, bufferLen, "%s", "moist");
      break;
    case dry:
      snprintf(reqStr, bufferLen, "%s", "dry");
      break;
    default:
      return 0;
      break;
  }
  return 1;
}

// Parse a light requirement int into a string
bool parseLightReq(int lightReq, int bufferLen, char reqStr[]) {
  switch (lightReq) {
    case fullShade:
      snprintf(reqStr, bufferLen, "%s", "full shade");
      break;
    case partialSun:
      snprintf(reqStr, bufferLen, "%s", "partial sun");
      break;
    case fullSun:
      snprintf(reqStr, bufferLen, "%s", "full sun");
      break;
    default:
      return 0;
      break;
  }
  return 1;
}

// Global container pointer for web API access
Container* globalContainer = nullptr;

// Function to get active plant sensor averages (for web API)
void getActivePlantAverages(float& avgLight, float& avgTemp, float& avgWater, float& avgHumidity) {
  
  if (globalContainer != nullptr) {

    avgLight = globalContainer->activePlant.avgLight;
    avgTemp = globalContainer->activePlant.avgTemp;
    avgWater = globalContainer->activePlant.avgWater;
    avgHumidity = globalContainer->activePlant.avgHumidity;
  } 
  else {
    
    // Default values if container not set
    avgLight = 0;
    avgTemp = 0;
    avgWater = 0;
    avgHumidity = 0;
  }
}
