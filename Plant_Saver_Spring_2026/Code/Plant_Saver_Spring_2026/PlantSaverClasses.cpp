#include "Arduino.h"
#include "PlantSaverClasses.h"
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Time.h>  //necessary for keeping track of time through deep-sleep cycles
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
  : commonName{}, scientificName{}, fact{}, lightReq{}, waterReq{}, hardiness{}, error(errorRef) {
  id = 0;
  avgLight = 0;
  avgWater = 0;
  avgHumidity = 0;
  avgTemp = 0;
  lightEval = 0;
  waterEval = 0;
  humidityEval = 0;
  tempEval = 0;
  plantPulled = 0;
}

// Take average of sensor readings
float Plant::getAvgReading(JsonDocument sensorDoc) {
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

// Check all average values against thresholds
void Plant::checkThresholds() {
  lightCheck();
  waterCheck();
  tempCheck();
  humidityCheck();
}

// Map light requirements to thresholds, then check average reading
//
// TODO: Re-vamp to work w/ solar cells
//
void Plant::lightCheck() {
  int lightReqLowHigh[2] = { 0 };  // [0] = low value, [1] = high value
  lightReqLowHigh[0] = lightReq[0];
  lightReqLowHigh[1] = (lightReq[1] != 0) ? lightReq[1] : lightReq[0];
  int thresholds[2] = { 0 };  // in lux
  for (int i = 0; i < 2; i++) {
    switch (lightReqLowHigh[i]) {
      case fullShade:
        thresholds[i] = 0 + 1075 * i;  // 0 to 1075 lux
        break;
      case partialSun:
        thresholds[i] = 1075 + 9675 * i;  // 1075 to 10750 lux
        break;
      case fullSun:
        thresholds[i] = 10750 + 999999 * i;  // 10750+ , top end is arbitrary
        break;
      default:
        lightEval = evalUnknown;
        return;
    }
  }
  if (avgLight >= thresholds[0] && avgLight <= thresholds[1]) {
    lightEval = evalOK;
  } else if (avgLight < thresholds[0]) {
    lightEval = evalLow;
  } else if (avgLight > thresholds[1]) {
    lightEval = evalHigh;
  }
}

// Map hardiness zones to temperature thresholds, then check average reading
void Plant::tempCheck() {
  int hardinessLowHigh[2];  // [0] = low value, [1] = high value
  hardinessLowHigh[0] = hardiness[0];
  hardinessLowHigh[1] = (hardiness[1] != 0) ? hardiness[1] : hardiness[0];
  int thresholds[2];  // in degrees F
  for (int i = 0; i < 2; i++) {
    switch (hardinessLowHigh[i]) {
      case 2:
        thresholds[i] = 26 + 4 * i;
        break;
      case 3:
        thresholds[i] = 32 + 4 * i;
        break;
      case 4:
        thresholds[i] = 39 + 4 * i;
        break;
      case 5:
        thresholds[i] = 45 + 3 * i;
        break;
      case 6:
        thresholds[i] = 50 + 4 * i;
        break;
      case 7:
        thresholds[i] = 54 + 3 * i;
        break;
      case 8:
        thresholds[i] = 61 + 3 * i;
        break;
      case 9:
        thresholds[i] = 64 + 4 * i;
        break;
      case 10:
        thresholds[i] = 68 + 4 * i;
        break;
      case 11:
        thresholds[i] = 75 + 4 * i;
        break;
      case 12:
        thresholds[i] = 80 + 20 * i;
        break;
      case 13:
        thresholds[i] = 80 + 20 * i;
        break;
      default:
        tempEval = evalUnknown;
        return;
    }
  }
  if (avgTemp >= thresholds[0] && avgTemp <= thresholds[1]) {
    tempEval = evalOK;
  } else if (avgTemp < thresholds[0]) {
    tempEval = evalLow;
  } else if (avgTemp > thresholds[1]) {
    tempEval = evalHigh;
  }
}

// Map water requirements and humidity readings to thresholds, then check average readings
void Plant::waterCheck() {
  int waterReqLowHigh[2];  // [0] = low value, [1] = high value
  waterReqLowHigh[0] = waterReq[0];
  waterReqLowHigh[1] = (waterReq[1] != 0) ? waterReq[1] : waterReq[0];
  int thresholds[2];  // in ADC counts
  for (int i = 0; i < 2; i++) {
    switch (waterReqLowHigh[i]) {
      case water:
        thresholds[i] = 0 + 1000 * i;  // 0 to 1000
        break;
      case wet:
        thresholds[i] = 1000 + 650 * i;  // 1000 to 1650
        break;
      case moist:
        thresholds[i] = 1650 + 650 * i;  // 1650 to 2300
        break;
      case dry:
        thresholds[i] = 2300 + 1795 * i;  // 2300 to 4095 (max)
        break;
      default:
        lightEval = evalUnknown;
        return;
    }
  }
  if (avgWater >= thresholds[0] && avgWater <= thresholds[1]) {
    waterEval = evalOK;
  } else if (avgWater < thresholds[0]) {  // lower reading = more water
    waterEval = evalHigh;
  } else if (avgWater > thresholds[1]) {
    waterEval = evalLow;
  }
}

// Check average humidity values against static thresholds
void Plant::humidityCheck() {
  if (avgHumidity <= 60 && avgHumidity >= 30) {
    humidityEval = evalOK;
  } else if (avgHumidity < 30) {
    humidityEval = evalLow;
  } else if (avgHumidity > 60) {
    humidityEval = evalHigh;
  }
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
  const char* jsonFact = plantDoc["fact"];
  snprintf(fact, NUM_CHARS_FACT, "%s", jsonFact);
  lightReq[0] = plantDoc["lightReq"][0];
  lightReq[1] = plantDoc["lightReq"][1];
  waterReq[0] = plantDoc["waterReq"][0];
  waterReq[1] = plantDoc["waterReq"][1];
  hardiness[0] = plantDoc["hardiness"][0];
  hardiness[1] = plantDoc["hardiness"][1];
  avgLight = plantDoc["avgLight"];
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
  plantDoc["fact"] = fact;
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
SensorReading::SensorReading(){
  tempReading = 0;
  waterReading = 0;
  humidityReading = 0;
  lightReading = 0;
}

/*----------------------------------------------------------- Container Class --------------------------------------------------------------*/

// Initialization
Container::Container()
  : activePlant(error), error(), header(error), sensorReading(), interface(error, activePlant) {
  activeMode = startupMode;
}

// Add new sensor data to the JsonDocument.
// Each sensor reading array is treated as a circular buffer
JsonDocument Container::addSensorReading(JsonDocument sensorDoc, float reading) {
  int startIndex = sensorDoc["startIndex"];
  sensorDoc["readings"][startIndex] = reading;
  startIndex = (startIndex + 1) % MAX_SENSOR_READINGS;
  sensorDoc["startIndex"] = startIndex;
  int numReadings = sensorDoc["numReadings"];
  if (numReadings < MAX_SENSOR_READINGS) {
    sensorDoc["numReadings"] = numReadings + 1;
  }
  return sensorDoc;
}

// Pull in plant data, add new readings, take averages, then push back to storage files
// To avoid excessive memory usage, each file is modified separately
void Container::updatePlantData() {
  char fileName[MAX_CHARS_FILENAME] = { 0 };
  JsonDocument sensorDoc;
  for (int i = 0; i < 4; i++) {
    switch (i) {
      case lightFile:
        snprintf(fileName, MAX_CHARS_FILENAME, LIGHT_PATH);
        sensorDoc = readSDFile(fileName);
        if (sensorDoc.isNull()) {
          error.addError(fileOperation);
          return;
        }
        sensorDoc = addSensorReading(sensorDoc, sensorReading.lightReading);
        activePlant.avgLight = activePlant.getAvgReading(sensorDoc);
        break;
      case waterFile:
        snprintf(fileName, MAX_CHARS_FILENAME, WATER_PATH);
        sensorDoc = readSDFile(fileName);
        if (sensorDoc.isNull()) {
          error.addError(fileOperation);
          return;
        }
        sensorDoc = addSensorReading(sensorDoc, sensorReading.waterReading);
        activePlant.avgWater = activePlant.getAvgReading(sensorDoc);
        break;
      case humidityFile:
        snprintf(fileName, MAX_CHARS_FILENAME, HUMIDITY_PATH);
        sensorDoc = readSDFile(fileName);
        if (sensorDoc.isNull()) {
          error.addError(fileOperation);
          return;
        }
        sensorDoc = addSensorReading(sensorDoc, sensorReading.humidityReading);
        activePlant.avgHumidity = activePlant.getAvgReading(sensorDoc);
        break;
      case tempFile:
        snprintf(fileName, MAX_CHARS_FILENAME, TEMP_PATH);
        sensorDoc = readSDFile(fileName);
        if (sensorDoc.isNull()) {
          error.addError(fileOperation);
          return;
        }
        sensorDoc = addSensorReading(sensorDoc, sensorReading.tempReading);
        activePlant.avgTemp = activePlant.getAvgReading(sensorDoc);
        break;
    }
    int pushJsonError = pushJsonDoc(sensorDoc, fileName);
    if (pushJsonError) {
      error.addError(jsonError);
    }
    sensorDoc.clear();
  }
}

// Clear out data associated with the existing user plant (apart from average readings)
// Create a new user plant from selected DB plant data
void Container::newUserPlant() {
  clearSensorData();
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
  const char* fact = plantDoc["cultivation_fact"];
  snprintf(activePlant.fact, NUM_CHARS_FACT, "%s", fact);
  JsonArray jsonHardinessVals = plantDoc["data"][0]["value"];
  JsonArray jsonLightReqs = plantDoc["data"][1]["value"];
  JsonArray jsonWaterReqs = plantDoc["data"][2]["value"];
  activePlant.hardiness[0] = jsonHardinessVals[0];  // Only need first and last elements of each
  activePlant.hardiness[1] = (jsonHardinessVals.size() > 1) ? jsonHardinessVals[jsonHardinessVals.size() - 1] : 0;
  activePlant.lightReq[0] = jsonLightReqs[0];
  activePlant.lightReq[1] = (jsonLightReqs.size() > 1) ? jsonLightReqs[jsonLightReqs.size() - 1] : 0;
  activePlant.waterReq[0] = jsonWaterReqs[0];
  activePlant.waterReq[1] = (jsonWaterReqs.size() > 1) ? jsonWaterReqs[jsonWaterReqs.size() - 1] : 0;
  plantDoc.clear();
  header.plantSelected = 1;
}

// Remove all sensor readings for the currently selected plant
void Container::clearSensorData() {
  for (int i = 0; i < 4; i++) {
    char fileName[MAX_CHARS_FILENAME] = { 0 };
    JsonDocument emptyDoc;
    JsonArray readings;
    switch (i) {
      case lightFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "%s", LIGHT_PATH);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
      case waterFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "%s", WATER_PATH);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
      case humidityFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "%s", HUMIDITY_PATH);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
      case tempFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "%s", TEMP_PATH);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
    }
    int pushJsonError = pushJsonDoc(emptyDoc, fileName);
    emptyDoc.clear();
    readings.clear();
    if (pushJsonError) {
      error.addError(pushJsonError);
      return;
    }
  }
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
  : activeMenu{}, selectedPlantIndex{}, numSelectCandidates{}, displayPlantIDs{}, displayPlantNames{}, selectedQueryChar{},
    displayIndices{0, 1, 2}, displayMap{0, 1, 2, 0}, query{'A','A','A','A','A'}, error(errorRef), activePlant(plantRef) {
      
      activeMenu = 0;
      selectedPlantIndex = 0;
      numSelectCandidates = 0;
      selectedQueryChar = 0;
      screenFocus = false;
      numMenus = 4; // Starts at 4, if data cached then 5
    
}

