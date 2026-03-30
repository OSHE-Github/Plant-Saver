/*---------------------------------------------------------- Libraries ---------------------------------------------------------*/
#include "Adafruit_LTR390.h"  // Library for LTR390 UV sensor
#include <SPI.h>              // Libraries for SSD1306 OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>     // Library for AHT20 Temperature & Humidity sensor
#include <SD.h>                 // Libraries for reading/writing to/from micro-SD
#include "PlantSaverClasses.h"  // Plant-Saver class/enum definitions
#include <math.h>
#include <ArduinoJson.h>
#include "driver/rtc_io.h"
#include <WiFi.h>               // WiFi for ESP32 Access Point
#include <ESPAsyncWebServer.h>  // Async Web Server
#include "PlantServer.h"

/*---------------------------------------------------------- Macros ---------------------------------------------------------*/

#define SCREEN_WIDTH 128            // OLED display width, in pixels
#define SCREEN_HEIGHT 64            // OLED display height, in pixels
#define OLED_RESET -1               // OLED Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D         // OLED screen I2C address - TODO - 3D
#define WAKE_PIN_BITMASK 201347072  // Pins 12, 14, 26 & 27
#define DISPLAY_TIMEOUT_M 1         // delay before timing out the display in minutes
#define MS_PER_MINUTE 60000         // Milliseconds per minute conversion factor
#define INTEGRATION_TIME 0.25       // LTR390 integration time
#define LTR390_GAIN 3               // Gain of the LTR390
#define TRIG_PULSE_LEN_MS 2000      // Trigger mode pulse length in ms
#define BUTTON_DEBOUNCE 150         // Millisecond debounce
#define REC_INTERVAL 10
#define SCROLL_INTERVAL_1 300         // Millisecond scroll interval
#define SCROLL_INTERVAL_2 100

// Pin Definitions - Prototype
/*
#define V_GATE_PERIPHERAL 2  // Gate control pin of peripheral low-side power MOSFET
#define SELECT_BTN 12        // Select button
#define CHG_SCREEN_BTN 14    // Change screen button
#define UP_BTN 26            // Up button
#define DOWN_BTN 27          // Down btton
#define CAP_SOIL_AOUT 34     // Capacitive soil sensor reading
#define SPI_CS 5             // CS pin for SPI
#define ERROR_IND_PIN 4      // Error indication LED
*/

// Pin Definitions - PCB

#define CAP_SOIL_AOUT 25 // Capacitive soil sensor reading (ADC)
#define UP_BTN 26 // Up button
#define DOWN_BTN 27 // Down button
#define CHG_SCREEN_BTN 14 // Change screen/cycle button
#define SELECT_BTN 12 // Select button
#define SPI_CS 5 // CS pin for SPI (uSD card)
#define V_GATE_PERIPHERAL 17 // Enable power to peripherals (normally open)
#define ERROR_IND_PIN 4


#define SOLAR1 32 // Solar measurements (ADC)
#define SOLAR2 34
#define SOLAR3 35
#define SOLAR4 33
#define SOLAR_CHG_EN 13 // Enable solar charging
#define GPIO_SOLAR_CHG_EN GPIO_NUM_13
#define GPIO_SELECT_BTN GPIO_NUM_12

/*------------------------------------------------------ Global Variables ------------------------------------------------------*/

const uint64_t usPerMinute = 60000000;  // Conversion factor between minutes and microseconds
const uint64_t samplingPeriodM = 1;     // Time between sensor measurements in minutes

/*---------------------------------------------------- Object Instantiation ----------------------------------------------------*/

//Adafruit_LTR390 ltr390 = Adafruit_LTR390();  // Create light sensor object
Adafruit_AHTX0 aht20;                        // create temperature & humidity sensor object

/*----------------------------------------------------------- Setup -------------------------------------------------------------*/

