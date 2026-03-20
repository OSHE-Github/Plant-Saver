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
// TODO - decrease max sensor readings
#define MAX_SENSOR_READINGS 200  // # of sensor readings allowed in FIFO
#define NUM_CHARS_NAME 50
#define QUERY_DIFF_THRESH 3 // Diff. between query and candidate must be < this #
#define NUM_CHARS_QUERY 5
#define MAX_DIGITS_ID 6

// Device params
#define INTEGRATION_TIME 0.25  // LTR390 integration time
#define LTR390_GAIN 3          // Gain of the LTR390
#define NUM_SOLAR_PANELS 4

// Display params
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // OLED Reset pin # (or -1 if sharing Arduino reset pin)
#define NUM_CANDIDATES_SHOWN 3 //im setting a random number here so i can test on my end --brandon
#define X_SCALE 6 // X (width) scaling for characters
#define Y_SCALE 8 // Y (height) scaling for characters
#define REC_WIDTH 52 // This stuff vvv for recommendation display
#define REC_HEIGHT 11
#define MARKER_WIDTH 5
#define MARKER_HEIGHT 8
#define THRESH_MARKER_HEIGHT 3

// Thresholds
#define LIGHT_MIN 0
#define LIGHT_MAX 107527
#define TEMP_MIN 26
#define TEMP_MAX 100
#define WATER_MIN 0
#define WATER_MAX 4095
#define HUMIDITY_MIN 0
#define HUMIDITY_MAX 100

// Paths
#define PLANT_DB_PATH "/plantDBTest.txt"
#define HEADER_PATH "/header.txt"
#define PLANT_PATH "/Plant/plant.txt"
#define HUMIDITY_PATH "/Plant/humidity.txt"
#define LIGHT_PATH "/Plant/light.txt"
#define TEMP_PATH "/Plant/temp.txt"
#define WATER_PATH "/Plant/water.txt"
#define TMP_GATHER_PATH "/Tmp/gather.txt"
#define TMP_SORT_PATH "/Tmp/sort.txt"
#define LIGHT_QUADRANT1_PATH "/Plant/light1.txt"
#define LIGHT_QUADRANT2_PATH "/Plant/light2.txt"
#define LIGHT_QUADRANT3_PATH "/Plant/light3.txt"
#define LIGHT_QUADRANT4_PATH "/Plant/light4.txt"

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
  float getAvgReading(JsonDocument& sensorDoc);
  void pullPlant();
  void pushPlant();
  void tempCheck(int threshold[2]);
  void waterCheck(int threshold[2]);
  void lightCheck(int threshold[2]);
  void humidityCheck(int threshold[2]);
  void calcAvgLight();
  int id;  // ID within the plant database
  char commonName[NUM_CHARS_NAME];
  char scientificName[NUM_CHARS_NAME];
  int lightReq[2];
  int waterReq[2];
  int hardiness[2];
  float avgLight;              // Overall average light reading
  float avgLightQuadrants[4];  // Quadrant-wise averages, start top and go CCW
  float avgWater;
  float avgHumidity;
  float avgTemp;
  // These variables ARE NOT stored:
  bool plantPulled;
  Error& error;
};

// Data associated with an instanced multi-sensor reading
class SensorReading {
public:
  SensorReading(Error& errorRef);
  void addTemp(float reading, Plant& plant);
  void addWater(int reading, Plant& plant);
  void addHumidity(float reading, Plant& plant);
  void addLight(int reading1, int reading2, int reading3, int reading4, Plant& plant);
  template<typename readingType>
  void parseNewData(readingType newReading, float& avgReading, char fileName[MAX_CHARS_FILENAME], Plant& plant);
  template<typename readingType>
  void addSensorReading(JsonDocument& sensorDoc, readingType reading);
  void clearFile(char fileName[MAX_CHARS_FILENAME]);
  void newFile(char fileName[MAX_CHARS_FILENAME]);
  float tempReading;
  int waterReading;
  float humidityReading;
  float lightReading;    // TODO: old light reading, remove when possible
  int lightReadings[4];  // Quadrant-wise light readings (0 = top, CCW order)
  Error& error;
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
  void displayDataMenu();
  void displayInfoMenu();
  void displayInputMenu();
  void displayErrorScreen();
  void displayLoadingScreen();
  void displayRecommendation(int recInd, int min, int max, int threshold[2], float val);
  void indexQueryPos(bool upDir);
  void displaySelectMenu();
  void scrollSelectDown();
  void scrollSelectUp();
  void queryDBPlants();
  void pullCachedData(int index, char name[], int& id);
  void nextScreen(bool plantSelected);
  void displayOff();
  char displayRecommendation(int recInd, int min, int max, int lowThresh, int highThresh, int val);
  int selectedPlantIndex;
  int activeMenu;
  int numSelectCandidates;
  int displayPlantIDs[NUM_CANDIDATES_SHOWN];
  int displayIndices[NUM_CANDIDATES_SHOWN];
  uint8_t displayMap[NUM_CANDIDATES_SHOWN + 1];
  uint8_t indexer; // Generic indexing variable
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
  void pullCachedData(int index, char name[], int& id);
  void getDBPlants();
  void newUserPlant();
  Plant activePlant;
  Error error;
  Header header;
  SensorReading sensorReading;
  Interface interface;
  int activeMode;
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

// Standalone helper for creating new files
int newReadingsFile(char fileName[MAX_CHARS_FILENAME]);

// Function to get active plant sensor averages (for web API)
void getActivePlantAverages(float& avgLight, float& avgTemp, float& avgWater, float& avgHumidity);

// Global container pointer for web API access
extern Container* globalContainer;

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
  selectMenu,
  dataMenu
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