// Initialize the display
bool Interface::begin(uint8_t vcs, uint8_t addr) {
  return display.begin(vcs, addr);
}

// Build and display the main menu
void Interface::displayMainMenu() {
  int startX = 5;
  int padX = 15;
  int padY = 1;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(horizontalCenterText(activePlant.commonName, NUM_CHARS_NAME, 1), 0);
  display.println(activePlant.commonName);
  // Water
  // 
  // TODO: Need a more digestible way of showing this value
  //
  display.drawBitmap(startX + 2, Y_SCALE + padY - 1, waterBmp, 7, 13, 1);
  display.setCursor(startX + padX, Y_SCALE + padY + 3 - 1);
  display.printf("%.0f cts", activePlant.avgWater);
  // Light
  display.drawBitmap(startX, Y_SCALE + 13 + padY*2, lightBmp, 11, 11, 1);
  display.setCursor(startX + padX, Y_SCALE + 13 + padY*2 + 2);
  display.printf("%.0f lux", activePlant.avgLight);
  // Temp
  display.drawBitmap(startX + 1, Y_SCALE + 13 + 11 + padY*3, tempBmp, 8, 13, 1);
  display.setCursor(startX + padX, Y_SCALE + 13 + 11 + padY*3 + 3);
  char tempReport[5] = {};
  int tempCharCt = snprintf(tempReport, 5, "%.0f", activePlant.avgTemp);
  display.print(tempReport);
  display.drawBitmap(startX + padX + tempCharCt*X_SCALE + 1, Y_SCALE + 13 + 11 + padY*3 + 3, degFBmp, 8, 8, 1);
  // Humidity
  display.drawBitmap(startX + 1, Y_SCALE + 13 + 11 + 13 + padY*4 + 1, rhBmp, 9, 13, 1);
  display.setCursor(startX + padX, Y_SCALE + 13 + 11 + 13 + padY*4 + 3 + 1);
  display.printf("%.0f%% %c", activePlant.avgHumidity);
  // Testing
  display.drawBitmap(SCREEN_WIDTH - 52 - 1, Y_SCALE + padY, testBmp, 52, 11, 1);
  display.drawBitmap(SCREEN_WIDTH - 52 - 1, Y_SCALE + 13 + padY*2, testBmp, 52, 11, 1);
  display.drawBitmap(SCREEN_WIDTH - 52 - 1, Y_SCALE + 13 + 11 + padY*3, testBmp, 52, 11, 1);
  display.drawBitmap(SCREEN_WIDTH - 52 - 1, Y_SCALE + 13 + 11 + 13 + padY*4 + 1, testBmp, 52, 11, 1);
  display.display();
  activeMenu = mainMenu;
}