void setup() {
  // Pin modes
  pinMode(V_GATE_PERIPHERAL, OUTPUT);
  pinMode(SELECT_BTN, INPUT_PULLUP);
  pinMode(CHG_SCREEN_BTN, INPUT_PULLUP);
  pinMode(UP_BTN, INPUT_PULLUP);
  pinMode(DOWN_BTN, INPUT_PULLUP);
  pinMode(CAP_SOIL_AOUT, INPUT);
  pinMode(ERROR_IND_PIN, OUTPUT);
  pinMode(SOLAR_CHG_EN, OUTPUT);
  pinMode(SOLAR1, INPUT);
  pinMode(SOLAR2, INPUT);
  pinMode(SOLAR3, INPUT);
  pinMode(SOLAR4, INPUT);
  //Power-Up
  //rtc_gpio_pullup_dis(GPIO_SOLAR_CHG_EN);
  //digitalWrite(SOLAR_CHG_EN, LOW);
  //delay(500);
  digitalWrite(V_GATE_PERIPHERAL, HIGH);  // Power-up peripherals
  delay(500);
  // Start serial monitor
  Serial.begin(115200);
  delay(1000);  // Allow time for serial to initialize
}

/*---------------------------------------------------------- Main Loop ----------------------------------------------------------*/

/*
  Main loop functions as a state machine where each state handler function determines the next state.
  Error occurrence holds the active state to return if the error can be cleared.
*/
void loop() {
  static Container container;
  globalContainer = &container;
  if (container.error.highestPriority) {
    errorModeHandler(container);
  } else {
    switch (container.activeMode) {
      case startupMode:
        startupModeHandler(container);
        break;
      case displayMode:
        displayModeHandler(container);
        break;
      case sensingMode:
        sensingModeHandler(container);
        break;
      case shutdownMode:
        shutdownModeHandler(container);
        break;
    }
  }
  delay(10);
}

/*----------------------------------------------------- Class Definitions --------------------------------------------------------*/



/*---------------------------------------------------- Function Definitions ------------------------------------------------------*/


/*
  Initialize all peripherals, then pull header data.
  If a user plant had been selected previously, that data is also pulled in. 
*/
void startupModeHandler(Container &container) {
  bool initFailed = 0; // Flag to track an initialization failure

  delay(1000); // brief delay to prevent weird transient effects w/ transistors

  Serial.println("Starting up");
  // SSD1306 Initialization
  if (!container.interface.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    container.error.addError(displayInit);
    initFailed = 1;
  } else {
    container.error.clearError(displayInit);
  }

  // LTR390 Initialization - Not needed for PCB
  /*
  if (!ltr390.begin()) {
    container.error.addError(lightSensorInit);
    delay(500);
    initFailed = 1;
  } else {
    ltr390.setMode(LTR390_MODE_ALS);                // Ambient lighting mode
    ltr390.setGain(LTR390_GAIN_3);                  // Gain of 3
    ltr390.setResolution(LTR390_RESOLUTION_16BIT);  // 16-bit resolution
    ltr390.configInterrupt(0, LTR390_MODE_UVS, 0);  // Disable interrrupts from the device
    container.error.clearError(lightSensorInit);
  }
  */

  // AHT20 initialization
  
  if (!aht20.begin()) {
    container.error.addError(tempSensorInit);
    initFailed = 1;
  } else {
    container.error.clearError(tempSensorInit);
  }
  
  // Micro-SD card initialization & initial data reading
  if (!SD.begin(SPI_CS)) {
    initFailed = 1;
    container.error.addError(SDInit);
  } else {
    container.error.clearError(SDInit);
    if (!container.header.headerPulled) {
      container.header.pullHeader();
      container.interface.numRecCandidates = container.header.numRecCandidates;
      container.sensorReading.pullParams();
    }
    if (!container.activePlant.plantPulled && container.header.headerPulled && container.header.plantSelected) {
      container.activePlant.pullPlant();  // Grab the active user plant only if it exists
    }
  }

  if (initFailed == 1) {  // One or more peripherals failed to initialize
    container.activeMode = startupMode;
  } else {                                                               // All peripherals initialized
    esp_sleep_wakeup_cause_t wakeSource = esp_sleep_get_wakeup_cause();  // Determine what woke the ESP32
    switch (wakeSource) {                                                // User mode (displayMode) if button wakeup, otherwise move to sensing steps
      case ESP_SLEEP_WAKEUP_EXT0:
        container.activeMode = displayMode;
        break;
      case ESP_SLEEP_WAKEUP_TIMER:
        container.activeMode = sensingMode;
        break;
      default:
        container.activeMode = displayMode;
    }
  }

  if (container.header.headerPulled && !container.header.plantSelected) {  // Automatically switch to display mode if no plant selected yet
    container.activeMode = displayMode;
  }
}

