# Plant-Saver
## Project Description
This is a project within Michigan Technological University's Open Source Hardware Enterprise (OSHE) focused on providing a tool for indoor plant growth hobbyists and enthusiasts. The Plant-Saver collects environmental data from its surroundings over several days to:
 * Recommend adjustments to the environment of a selected plant
 * Recommend plants which would be suitable for an environment

These values are gathered using three sensor types. The first is a set of solar cells placed at four separate angles to measure light intensity and direction. The [DHT20](https://www.digikey.com/en/products/detail/adafruit-industries-llc/5183/15204087) is used to measure both temperature and relative humidity. A capacitive soil sensor such as [this one](https://www.amazon.com/Stemedu-Capacitive-Corrosion-Resistant-Electronic/dp/B0BTHL6M19/ref=sr_1_16?dib=eyJ2IjoiMSJ9.CatMvf0Y8zuFXnifQkoxtoyzxnr0dTRjin4kizkKefxWYe7dKQhMQeNfOIEoMku838ZBSTELCy-yV1O5iF0BEBUiiwh7XnL50mE84VGoKhIKDEL4t4DRgwiMUpLFS0TYha-_nLmbxnhb_toJgTM9vUH5opcPKxvyihWvgCWEASKPDnqrc9PMbQT0UYUkfNTcOGTdrYIC4L3fVzoA97cCg1sK_M5ce1H5Qa8APLBPsUfeiK5XEMrJkweehjqo-Rvlo1LemSDOZoT_31WmuTyUJIYx10by8kh4YatVXFPf12U.AzxNLC5aDzEWhU-sOxfi9ZimVgmziwEPRNp3VOB1Zp0&dib_tag=se&keywords=soil+moisture+sensor&qid=1758037951&sr=8-16) takes soil moisture readings. An [OLED Display](https://www.adafruit.com/product/938) is used to indicate information to the user. 

The project is designed around the ESP32 microcontroller, primarily due to it having more memory than other popular chips such as the ATmega328P used in the Arduino UNO. The ESP32 also has the built-in capability to transmit data wirelessly, which is used to expand the UI to a web app.

## Files 
The current latest build is located in the Plant_Saver_Fall_2026 directory. This folder contains the various components of the project, including:
1. ***Code*** | The Arduino project containing the code that runs the Plant-Saver. This is divided further into a main state machine file, a library of classes used in that state machine, and a library for managing the server.
2. ***Plant_Saver_Kicad*** | A KiCad 9.0 project containing project schematic and PCB files. There are currently two versions of this, and the discrepancies between them will be touched on later.
3. ***EmptyPlantSaverFs*** | A zip file which can be unpacked to initialize the Micro-SD used in the project. Do not create any additional folders to extract the contents to. They should be placed directly in the root directory of the Micro-SD.
4. ***plantSaverParser*** | A python file used to create the plant database. Calls are made to the Permapeople API until all available plants are received. Data which cannot be used by the Plant-Saver is discarded, and the remaining information is reformatted for easier use in the project.

## A Note on Electrical Designs
As was previously mentioned, there are currently two versions of the KiCad project under this repository. The first, Plant_Saver_Kicad, contains the project that our team built as-is, kept in the repository for posterity. After this PCB was populated and tested, several errors were noted and rectified. In light of these changes and those still ongoing to make improvements to the project, we thought it best to create a copy of the project (Plant_Saver_Kicad_V2). This copy contains design changes which **have not been tested** and may not be complete. As such, it is advised to use it as a reference rather than an absolute guide.

## Attributions
 * This project makes use of data provided by the Permapeople agricultural database, located at [permapeople.org](https://permapeople.org/). The database and related content is licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by/4.0/). Only slight formatting modifications were made to the data received via their API to allow for integration with this project.

 * Thanks to Dr. Shane Oberloier for his advisorship, Michigan Technological University for funding and use of facilities, and the rest of the OSHE team for their direct and indirect support.