// Build and display the info menu
void Interface::displayInfoMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(horizontalCenterText(activePlant.commonName, NUM_CHARS_NAME, 1), 0);
  display.println(activePlant.commonName);
  display.setCursor(horizontalCenterText(activePlant.scientificName, NUM_CHARS_NAME, 1), 10);
  display.println(activePlant.scientificName);
  display.setCursor(0, 30);
  display.println(activePlant.fact);
  display.display();
  activeMenu = infoMenu;
}

// Build and display query input menu
// 
// TODO: ugly, make pretty
//
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
    display.drawBitmap(startX + (dx*selectedQueryChar) - 3, startY - 8 - 6, upArrowBmp, 16, 8, 1);
    display.drawBitmap(startX + (dx*selectedQueryChar) - 3, startY + Y_SCALE*2 + 6, downArrowBmp, 16, 8, 1);
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
  display.fillRect(0, 25, 3*display.width()/4, 20, SSD1306_WHITE);
  switch (displayMap[3]) {
    case 0:
      if (numSelectCandidates > 0) {
        display.setCursor(0, 30);
        display.setTextColor(SSD1306_BLACK);
        display.println(displayPlantNames[displayMap[0]]);
      }
      if (numSelectCandidates > 1) {
        display.setCursor(0, 55);
        display.setTextColor(SSD1306_WHITE);
        display.println(displayPlantNames[displayMap[1]]);
      }
      break;
    case 1:
      if (numSelectCandidates > 0) {
        display.setCursor(0, 10);
        display.setTextColor(SSD1306_WHITE);
        display.println(displayPlantNames[displayMap[0]]);
      }
      if (numSelectCandidates > 1) {
        display.setCursor(0, 30);
        display.setTextColor(SSD1306_BLACK);
        display.println(displayPlantNames[displayMap[1]]);
      }
      if (numSelectCandidates > 2) {
        display.setCursor(0, 55);
        display.setTextColor(SSD1306_WHITE);
        display.println(displayPlantNames[displayMap[2]]);
      }
      break;
    case 2:
      display.setCursor(0, 10);
      display.setTextColor(SSD1306_WHITE);
      display.println(displayPlantNames[displayMap[1]]);
      display.setCursor(0, 30);
      display.setTextColor(SSD1306_BLACK);
      display.println(displayPlantNames[displayMap[2]]);
      break;
  }
  display.display();
  activeMenu = selectMenu;
  numMenus = 5;
}

