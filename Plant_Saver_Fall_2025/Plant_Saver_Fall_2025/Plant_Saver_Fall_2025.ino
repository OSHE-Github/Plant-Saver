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

/*---------------------------------------------------------- Macros ---------------------------------------------------------*/

#define SCREEN_WIDTH 128            // OLED display width, in pixels
#define SCREEN_HEIGHT 64            // OLED display height, in pixels
#define OLED_RESET -1               // OLED Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D         // OLED screen I2C address
#define WAKE_PIN_BITMASK 201347072  // Pins 12, 14, 26 & 27
#define DISPLAY_TIMEOUT_M 1         // delay before timing out the display in minutes
#define MS_PER_MINUTE 60000         // Milliseconds per minute conversion factor
#define INTEGRATION_TIME 0.25       // LTR390 integration time
#define LTR390_GAIN 3               // Gain of the LTR390
#define TRIG_PULSE_LEN_MS 2000      // Trigger mode pulse length in ms

// Pin Definitions
#define V_GATE_PERIPHERAL 2  // Gate control pin of peripheral low-side power MOSFET
#define SELECT_BTN 12        // Select button
#define CHG_SCREEN_BTN 14    // Change screen button
#define UP_BTN 26            // Up button
#define DOWN_BTN 27          // Down btton
#define CAP_SOIL_AOUT 34     // Capacitive soil sensor reading
#define SPI_CS 5             // CS pin for SPI
#define TRIG_OUTPUT_PIN 32   // External trigger output pin
#define ERROR_IND_PIN 4      // Error indication LED

/*------------------------------------------------------ Global Variables ------------------------------------------------------*/

const uint64_t usPerMinute = 60000000;  // Conversion factor between minutes and microseconds
const uint64_t samplingPeriodM = 1;     // Time between sensor measurements in minutes
RTC_DATA_ATTR bool powerUpFlag = 0;     // used for initialization after a power-up (primarily timekeeping)

/*---------------------------------------------------- Object Instantiation ----------------------------------------------------*/

Adafruit_LTR390 ltr390 = Adafruit_LTR390();  // Create light sensor object
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
  pinMode(TRIG_OUTPUT_PIN, OUTPUT);
  pinMode(ERROR_IND_PIN, OUTPUT);
  // Start serial monitor
  Serial.begin(115200);
  delay(2000);  // Allow time for serial to initialize
}

/*---------------------------------------------------------- Main Loop ----------------------------------------------------------*/