/*
  Handle user inputs and display switching. Button presses are detected
  and associated functions are executed once per input with a preset debounce. 
  Inactivity is tracked, and after a set period the device is set into shutdown mode.
*/
void displayModeHandler(Container &container) {
  static int scrollInt = SCROLL_INTERVAL_1;
  static bool serverInit = 0;
  static bool chgOns = 0;
  static bool selOns = 0;  // One-shot bits for triggering events from button inputs
  static bool upOns = 0;
  static bool downOns = 0;
  static unsigned long downDb = 0; // Debounces to prevent jitter
  static unsigned long upDb = 0;
  static unsigned long selDb = 0;
  static unsigned long chgDb = 0;
  static unsigned long startTime = millis();  // Timekeeping for inactivity watchdog
  unsigned long currentTime = millis();
  static uint8_t prevMenu = noMenu;

  if (serverInit == false) {
    Serial.println("display mode");
    container.interface.pullWebRecs();
    initializePlantServer();
    serverInit = true;
  }

  if (container.interface.activeMenu == noMenu) {
    container.interface.displayMainMenu();
  }
  // Change screen button
  if (!digitalRead(CHG_SCREEN_BTN) && chgOns == 0) {
    chgOns = 1;
    chgDb = startTime = millis();
    if (container.interface.activeMenu == inputMenu && container.interface.screenFocus) {
      container.interface.indexer = (container.interface.indexer + 1) % NUM_CHARS_QUERY;
      container.interface.displayInputMenu();
    } else if (container.interface.activeMenu != inputMenu || !container.interface.screenFocus) {
      container.interface.nextScreen(container.header.plantSelected);
    }
  } else if (digitalRead(CHG_SCREEN_BTN) && chgOns && millis() - chgDb > BUTTON_DEBOUNCE) {
    chgOns = 0;
  }  else if (!digitalRead(CHG_SCREEN_BTN) && chgOns && millis() - chgDb > 700) {
    if (container.interface.activeMenu == inputMenu && container.interface.screenFocus) {
      container.interface.screenFocus = false;
      container.interface.displayInputMenu();
    }
  }

  // Up button
  if (!digitalRead(UP_BTN) && upOns == 0) {
    upOns = 1;
    upDb = startTime = millis();
    if (container.interface.activeMenu == mainMenu && container.interface.screenFocus) {
      container.interface.indexer = container.interface.indexer > 0 ? container.interface.indexer - 1 : 3;
      container.interface.displayMainMenu();
    }
    else if (container.interface.activeMenu == inputMenu && container.interface.screenFocus) {
      container.interface.indexQueryPos(1);
      container.interface.displayInputMenu();
    } else if (container.interface.activeMenu == selectMenu) {
      container.interface.scrollSelectUp();
      container.interface.displaySelectMenu();
    }
  } else if (digitalRead(UP_BTN) && upOns && millis() - upDb > BUTTON_DEBOUNCE) {
    upOns = 0;
    scrollInt = SCROLL_INTERVAL_1;
  } else if (!digitalRead(UP_BTN) && upOns && millis() - upDb > scrollInt) {
    upOns = 0;
    scrollInt = SCROLL_INTERVAL_2;
  }

  // Down button
  if (!digitalRead(DOWN_BTN) && downOns == 0) {
    downOns = 1;
    downDb = startTime = millis();
    if (container.interface.activeMenu == mainMenu && container.interface.screenFocus) {
      container.interface.indexer = (container.interface.indexer + 1) % 4;
      container.interface.displayMainMenu();
    }
    if (container.interface.activeMenu == inputMenu && container.interface.screenFocus) {
      container.interface.indexQueryPos(0);
      container.interface.displayInputMenu();
    } else if (container.interface.activeMenu == selectMenu) {
      container.interface.scrollSelectDown();
      container.interface.displaySelectMenu();
    }
  } else if (digitalRead(DOWN_BTN) && downOns && millis() - downDb > BUTTON_DEBOUNCE) {
    downOns = 0;
    scrollInt = SCROLL_INTERVAL_1;
  }  else if (!digitalRead(DOWN_BTN) && downOns && millis() - downDb > scrollInt) {
    downOns = 0;
    scrollInt = SCROLL_INTERVAL_2;
  }

  // Select button
  if (!digitalRead(SELECT_BTN) && selOns == 0) {
    selOns = 1;
    selDb = startTime = millis();
    switch (container.interface.activeMenu) {
      case inputMenu:
        if (!container.interface.screenFocus) {
          container.interface.screenFocus = 1;
          container.interface.indexer = 0;
          container.interface.displayInputMenu();
        } else {
          container.interface.screenFocus = 0;
          container.interface.displayLoadingScreen();
          container.interface.queryDBPlants();
          container.interface.numSelectCandidates = container.interface.numSearchCandidates;
          snprintf(container.interface.selectFileSource, MAX_CHARS_FILENAME, "%s", TMP_SORT_PATH);
          container.interface.initSelectMenu(TMP_SORT_PATH);
          container.interface.indexer = 0;
          container.interface.displaySelectMenu();
        }
        break;
      case selectMenu:
        if (container.interface.numSelectCandidates > 0) {
          container.interface.displayLoadingScreen();
          container.newUserPlant();
          container.interface.displayMainMenu();
        }
        break;
      case mainMenu:
        if (!container.interface.screenFocus) {
          container.interface.screenFocus = 1;
          container.interface.indexer = 0;
          container.interface.displayMainMenu();
        } else {
          container.interface.screenFocus = 0;
          container.interface.displayDataMenu();
        }
        break;
      case recMenu:
        container.interface.indexer = 0;
        container.interface.numSelectCandidates = container.interface.numRecCandidates;
        snprintf(container.interface.selectFileSource, MAX_CHARS_FILENAME, "%s", TMP_REC_PATH);
        container.interface.initSelectMenu(TMP_REC_PATH);
        container.interface.displaySelectMenu();
        break;
    }
  } else if (digitalRead(SELECT_BTN) && selOns && millis() - selDb > BUTTON_DEBOUNCE) {
    selOns = 0;
  }

  // Inactivity watchdog timer
  if (currentTime - startTime > (DISPLAY_TIMEOUT_M * MS_PER_MINUTE)) {  // go into deep sleep after a period of inactivity
    Serial.println(F("shutting down..."));
    container.activeMode = shutdownMode;
  }
}