// Return a character to represent a threshold evaluation
char Interface::getEvalIndicator(int eval) {
  switch (eval) {
    case evalUnknown:
      return '?';
    case evalLow:
      return 'v';
    case evalHigh:
      return '^';
    case evalOK:
      return '-';
  }
  return '?';
}

//
// TODO: Rework eval indication system - use this function to draw the recommendation indicator
// Probably will need to have the level checks return the threshold values as well as a min/max
// to use in this function.
//
char Interface::displayRecommendation(int recInd, int min, int max, int lowThresh, int highThresh, int val) {

}

// Increment/Decrement the current query position alphabetically
void Interface::indexQueryPos(bool upDir) {
  uint8_t pos = selectedQueryChar;
  if (!upDir) { // Down
    if (query[pos] >= 65 && query[pos] < 90) { // A-Y
      query[pos]++;
    } else if (query[pos] == 90) { // 'Z'
      query[pos] = '_';
    } else if (query[pos] == 95) { // '_'
      query[pos] = '\0';
    } else { // ''
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
    } else {
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
    dbFile.find("\"name\":\"");
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
    dbFile.find("id\": "); // TODO: Make sure this matches Jackson's scripts
    char idChar[MAX_DIGITS_ID + 1] = {}; // Max 999,999
    uint8_t idDigitCt = dbFile.readBytesUntil(',', idChar, MAX_DIGITS_ID + 1);
    dbFile.find("\"name\":\""); // Spaces or nah?
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
int pullPlant(char fileName[], int id, JsonDocument& doc) {
  File file = SD.open(fileName, FILE_READ);
  char searchID1[20] = {};
  uint8_t search1CharCt = snprintf(searchID1, 20, "{\"id\": %d", id);
  bool found = file.find(searchID1);
  if (!found) {
    file.close();
    return fileOperation;
  }
  unsigned long marker1 = file.position() - search1CharCt;
  uint8_t interListTerminalCt = 7;
  uint8_t endListTerminalCt = 2;
  found = file.findUntil(",{\"id\":", "}]}");
  unsigned long marker2 = file.position();
  marker2 = found ? marker2 - interListTerminalCt : marker2 - endListTerminalCt;
  const int len = marker2 - marker1 + 1;
  char raw[len] = {};
  file.seek(marker1);
  file.readBytes(raw, len - 1);
  file.close();
  deserializeJson(doc, raw);
  return noError;
}

// Standalone helper for centering text horizontally
int horizontalCenterText(char text[], int bufferLen, int fontSize) {
  int len = 0;
  for (int i = 0; i < bufferLen; i++) {
    if (text[i] == '\0') {
      len = i;
      break;
    }
    else {
      len = i + 1;
    }
  }
  int startPt = int((SCREEN_WIDTH/2) - ((fontSize*X_SCALE*len)/2));
  startPt = startPt >= 0 ? startPt : 0;
  return startPt;
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