/*
  Main loop functions as a state machine where each state handler function determines the next state.
  Error occurrence holds the active state to return if the error can be cleared.
*/
void loop() {
  static Container container;
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
      case triggerMode:
        triggerModeHandler(container);
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
  Initialize all peripherals, then pull header data. If not already set, date is pulled from timestamp in header
  If a user plant had been selected previously, that data is also pulled in. 
*/
void startupModeHandler(Container &container) {
  bool initFailed = 0;  // Flag to track an initialization failure

  digitalWrite(V_GATE_PERIPHERAL, HIGH);  // Power-up peripherals
  digitalWrite(ERROR_IND_PIN, LOW);       // Reset error indicator

  // SSD1306 Initialization
  if (!container.interface.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    container.error.addError(displayInit);
    initFailed = 1;
  } else {
    container.error.clearError(displayInit);
  }

  // LTR390 Initialization
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
    if (!container.headerPulled) {
      container.pullHeader();
    }
    if (!container.plantPulled && container.headerPulled && container.header.activePlantID != 0) {
      container.pullPlant();  // Grab the active user plant only if it exists
    }
  }

  // Power-up initialization
  if (!powerUpFlag) {
    char fallBackTimeStr[] = "2025-11-01 00:00:00";  // Default time, used only if the header timestamp cannot be found
    if (container.headerPulled && container.header.date[0] != '\0') {
      if (!setTimeFromTimeStr(container.header.date)) {
        setTimeFromTimeStr(fallBackTimeStr);
      }
    } else {
      setTimeFromTimeStr(fallBackTimeStr);
    }
    powerUpFlag = 1;
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

  if (container.headerPulled && container.header.activePlantID == 0) {  // Automatically switch to display mode if no plant selected yet
    container.activeMode = displayMode;
  }
}

/*
  Pull 10 plants to display and show main menu upon first execution. Button presses are detected
  and associated functions are executed once per input. Inactivity is tracked, and after a set period
  the device is set into shutdown mode.
*/
void displayModeHandler(Container &container) {
  static bool chgOns = 0;
  static bool selOns = 0;  // One-shot bits for triggering events from button inputs
  static bool upOns = 0;
  static bool downOns = 0;
  static unsigned long startTime = millis();  // Timekeeping for inactivity watchdog
  unsigned long currentTime = millis();
  if (!container.dbPlantsPulled) {
    container.getDBPlants();
  }
  if (container.interface.activeMenu == noMenu) {
    container.activePlant.checkThresholds();
    container.interface.displayMainMenu(container.activePlant);
  }
  // Change screen button
  if (!digitalRead(CHG_SCREEN_BTN) && chgOns == 0) {
    chgOns = 1;
    startTime = millis();
    container.interface.nextScreen(container.activePlant, container.plants[container.interface.selectedPlantIndex].commonName);
  } else if (digitalRead(CHG_SCREEN_BTN) && chgOns == 1) {
    chgOns = 0;
  }
  // Up button
  if (!digitalRead(UP_BTN) && upOns == 0) {
    upOns = 1;
    startTime = millis();
    if (container.interface.activeMenu == selectMenu) {
      container.interface.selectedPlantIndex = (container.interface.selectedPlantIndex > 0) ? container.interface.selectedPlantIndex - 1 : (NUM_DISPLAY_PLANTS - 1);
      container.interface.displaySelectMenu(container.plants[container.interface.selectedPlantIndex].commonName);
    }
  } else if (digitalRead(UP_BTN) && upOns == 1) {
    upOns = 0;
  }
  // Down button
  if (!digitalRead(DOWN_BTN) && downOns == 0) {
    downOns = 1;
    startTime = millis();
    if (container.interface.activeMenu == selectMenu) {
      container.interface.selectedPlantIndex = (container.interface.selectedPlantIndex < (NUM_DISPLAY_PLANTS - 1)) ? container.interface.selectedPlantIndex + 1 : 0;
      container.interface.displaySelectMenu(container.plants[container.interface.selectedPlantIndex].commonName);
    }
  } else if (digitalRead(DOWN_BTN) && downOns == 1) {
    downOns = 0;
  }
  // Select button
  if (!digitalRead(SELECT_BTN) && selOns == 0) {
    selOns = 1;
    startTime = millis();
    if (container.interface.activeMenu == selectMenu) {
      container.newUserPlant(1);
      container.activePlant.checkThresholds();
    }
  } else if (digitalRead(SELECT_BTN) && selOns == 1) {
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
  static bool lightRead = 0;
  static bool humidityRead = 0;
  static bool tempRead = 0;
  static bool waterRead = 0;

  // Get data from each device
  if (lightRead == 0) {
    container.sensorReading.lightReading = (0.6 * ltr390.readALS()) / (LTR390_GAIN * INTEGRATION_TIME);  // Lux = 0.6*ALS_DATA/(Gain*integration time(ms))
    lightRead = 1;
  }

  if (humidityRead == 0 || tempRead == 0) {
    sensors_event_t humidity, temp;  // AHT20
    aht20.getEvent(&humidity, &temp);
    container.sensorReading.humidityReading = humidity.relative_humidity;
    container.sensorReading.tempReading = temp.temperature * 1.8 + 32;
    humidityRead = 1;
    tempRead = 1;
  }

  if (waterRead == 0) {
    container.sensorReading.waterReading = analogRead(CAP_SOIL_AOUT);  // Capacitive soil sensor
    waterRead = 1;
  }

  getTimeStr(container.sensorReading.timeStamp);

  if (lightRead == 1 && humidityRead == 1 && tempRead == 1 && waterRead == 1) {
    container.updatePlantData();
    container.activeMode = triggerMode;
  }
}

/*
 Check user-set trigger thresholds, output a pulse if any are met
*/
void triggerModeHandler(Container &container) {
  if (container.sensorReading.lightReading > container.header.lightThreshold || container.sensorReading.waterReading > container.header.waterThreshold || container.sensorReading.humidityReading > container.header.humidityThreshold || container.sensorReading.tempReading > container.header.tempThreshold) {
    digitalWrite(TRIG_OUTPUT_PIN, HIGH);
    delay(TRIG_PULSE_LEN_MS);
    digitalWrite(TRIG_OUTPUT_PIN, LOW);
  }
  container.activeMode = shutdownMode;
}

void shutdownModeHandler(Container &container) {
  container.pushHeader();
  container.pushPlant();
  // Set ESP32 into deep sleep mode
  container.interface.displayOff();
  digitalWrite(V_GATE_PERIPHERAL, LOW);  // Shut down peripherals
  rtc_gpio_pullup_en(GPIO_NUM_12);
  rtc_gpio_pulldown_dis(GPIO_NUM_12);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0);
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
  unsigned long currentTime = millis();
  if (currentTime - startTime > 500) {  // Re-check existing errors
    switch (container.error.highestPriority) {
      case displayInit:
        if (container.interface.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
          container.error.clearError(displayInit);
        }
      case lightSensorInit:
        if (ltr390.begin()) {
          container.error.clearError(lightSensorInit);
        }
        break;
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
  container.error.indicateError();
}