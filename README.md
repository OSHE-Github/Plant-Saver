# Plant-Saver
## Project Description
This is a project within Michigan Technological University's Open Source Hardware Enterprise (OSHE) focused on providing a tool for indoor plant growth hobbyists and enthusiasts. The Plant-Saver is designed to use data from multiple sensors over days or weeks to assess the suitability of an environment for plant growth. 

The current stage of the project is focused on providing recommendations for improvements to four main factors:
* Water level
* Ambient light level
* Temperature
* Relative humidity

These values are measured using three sensors, two of which are at present located on premade breakout boards. These are the [LTR390](https://www.adafruit.com/product/4831?srsltid=AfmBOoqU5iz8eunMPtCgOZjQv9Xd9VcFfiFB22g8B0UnARdBg-10L_Zb) for ambient light and [AHT20](https://www.adafruit.com/product/4566?srsltid=AfmBOoqB3MfBNdqUE-nxQabkxx0p2WcYAA2l8huIZYk5sai5YeIe0qZl) for temperature and humidity. A capacitive soil sensor such as [this one](https://www.amazon.com/Stemedu-Capacitive-Corrosion-Resistant-Electronic/dp/B0BTHL6M19/ref=sr_1_16?dib=eyJ2IjoiMSJ9.CatMvf0Y8zuFXnifQkoxtoyzxnr0dTRjin4kizkKefxWYe7dKQhMQeNfOIEoMku838ZBSTELCy-yV1O5iF0BEBUiiwh7XnL50mE84VGoKhIKDEL4t4DRgwiMUpLFS0TYha-_nLmbxnhb_toJgTM9vUH5opcPKxvyihWvgCWEASKPDnqrc9PMbQT0UYUkfNTcOGTdrYIC4L3fVzoA97cCg1sK_M5ce1H5Qa8APLBPsUfeiK5XEMrJkweehjqo-Rvlo1LemSDOZoT_31WmuTyUJIYx10by8kh4YatVXFPf12U.AzxNLC5aDzEWhU-sOxfi9ZimVgmziwEPRNp3VOB1Zp0&dib_tag=se&keywords=soil+moisture+sensor&qid=1758037951&sr=8-16) is used to measure soil moisture. An [OLED Display](https://www.adafruit.com/product/938) is used to indicate information to the user. 

The project is designed around the ESP32 microcontroller, primarily due to it having more memory than other popular chips such as the ATmega328P used in the Arduino UNO. The ESP32 also has the built-in capability to transmit data wirelessly, keeping the possibility open for this device to be integrated into a smart home network.

## Files 
The current latest build is located in the Plant_Saver_Fall_2025 directory. This is an Arduino project containing:
1. ***Plant_Saver_Fall_2025.ino*** | The setup, main loop, and state handler functions. This essentially functions as a state machine which manipulates information in a data container object which is passed between functions
2. ***PlantSaverClasses.h*** | A header file containing definitions for classes, enumerables, and standalone helper functions. 
3. ***PlantSaverClasses.cpp*** | A C++ file defining the functionality of methods/standalone functions. This is where the bulk of the code is, since most operations in the state handler functions are done using methods.

These files can be downloaded and copied into an Arduino project to be downloaded to the ESP32.

Additionally, the EmptyFS zip file is needed to construct the file system which the Plant-Saver uses to store data and initialize certain settings. After downloading it, the contents can be extracted directly to the micro SD which will be used to store data. Do not create any new folders to extract the contents to, as this will prevent the device from accessing the files. 

After copying the filesystem onto a micro SD, the only file which may need editing is ***header.txt***. The following fields can be used to configure the device:
* The *date* field sets the time used by the ESP32's internal RTC clock, which in turn generates timestamps for each measurement. The format of this timestamp roughly follows ISO 8601 with the millisecond count omitted. When editing this field, do not remove the enclosing quotes or change the format.
* The *lightThreshold*, *tempThreshold*, *waterThreshold*, and *humidityThreshold* fields can be edited to set certain environmental thresholds. When a sensor reading is taken, if any values are above the selected thresholds the device will output a two-second pulse on an external trigger pin. Keep in mind that these are integer values, and thus should not contain a decimal point.

## Attributions
 * This project makes use of data provided by the Permapeople agricultural database, located at [permapeople.org](https://permapeople.org/). The database and related content is licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by/4.0/). Only slight formatting modifications were made to the data received via their API to allow for integration with this project.

 * Thanks to Dr. Shane Oberloier for his advisorship, Michigan Technological University for funding and use of facilities, and the rest of the OSHE team for their direct and indirect support.
