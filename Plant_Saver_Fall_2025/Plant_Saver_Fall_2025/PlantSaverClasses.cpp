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

/*--------------------------------------------------------- DBPlant Class ---------------------------------------------------------*/

// Initialization
DBPlant::DBPlant()
  : commonName{}, scientificName{}, fact{}, lightReq{}, waterReq{}, hardiness{} {}

/*---------------------------------------------------------- Plant Class ----------------------------------------------------------*/

// Initialization
Plant::Plant()
  : commonName{}, scientificName{}, fact{}, lightReq{}, waterReq{}, hardiness{} {
  selfID = 0;
  baseID = 0;
  avgLight = 0;
  avgWater = 0;
  avgHumidity = 0;
  avgTemp = 0;
  lightEval = 0;
  waterEval = 0;
  humidityEval = 0;
  tempEval = 0;
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

// Check all average values against thresholds
void Plant::checkThresholds() {
  lightCheck();
  waterCheck();
  tempCheck();
  humidityCheck();
}

// Map light requirements to thresholds, then check average reading
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

/*---------------------------------------------------------- Sensor Reading Class ----------------------------------------------------------*/

// Initialization
SensorReading::SensorReading()
  : timeStamp{} {
  tempReading = 0;
  waterReading = 0;
  humidityReading = 0;
  lightReading = 0;
  plantID = 0;
}

/*----------------------------------------------------------- Container Class --------------------------------------------------------------*/

// Initialization
Container::Container()
  : activePlant(), error(), header(), sensorReading(), interface(), plants{} {
  activeMode = startupMode;
  plantPulled = 0;
  dbPlantsPulled = 0;
  headerPulled = 0;
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

// Add a new timestamp to the array in FIFO format. numReadings is stored as JSON, readings are just raw text data
// To avoid pulling hundreds of strings at a time, the function
// 1. Pulls from the dates file one by one, writing each back into a temporary file
// 2. Places the new timestamp at the top of the data
// 3. Reads from the temporary file character by character into the original file
// 4. Deletes the temporary file, leaving the modified original file
void Container::addTimeStamp() {
  char fileName[MAX_CHARS_FILENAME] = { 0 };
  snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/dates.txt", header.activePlantID);
  char tempFileName[MAX_CHARS_FILENAME] = { 0 };
  snprintf(tempFileName, MAX_CHARS_FILENAME, "/plant%i/tmp.txt", header.activePlantID);
  File inputFile = SD.open(fileName, FILE_READ);
  File outputFile = SD.open(tempFileName, FILE_WRITE);
  if (!inputFile || !outputFile) {
    error.addError(fileOperation);
    inputFile.close();
    outputFile.close();
    return;
  }
  JsonDocument timeParamsDoc;
  JsonDocument filter;
  filter["numReadings"] = true; // Only pull out data associated with the numReadings key
  DeserializationError jsonDeserializationError = deserializeJson(timeParamsDoc, inputFile,
                                                                  DeserializationOption::Filter(filter));
  if (jsonDeserializationError) {
    inputFile.close();
    outputFile.close();
    SD.remove(tempFileName);
    error.addError(jsonError);
    return;
  }
  int numReadings = timeParamsDoc["numReadings"];
  numReadings = (numReadings < MAX_SENSOR_READINGS) ? numReadings + 1 : numReadings;
  timeParamsDoc["numReadings"] = numReadings;
  serializeJson(timeParamsDoc, outputFile);
  timeParamsDoc.clear();
  outputFile.print("\r\n"); // Need a newline after every timestamp for future operations
  inputFile.seek(0);
  inputFile.find("}");
  outputFile.println(sensorReading.timeStamp);
  if (numReadings - 1 > 0) {
    inputFile.seek(inputFile.position() + 2);  // go past newline to start reading
  }
  for (int i = 1; i < numReadings; i++) {
    char timeStampCopy[NUM_CHARS_TIMESTAMP] = { 0 };
    inputFile.readBytesUntil('\r', timeStampCopy, NUM_CHARS_TIMESTAMP);
    outputFile.println(timeStampCopy);
    inputFile.seek(inputFile.position() + 1);
  }
  inputFile.close();
  outputFile.close();
  inputFile = SD.open(tempFileName, FILE_READ);  // cannot rename files, so write back instead
  if (!inputFile) {
    error.addError(fileOperation);
    return;
  }
  outputFile = SD.open(fileName, FILE_WRITE);
  if (!outputFile) {
    inputFile.close();
    error.addError(fileOperation);
    return;
  }
  while (inputFile.available()) {
    outputFile.write((char)inputFile.read());
  }
  outputFile.close();
  inputFile.close();
  SD.remove(tempFileName);
}

// Pull in plant data, add new readings, take averages, then push back to storage files
// To avoid excessive memory usage, each file is modified separately
void Container::updatePlantData() {
  int plantID = header.activePlantID;
  char fileName[MAX_CHARS_FILENAME] = { 0 };
  JsonDocument sensorDoc;
  for (int i = 0; i < 4; i++) {
    switch (i) {
      case lightFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/light.txt", plantID);
        sensorDoc = readSDFile(fileName);
        if (sensorDoc.isNull()) {
          error.addError(fileOperation);
          return;
        }
        sensorDoc = addSensorReading(sensorDoc, sensorReading.lightReading);
        activePlant.avgLight = activePlant.getAvgReading(sensorDoc);
        break;
      case waterFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/water.txt", plantID);
        sensorDoc = readSDFile(fileName);
        if (sensorDoc.isNull()) {
          error.addError(fileOperation);
          return;
        }
        sensorDoc = addSensorReading(sensorDoc, sensorReading.waterReading);
        activePlant.avgWater = activePlant.getAvgReading(sensorDoc);
        break;
      case humidityFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/humidity.txt", plantID);
        sensorDoc = readSDFile(fileName);
        if (sensorDoc.isNull()) {
          error.addError(fileOperation);
          return;
        }
        sensorDoc = addSensorReading(sensorDoc, sensorReading.humidityReading);
        activePlant.avgHumidity = activePlant.getAvgReading(sensorDoc);
        break;
      case tempFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/temp.txt", plantID);
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
  this->addTimeStamp();
}

// Pull in the header data from the SD and parse it into a header object
void Container::pullHeader() {
  JsonDocument headerDoc;
  char fileName[12] = "/header.txt";
  headerDoc = readSDFile(fileName);
  if (headerDoc.isNull()) {
    error.addError(fileOperation);
    return;
  }
  header.numDBPlants = headerDoc["numDBPlants"];
  header.activePlantID = headerDoc["activePlantID"];
  const char* date = headerDoc["date"];
  snprintf(header.date, NUM_CHARS_TIMESTAMP, "%s", date);
  header.lightThreshold = headerDoc["lightThreshold"];
  header.tempThreshold = headerDoc["tempThreshold"];
  header.waterThreshold = headerDoc["waterThreshold"];
  header.humidityThreshold = headerDoc["humidityThreshold"];
  headerDoc.clear();
  headerPulled = 1;
}

// Take data from the header object and push it back into the header file
void Container::pushHeader() {
  JsonDocument headerDoc;
  headerDoc["numDBPlants"] = header.numDBPlants;
  headerDoc["activePlantID"] = header.activePlantID;
  getTimeStr(header.date);  // Header date should always be the time of shutdown
  headerDoc["date"] = header.date;
  headerDoc["lightThreshold"] = header.lightThreshold;
  headerDoc["tempThreshold"] = header.tempThreshold;
  headerDoc["waterThreshold"] = header.waterThreshold;
  headerDoc["humidityThreshold"] = header.humidityThreshold;
  char fileName[MAX_CHARS_FILENAME] = "/header.txt";
  int pushJsonError = pushJsonDoc(headerDoc, fileName);
  if (pushJsonError) {
    error.addError(jsonError);
  }
  headerDoc.clear();
}

// Pull data from the plant file of the active plant's folder and parse it into a plant object
void Container::pullPlant() {
  char fileName[MAX_CHARS_FILENAME] = { 0 };
  snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/plant.txt", header.activePlantID);
  JsonDocument plantDoc = readSDFile(fileName);
  if (plantDoc.isNull()) {
    error.addError(fileOperation);
    return;
  }
  activePlant.selfID = plantDoc["selfID"];
  activePlant.baseID = plantDoc["baseID"];
  const char* commonName = plantDoc["commonName"];
  snprintf(activePlant.commonName, NUM_CHARS_NAME, "%s", commonName);
  const char* scientificName = plantDoc["scientificName"];
  snprintf(activePlant.scientificName, NUM_CHARS_NAME, "%s", scientificName);
  const char* fact = plantDoc["fact"];
  snprintf(activePlant.fact, NUM_CHARS_FACT, "%s", fact);
  activePlant.lightReq[0] = plantDoc["lightReq"][0];
  activePlant.lightReq[1] = plantDoc["lightReq"][1];
  activePlant.waterReq[0] = plantDoc["waterReq"][0];
  activePlant.waterReq[1] = plantDoc["waterReq"][1];
  activePlant.hardiness[0] = plantDoc["hardiness"][0];
  activePlant.hardiness[1] = plantDoc["hardiness"][1];
  activePlant.avgLight = plantDoc["avgLight"];
  activePlant.avgWater = plantDoc["avgWater"];
  activePlant.avgHumidity = plantDoc["avgHumidity"];
  activePlant.avgTemp = plantDoc["avgTemp"];
  plantPulled = 1;
  plantDoc.clear();
}

// Take data from a plant object and push it into the plant file of the active plant's folder
void Container::pushPlant() {
  char fileName[MAX_CHARS_FILENAME] = { 0 };
  snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/plant.txt", header.activePlantID);
  JsonDocument plantDoc;
  plantDoc["selfID"] = activePlant.selfID;
  plantDoc["baseID"] = activePlant.baseID;
  plantDoc["commonName"] = activePlant.commonName;
  plantDoc["scientificName"] = activePlant.scientificName;
  plantDoc["fact"] = activePlant.fact;
  JsonArray jsonLightReq = plantDoc["lightReq"].to<JsonArray>();
  jsonLightReq.add(activePlant.lightReq[0]);
  jsonLightReq.add(activePlant.lightReq[1]);
  JsonArray jsonWaterReq = plantDoc["waterReq"].to<JsonArray>();
  jsonWaterReq.add(activePlant.waterReq[0]);
  jsonWaterReq.add(activePlant.waterReq[1]);
  JsonArray jsonHardiness = plantDoc["hardiness"].to<JsonArray>();
  jsonHardiness.add(activePlant.hardiness[0]);
  jsonHardiness.add(activePlant.hardiness[1]);
  plantDoc["avgLight"] = activePlant.avgLight;
  plantDoc["avgWater"] = activePlant.avgWater;
  plantDoc["avgHumidity"] = activePlant.avgHumidity;
  plantDoc["avgTemp"] = activePlant.avgTemp;
  int pushJsonError = pushJsonDoc(plantDoc, fileName);
  if (pushJsonError) {
    error.addError(pushJsonError);
  }
  plantDoc.clear();
}

// Pull up to NUM_DISPLAY_PLANTS from the database and parse into an array of DBPlant objects
void Container::getDBPlants() {
  char fileName[MAX_CHARS_FILENAME] = "/plantDB.txt";
  JsonDocument plantsDoc = readSDFile(fileName);
  if (plantsDoc.isNull()) {
    error.addError(fileOperation);
    return;
  }
  JsonArray jsonPlants = plantsDoc["plants"];
  int index = 0;
  for (JsonVariant plant : jsonPlants) {
    if (index >= NUM_DISPLAY_PLANTS) {
      break;
    }
    plants[index].id = plant["id"];
    const char* commonName = plant["name"];
    snprintf(plants[index].commonName, NUM_CHARS_NAME, "%s", commonName);
    JsonArray jsonHardinessVals = plant["data"][0]["value"];
    JsonArray jsonLightReqs = plant["data"][1]["value"];
    JsonArray jsonWaterReqs = plant["data"][2]["value"];
    plants[index].hardiness[0] = jsonHardinessVals[0];  // Only need first and last elements of each
    plants[index].hardiness[1] = (jsonHardinessVals.size() > 1) ? jsonHardinessVals[jsonHardinessVals.size() - 1] : 0;
    plants[index].lightReq[0] = jsonLightReqs[0];
    plants[index].lightReq[1] = (jsonLightReqs.size() > 1) ? jsonLightReqs[jsonLightReqs.size() - 1] : 0;
    plants[index].waterReq[0] = jsonWaterReqs[0];
    plants[index].waterReq[1] = (jsonWaterReqs.size() > 1) ? jsonWaterReqs[jsonWaterReqs.size() - 1] : 0;
    const char* scientificName = plant["scientific_name"];
    snprintf(plants[index].scientificName, NUM_CHARS_NAME, "%s", scientificName);
    const char* fact = plant["cultivation_fact"];
    snprintf(plants[index].fact, NUM_CHARS_FACT, "%s", fact);
    index++;
  }
  plantsDoc.clear();
  dbPlantsPulled = 1;
}

// Clear out data associated with the existing user plant (apart from average readings)
// Create a new user plant from selected DB plant data
void Container::newUserPlant(int newSelfID) {
  clearSensorData();
  int index = interface.selectedPlantIndex;
  activePlant.selfID = newSelfID;
  activePlant.baseID = plants[index].id;
  snprintf(activePlant.commonName, NUM_CHARS_NAME, "%s", plants[index].commonName);
  snprintf(activePlant.scientificName, NUM_CHARS_NAME, "%s", plants[index].scientificName);
  snprintf(activePlant.fact, NUM_CHARS_FACT, "%s", plants[index].fact);
  activePlant.lightReq[0] = plants[index].lightReq[0];
  activePlant.lightReq[1] = plants[index].lightReq[1];
  activePlant.waterReq[0] = plants[index].waterReq[0];
  activePlant.waterReq[1] = plants[index].waterReq[1];
  activePlant.hardiness[0] = plants[index].hardiness[0];
  activePlant.hardiness[1] = plants[index].hardiness[1];
  header.activePlantID = newSelfID;
}

// Remove all sensor readings for the currently selected plant
void Container::clearSensorData() {
  for (int i = 0; i < 5; i++) {
    char fileName[MAX_CHARS_FILENAME] = { 0 };
    JsonDocument emptyDoc;
    JsonArray readings;
    switch (i) {
      case lightFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/light.txt", header.activePlantID);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
      case waterFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/water.txt", header.activePlantID);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
      case humidityFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/humidity.txt", header.activePlantID);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
      case tempFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/temp.txt", header.activePlantID);
        emptyDoc["startIndex"] = 0;
        emptyDoc["numReadings"] = 0;
        readings = emptyDoc["readings"].to<JsonArray>();
        break;
      case datesFile:
        snprintf(fileName, MAX_CHARS_FILENAME, "/plant%i/dates.txt", header.activePlantID);
        emptyDoc["numReadings"] = 0;
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
Header::Header()
  : date{} {
  activePlantID = 0;
  numDBPlants = 0;
  lightThreshold = 0;
  tempThreshold = 0;
  waterThreshold = 0;
  humidityThreshold = 0;
}

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

/*----------------------------------------------------------------- Interface Class ----------------------------------------------------------------*/

// Initialization
Interface::Interface()
  : activeMenu{}, selectedPlantIndex{} {}

// Initialize the display
bool Interface::begin(uint8_t vcs, uint8_t addr) {
  return display.begin(vcs, addr);
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

// Build and display the main menu
void Interface::displayMainMenu(Plant activePlant) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(activePlant.commonName);
  display.setCursor(0, 10);
  display.printf("Water lvl %.0f %c", activePlant.avgWater, getEvalIndicator(activePlant.waterEval));
  display.setCursor(0, 20);
  display.printf("Light lvl %.0f %c", activePlant.avgLight, getEvalIndicator(activePlant.lightEval));
  display.setCursor(0, 30);
  display.printf("Temp lvl %.0f %c", activePlant.avgTemp, getEvalIndicator(activePlant.tempEval));
  display.setCursor(0, 40);
  display.printf("RH lvl %.0f %c", activePlant.avgHumidity, getEvalIndicator(activePlant.humidityEval));
  display.display();
  activeMenu = mainMenu;
}

// Build and display the info menu
void Interface::displayInfoMenu(Plant activePlant) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(activePlant.commonName);
  display.setCursor(0, 10);
  display.println(activePlant.scientificName);
  display.setCursor(0, 30);
  display.println(activePlant.fact);
  display.display();
  activeMenu = infoMenu;
}

// Build and display the plant selection menu
void Interface::displaySelectMenu(char plantName[]) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(plantName);
  display.display();
  activeMenu = selectMenu;
}

// Cycle through available screens
void Interface::nextScreen(Plant activePlant, char plantName[]) {
  activeMenu = (activeMenu + 1) % (NUM_MENUS + 1);
  activeMenu = activeMenu == 0 ? 1 : activeMenu;
  switch (activeMenu) {
    case noMenu:
      displayMainMenu(activePlant);
      break;
    case mainMenu:
      displayMainMenu(activePlant);
      break;
    case infoMenu:
      displayInfoMenu(activePlant);
      break;
    case selectMenu:
      displaySelectMenu(plantName);
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

// Fills a pre-allocated buffer with a date string matching ISO 8601, millseconds excluded
void getTimeStr(char* buffer) {
  time_t now;
  struct tm timeInfo;

  time(&now);
  localtime_r(&now, &timeInfo);

  snprintf(buffer, NUM_CHARS_TIMESTAMP, "%d-%02d-%02d %02d:%02d:%02d",
           timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
           timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
}

// Set the the local RTC time using a date string matching ISO 8601, milliseconds excluded
bool setTimeFromTimeStr(char timeStr[]) {
  for (int i = 0; i < 19; i++) {
    if (timeStr[i] == '\0') {
      return 0;
    }
  }
  char secChar[3] = { 0 };
  memcpy(secChar, timeStr + 17, 2);
  int sec = atoi(secChar);
  char minChar[3] = { 0 };
  memcpy(minChar, timeStr + 14, 2);
  int min = atoi(minChar);
  char hourChar[3] = { 0 };
  memcpy(hourChar, timeStr + 11, 2);
  int hour = atoi(hourChar);
  char dayChar[3] = { 0 };
  memcpy(dayChar, timeStr + 8, 2);
  int day = atoi(dayChar);
  char monthChar[3] = { 0 };
  memcpy(monthChar, timeStr + 5, 2);
  int month = atoi(monthChar);
  char yearChar[5] = { 0 };
  memcpy(yearChar, timeStr, 4);
  int year = atoi(yearChar);
  struct tm timeInfo;
  timeInfo.tm_year = year - 1900;
  timeInfo.tm_mon = month - 1;
  timeInfo.tm_mday = day;
  timeInfo.tm_hour = hour;
  timeInfo.tm_min = min;
  timeInfo.tm_sec = sec;
  time_t epoch = mktime(&timeInfo);
  struct timeval tv;
  if (epoch > 2082758399) {
    tv.tv_sec = epoch - 2082758399;
  } else {
    tv.tv_sec = epoch;
  }
  tv.tv_usec = 0;
  if (settimeofday(&tv, NULL) == -1) {
    return 0;
  }
  return 1;
}