/*
 Take readings from each sensor to construct a sensorReadings object, then update active user plant averages
 as well as each sensor readings file
*/
void sensingModeHandler(Container &container) {
  static bool lightRead = false;
  static bool humidityRead = false;
  static bool tempRead = false;
  static bool waterRead = false;
  static bool paramsPulled = false;

  if (paramsPulled == false) {
    container.sensorReading.pullParams();
    paramsPulled = true;
  }

  // Get data from each device
  // TODO: Replace w/ actual light readings
  if (lightRead == false) {
    //float light1 = (0.6 * ltr390.readALS()) / (LTR390_GAIN * INTEGRATION_TIME) + 100000;
    //float light2 = light1 + 10;
    //float light3 = light2 + 10;
    //float light4 = light3 + 10;
    float light1 = analogRead(SOLAR1);
    float light2 = analogRead(SOLAR2);
    float light3 = analogRead(SOLAR3);
    float light4 = analogRead(SOLAR4);
    container.sensorReading.addLight(light1, light2, light3, light4, container.activePlant);
    lightRead = true;
  }

  if (humidityRead == false || tempRead == false) {
    sensors_event_t humidity, temp;  // AHT20
    aht20.getEvent(&humidity, &temp);
    container.sensorReading.addHumidity(humidity.relative_humidity, container.activePlant);
    container.sensorReading.addTemp(temp.temperature, container.activePlant);
    humidityRead = true;
    tempRead = true;
  }

  if (waterRead == false) {
    container.sensorReading.addWater(analogRead(CAP_SOIL_AOUT), container.activePlant);
    waterRead = true;
  }

  if (lightRead == true && humidityRead == true && tempRead == true && waterRead == true) {
    container.header.numReadings++;
    if (container.header.numReadings >= REC_INTERVAL) {
      container.header.numReadings = 0;
      container.gatherRecCandidates();
    }
    container.activeMode = shutdownMode;
  }
}

