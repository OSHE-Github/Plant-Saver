#include "esp32-hal-rmt.h"
#ifndef PlantSaverClasses_h
#define PlantSaverClasses_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "PlantSaverAssets.h"

/*------------------------------------------------------------ Macros ------------------------------------------------------------*/

// Misc.
#define ERROR_IND_PIN 4  // Error indication LED
#define MAX_CHARS_FILENAME 21
#define MAX_SENSOR_READINGS 200  // # of sensor readings allowed in FIFO
#define NUM_CHARS_NAME 50
#define NUM_CHARS_FACT 100
#define QUERY_DIFF_THRESH 3 // Diff. between query and candidate must be < this #
#define NUM_CHARS_QUERY 5
#define MAX_DIGITS_ID 6

// Display params
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // OLED Reset pin # (or -1 if sharing Arduino reset pin)
#define NUM_CANDIDATES_SHOWN 3
#define X_SCALE 6 // X (width) scaling for characters
#define Y_SCALE 8 // Y (height) scaling for characters
#define REC_WIDTH 52 // This stuff vvv for recommendation display
#define REC_HEIGHT 11
#define MARKER_WIDTH 5
#define MARKER_HEIGHT 8
#define THRESH_MARKER_HEIGHT 3

// Paths
#define PLANT_DB_PATH "/plantDB.txt"
#define HEADER_PATH "/header.txt"
#define PLANT_PATH "/Plant/plant.txt"
#define HUMIDITY_PATH "/Plant/humidity.txt"
#define LIGHT_PATH "/Plant/light.txt"
#define TEMP_PATH "/Plant/temp.txt"
#define WATER_PATH "/Plant/water.txt"
#define TMP_GATHER_PATH "/Tmp/gather.txt"
#define TMP_SORT_PATH "/Tmp/sort.txt"

/*------------------------------------------------------- Class Definitions -------------------------------------------------------*/

// Class to store/manipulate/report system errors
class Error {
public:
  Error();
  int getError(int errorStatus);
  void clearError(int errorStatus);
  void addError(int errorStatus);
  void indicateError();
  int highestPriority;
private:
  int _errorList[8];
  int _flashCt;
  int _flashDuration;
  bool _indicatorOn;
  unsigned long _startTime;
};

// Data of plants actively being monitored
class Plant {
public:
  Plant(Error& errorRef);
  float getAvgReading(JsonDocument sensorDoc);
  void checkThresholds();
  void pullPlant();
  void pushPlant();
  int id;  // ID within the larger plant database
  char commonName[NUM_CHARS_NAME];
  char scientificName[NUM_CHARS_NAME];
  char fact[NUM_CHARS_FACT];
  int lightReq[2];
  int waterReq[2];
  int hardiness[2];
  float avgLight;
  float avgWater;
  float avgHumidity;
  float avgTemp;
  // These variables ARE NOT stored:
  int lightEval;
  int waterEval;
  int humidityEval;
  int tempEval;
  bool plantPulled;
  Error& error;
private:
  void tempCheck();
  void waterCheck();
  void lightCheck();
  void humidityCheck();
};

// Data associated with an instanced multi-sensor reading
class SensorReading {
public:
  SensorReading();
  float tempReading;
  float waterReading;
  float humidityReading;
  float lightReading;
};

// Class for storing/retrieving header file data
class Header {
public:
  Header(Error& errorRef);
  void pullHeader();
  void pushHeader();
  int numDBPlants;  // Number of plants in the larger read-only database
  int lightThreshold;
  int tempThreshold;
  int waterThreshold;
  int humidityThreshold;
  bool plantSelected;
  bool headerPulled;
  Error& error;
};

// Class to store data/methods surrounding the user interface
class Interface {
public:
  Interface(Error& errorRef, Plant& plantRef);
  bool begin(uint8_t vcs, uint8_t addr);
  void displayMainMenu();
  void displayInfoMenu();
  void displayInputMenu();
  char getEvalIndicator(int eval);
  void indexQueryPos(bool upDir);
  void displaySelectMenu();
  void scrollSelectDown();
  void scrollSelectUp();
  void queryDBPlants();
  void pullCachedData(int index, char name[], int& id);
  void nextScreen(bool plantSelected);
  void displayOff();
<<<<<<< HEAD
  char displayRecommendation(int recInd, int min, int max, int lowThresh, int highThresh, int val);
=======
>>>>>>> origin/main
  int selectedPlantIndex;
  int activeMenu;
  int numSelectCandidates;
  int displayPlantIDs[NUM_CANDIDATES_SHOWN];
  int displayIndices[NUM_CANDIDATES_SHOWN];
  uint8_t displayMap[NUM_CANDIDATES_SHOWN + 1];
  uint8_t selectedQueryChar;
  uint8_t numMenus;
  char query[NUM_CHARS_QUERY + 1];
  char displayPlantNames[NUM_CANDIDATES_SHOWN][NUM_CHARS_NAME];
  bool screenFocus;
  Error& error;
  Plant& activePlant;
};

// Class to store/pass around multiple objects between functions
class Container {
public:
  Container();
  void updatePlantData();
  void pullCachedData(int index, char name[], int& id);
  void getDBPlants();
  void newUserPlant();
  void clearSensorData();
  Plant activePlant;
  Error error;
  Header header;
  SensorReading sensorReading;
  Interface interface;
  int activeMode;
private:
  JsonDocument addSensorReading(JsonDocument sensorDoc, float reading);
};

/*------------------------------------------------------- Standalone Helpers -------------------------------------------------------*/

// Standalone file reader
JsonDocument readSDFile(char fileName[]);

// Standalone file writer
int pushJsonDoc(JsonDocument doc, char fileName[]);

// Standalone helper for querying
uint8_t compareToQuery(char candidate[], char query[]);

// Standalone helper for pulling plant data from DB
int pullPlant(char fileName[], int id, JsonDocument& doc);

// Standalone helper for centering text horizontally
int horizontalCenterText(char text[], int bufferLen, int fontSize);

/*---------------------------------------------------------- enumerables -----------------------------------------------------------*/

// For tracking states
enum StateTracker {
  startupMode,
  displayMode,
  sensingMode,
  triggerMode,
  shutdownMode,
  errorMode
};

// For tracking current menu
enum Menu {
  noMenu,
  mainMenu,
  infoMenu,
  inputMenu,
  selectMenu
};

// For returning/parsing error status from functions
enum ErrorStatus {
  noError,
  displayInit,
  lightSensorInit,
  tempSensorInit,
  moistureSensorInit,
  jsonError,
  fileOperation,
  SDInit
};

// For iterating through multiple files
enum FileTypes {
  lightFile,
  waterFile,
  humidityFile,
  tempFile,
  datesFile
};

// For iterating/checking threshold evaluations
enum Eval {
  evalUnknown,
  evalLow,
  evalHigh,
  evalOK
};

// For checking light requirements
enum LightValues {
  fullShade = 1,
  partialSun = 2,
  fullSun = 3
};

// For checking water requirements
enum waterValues {
  water = 1,
  wet = 2,
  moist = 3,
  dry = 4
};


#endif