/*
  Push data to storage, finish any remaining housekeeping
  Then, setup wake sources and put device into sleep mode
*/
void shutdownModeHandler(Container &container) {
  digitalWrite(ERROR_IND_PIN, HIGH);
  delay(1000);
  digitalWrite(ERROR_IND_PIN, LOW);
  Serial.println("Shutting down");
  container.header.pushHeader();
  container.activePlant.pushPlant();
  SD.remove(TMP_SORT_PATH);
  if (container.newPlant) {
    SD.remove(TMP_REC_PATH);
  }
  // Set ESP32 into deep sleep mode
  container.interface.displayOff();
  digitalWrite(V_GATE_PERIPHERAL, LOW);  // Shut down peripherals
  rtc_gpio_pullup_en(GPIO_SELECT_BTN);
  //rtc_gpio_pullup_en(GPIO_SOLAR_CHG_EN);
  rtc_gpio_pulldown_dis(GPIO_SELECT_BTN);
  //rtc_gpio_pulldown_dis(GPIO_SOLAR_CHG_EN);
  esp_sleep_enable_ext0_wakeup(GPIO_SELECT_BTN, 0);
  uint64_t sleep_time = (samplingPeriodM * usPerMinute);
  esp_sleep_enable_timer_wakeup(sleep_time);
  esp_deep_sleep_start();
}

/*
 Indicate current error via the LED output. Number of LED pulses in one sequence matches the error code.
 Re-check initialization errors periodically to clear automatically
*/
void errorModeHandler(Container &container) {
  static unsigned long startTime = millis();
  static unsigned long displayStartTime = millis();
  unsigned long currentTime = millis();
  if (currentTime - startTime > 500) {  // Re-check existing errors
    switch (container.error.highestPriority) {
      case displayInit:
        if (container.interface.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
          container.error.clearError(displayInit);
        }
        /*
      case lightSensorInit:
        if (ltr390.begin()) {
          container.error.clearError(lightSensorInit);
        }
        break;
        */
      case tempSensorInit:
        if (aht20.begin()) {
          container.error.clearError(tempSensorInit);
        }
        break;
      case SDInit:
        if (SD.begin(SPI_CS)) {
          container.error.clearError(SDInit);
        }
        break;
    }
    startTime = currentTime;
  }
  if (currentTime - displayStartTime > 2000) {
    container.error.indicateError();
    container.interface.displayErrorScreen();
  }
}