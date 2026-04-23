# Plant-Saver
## Project Description
Many new gardeners or horticulturists struggle to create a proper environment for plant growth. The Plant-Saver aims to simplify the process and provide proper recommendations for the user. Housed in a convenient form factor, the device will analyze local conditions including temperature, soil moisture, humidity, and lighting. This data is utilized to provide recommendations for compatible species or for improving the care of a specific nearby plant. A simple local UI and accompanying web application ensures this information can be accessed efficiently in an easily digestible format. Environmental data is displayed along with an indication of how it compares to ideal conditions. Displaying the raw data readings while expressing recommended changes in simple terms allows the user to have a better understanding of their plant’s environment and how to alter it for improved growth.

<img src="/Assets/main_product_image.jpg" alt="Picture of the Plant-Saver resting in a pot." width="400"/>

*Main product image*

## Repository Structure
Project information/resources from semester 1 can be found in the folder Plant_Saver_Spring_2025. The newest iteration of the project from semester 2 is located in Plant_Saver_Fall_2026. This includes the following files/directories:
1. `Code`: Contains the arduino project used to program the Plant-Saver.
2. `JHA`: A job hazard analysis performed for the project.
3. `Plant_Saver_Kicad`: The KiCad project used to design the Plant-Saver’s PCB. This is the original version of the electrical design kept for posterity.
4. `Plant_Saver_Kicad_V2`: A KiCad project to which updates are being made to rectify the main issues found in testing of the original electrical design.
5. `Plant_Saver_Onshape`: Contains two STL files needed for printing the housing’s main body and back plate.
6. `Symbols_Footprints`: Symbol and footprint library storage for KiCad projects, separated from either project to be used globally.
7. `EmptyPlantSaverFs`: A zipped version of the Plant-Saver’s filesystem, “zeroed” out to its uninitialized form.
8. `plantSaverParser`: A python script used for pulling data using permapeople’s API and converting it into the database used by the Plant-Saver.

## Semester 1 in Brief
This project is the continuation of a prototyping phase performed in the previous semester (Fall 2025). The goal of that phase was to develop a functional breadboard version of the electronics, primarily for software development and testing purposes. The methodology of that prototype was very similar, but it was functionally limited so as to allow testing in a limited scope. Instead of a database with thousands of plants, a set of 10 were randomly chosen to be able to quickly optimize the formatting. An LTR390 was used in place of solar cells, which was a simpler sensing method but lacked the ability to detect the direction of incoming light and did not allow solar energy to be captured for powering the device. Battery charging was also not included in this prototype. Through developing and testing the device in this rudimentary state, its utility and feasibility were verified and a foundation for the Plant-Saver’s current software and database was developed.

# Methodology in Brief
## General Design overview
Building on the previous semester’s progress, the primary development goals were to:
1. Condense the form factor so as to be reasonably well-suited  for potted plant use and seal the device electronics to resist expected environmental conditions.
2. Expand the database to include all usable plants within the permapeople database.
3. Develop the existing UI to provide better recommendation, display, and search functions.
4. Move the majority of the device electronics to a single PCB to optimize space usage.
5. Integrate the new solar measurement method and battery charging into the device.

These goals were largely met in the current iteration of the project, although there is still room for improvement. Device electronics were condensed down to a single primary board with the DHT20, capacitive soil sensor, display, and other interface elements being connected via cables due to the need for flexible positioning. A 3D-printed enclosure was designed and produced out of PETG filament to house these electronics. This housing is compact, fitting easily into medium to large pots, and features dedicated ports for all externally accessible components which are sealed to prevent water ingress. The UI was expanded and now includes a local OLED display and remotely accessible web interface.

## General Program Execution

<img src="/Assets/Flowchart_S26.png" alt="Flowchart depicting the high-level operation of the Plant-Saver's software." width="800"/>

*High-Level operational flowchart of the Plant-Saver's software*

The normal operational loop that the Plant-Saver undergoes is as follows:
1. The ESP32 is woken by its RTC clock after the sampling period completes, or by a press of the select button.
2. Peripherals are powered on and initialized. This includes the display and all sensors.
3. The  header, parameters file, and active plant (if it exists) are pulled from the micro SD card.
4. Depending on the wake source, the device moves into one of two modes.
 - If the wake source was the select button, the device is moved into display mode. The watchdog timer is started and is reset every time an input is detected. Average readings are checked against a set of threshold values and recommendations are made to the user. After the watchdog timer completes, the device moves into shutdown mode.
 - If the wake source is the timer, the device moves into sensing mode. All sensor readings are taken and stored, the average readings are updated, and if there are enough readings new recommendations are generated.
5. The active plant is pushed back to the micro SD, temporary files are removed, peripheral power is removed, and the ESP32 is put back into deep sleep.

The broad structure of the program is designed around a state machine with individual functions for handling each state. Data is segmented into classes with self-contained methods for management. The main purpose of this is to keep the design as modular as possible such that it can be easily reconfigured.

If at any point during this sequence an error is detected, the error is displayed to the user by the local OLED display (if possible) and the indicator LED by a sequence of flashes. Errors that can be resolved without restarting are checked periodically, and if the source of the error is cleared the device restarts its initialization sequence.

## Sensing
The device utilizes four sensor types: photovoltaic cells, a combined temperature and humidity sensor, and a capacitive soil moisture sensor. The temperature and humidity sensor (DHT20) communicates with the ESP32 over I2C to deliver readings for both values. The capacitive moisture sensor is read directly from one of the microcontroller’s ADC pins. All four solar cells are measured by scaling their open-circuit voltage with a resistor divider network to a 0-3.3V range, the output of which is fed into a voltage-follower op-amp configuration and read by four separate ADC pins. All measurement devices are powered via a peripheral power bus which is only enabled when ESP32 is awake to save power. 

Each time the device wakes from deep sleep via the RTC timer, one measurement is taken from each device (solar panels are measured individually). Each JSON file for storing sensor readings is temporarily pulled into memory one-by-one so that the new reading can be added and the average readings updated. These files operate as rolling buffers where new readings replace the oldest data once the maximum number of readings is reached (default is 48). If a new plant is selected, these files are cleared immediately but the average readings for each environmental variable are held until the next sensor reading is taken.

The main variance in the sensing methodology from the previous semester’s prototype is in the use of solar cells to measure irradiance. Previously, the LTR390 was used for this purpose, but multiple factors led to this being swapped out. The form factor of the sensor meant it would be nearly impossible to hand-solder, and existing breakout boards would have been challenging to expose properly to ambient light. Other methods such as photoresistors were considered, but solar cells were chosen due to the fact that they allow for light energy to also be used for powering the device, making it more suitable for longer-term use.

# Bill of Materials
| # | Component | Qty | Total Cost | Link |
| - | --------- | --- | ---------- | ---- |
| 1 | 3.7V LiPo battery | 1 | $9.99 | [Makerfocus 3.7V 2000mAh](https://www.amazon.com/MakerFocus-Rechargeable-Protection-Insulated-Development/dp/B0DTHG757X?th=1) |
| 2 | 0.1 uF ceramic SMD capacitor | 5 | $0.6 | [C1206C104K5RACTU](https://www.digikey.com/en/products/detail/kemet/C1206C104K5RACTU/411248) |
| 3 | 10 uF ceramic SMD capacitor | 4 | $0.32 | [CL31A106KBHNNNE](https://www.digikey.com/en/products/detail/samsung-electro-mechanics/CL31A106KBHNNNE/3888534) |
| 4 | 1 uF ceramic SMD capacitor | 5 | $0.335 | [CL31B105KAHNNNE](https://www.digikey.com/en/products/detail/samsung-electro-mechanics/CL31B105KAHNNNE/3886755) |
| 5 | 1 nF ceramic SMD capacitor | 1 | $0.11 | [CL31B102KBCNNNC](https://www.digikey.com/en/products/detail/samsung-electro-mechanics/CL31B102KBCNNNC/3886708) |
| 6 | 4.7 uF ceramic SMD capacitor | 1 | $0.16 | [CL31B475KAHNNNE](https://www.digikey.com/en/products/detail/samsung-electro-mechanics/CL31B475KAHNNNE/3886713) |
| 7 | Red through-hole LED | 1 | $0.14 | [LTL-4224](https://www.digikey.com/en/products/detail/liteon/LTL-4224/217584) |
| 8 | Yellow through-hole LED | 1 | $0.16 | [LTL-4253](https://www.digikey.com/en/products/detail/liteon/LTL-4253/214719) | 
| 9 | Green through-hole LED | 1 | $0.14 | [LTL-4231](https://www.digikey.com/en/products/detail/liteon/LTL-4231/670004) |
| 10 | 10-Pin male header, 2.54mm | 2 | $0.48 | [SBH11-PBPC-D05-ST-BK](https://www.digikey.com/en/products/detail/sullins-connector-solutions/SBH11-PBPC-D05-ST-BK/1990062) |
| 11 | 4-Pin JST SH header, 1.00mm | 1 | $0.5 | [SM04B-SRSS-TB](https://www.digikey.com/en/products/detail/jst-sales-america-inc/SM04B-SRSS-TB/926710) |
| 12 | 3-pin JST PH header, 2.00mm | 1 | $0.12 | [S3B-PH-K-S](https://www.digikey.com/en/products/detail/jst-sales-america-inc/S3B-PH-K-S/926627) |
| 13 | 6-pin USB-C port | 1 | $2.5 | [54-00284](https://www.digikey.com/en/products/detail/tensility-international-corp/54-00284/15667380) |
| 14 | 1.25mm Micro-JST style male port | 1 | $0.3 | [530470260](https://www.digikey.com/en/products/detail/molex/0530470260/15622895) | 
| 15 | Micro-SD port | 1 | $1.83 | [DM3D-SF](https://www.digikey.com/en/products/detail/hirose-electric-co-ltd/DM3D-SF/1786510) | 
| 16 | PMOS | 10 | $3.55 | [G09P02L](https://www.digikey.com/en/products/detail/goford-semiconductor/G09P02L/16533523) |
| 17 | PMOS | 2  | $2.380 | [ISP12DP06NMXTSA1](https://www.digikey.com/en/products/detail/infineon-technologies/ISP12DP06NMXTSA1/10058802) |
| 18 | 10k SMD resistor | 11 | $0.132 | [RC1206FR-0710KL](https://www.digikey.com/en/products/detail/yageo/RC1206FR-0710KL/728483) |
| 19 | 100k SMD resistor | 3 | $0.033 | [RC1206FR-07100KL](https://www.digikey.com/en/products/detail/yageo/RC1206FR-07100KL/728492) |
| 20 | 150k SMD resistor | 2 | $0.032 | [RC1206FR-07150KL](https://www.digikey.com/en/products/detail/yageo/RC1206FR-07150KL/728562) |
| 21 | 5.1k SMD resistor | 2 | $0.026 | [RC1206FR-075K1L](https://www.digikey.com/en/products/detail/yageo/RC1206FR-075K1L/728947) |
| 22 | 200k SMD resistor | 4 | $0.04 | [RC1206FR-07200KL](https://www.digikey.com/en/products/detail/yageo/RC1206FR-07200KL/728676) |
| 23 | 10k NTC thermistor | 2 | $1.70 | [NTCLE317E4103SBA](https://www.digikey.com/en/products/detail/vishay-dale/NTCLE317E4103SBA/12331368) |
| 24 | 1k SMD resistor | 1 | $0.1 | [MFR-25FRF52-1K](https://www.digikey.com/en/products/detail/yageo/MFR-25FRF52-1K/14891) |
| 25 | 1k through-hole resistor | 1 | $0.1 | [MFR-25FRF52-1K](https://www.digikey.com/en/products/detail/yageo/MFR-25FRF52-1K/14891) |
| 26 | 5.6k through-hole resistor | 1 | $0.1 | [MFR50SFTE52-5K6](https://www.digikey.com/en/products/detail/yageo/MFR50SFTE52-5K6/9151627) |
| 27 |20k through-hole resistor | 1 | $0.1 | [MFR-25FBF52-20K](https://www.digikey.com/en/products/detail/yageo/MFR-25FBF52-20K/13276) |
| 28 | 220 through-hole resistor | 3 | $0.3 | [CFR-25JR-52-220R](https://www.digikey.com/en/products/detail/yageo/CFR-25JR-52-220R/11958) |
| 29 | 205k through-hole resistor | 5 | $0.195 | [MFR-25FBF52-205K](https://www.digikey.com/en/products/detail/yageo/MFR-25FBF52-205K/13549) |
| 30 | 301k through-hole resistor | 5 | $0.195 | [MFR-25FRF52-301K](https://www.digikey.com/en/products/detail/yageo/MFR-25FRF52-301K/15121) |
| 31 | 3.6k through-hole resistor | 1 | $0.1 | [MFR-25FTE52-3K6](https://www.digikey.com/en/products/detail/yageo/MFR-25FTE52-3K6/9140152) |
| 32 | 22.1k through-hole resistor | 1 | $0.1 | [MFR-25FTE52-3K6](https://www.digikey.com/en/products/detail/yageo/MFR-25FTE52-3K6/9140152) |
| 33 | 71.5k through-hole resistor | 1 | $0.1 | [MFR-25FBF52-71K5](https://www.digikey.com/en/products/detail/yageo/MFR-25FBF52-71K5/13444) |
| 34 | 402k through-hole resistor | 1 | $0.1 | [MFR-25FBF52-402K](https://www.digikey.com/en/products/detail/yageo/MFR-25FBF52-402K/13620) |
| 35 | ~1" solar cell | 4 | $23.52 | [KXOB061K08F-TR](https://www.digikey.com/en/products/detail/anysolar-ltd/KXOB061K08F-TR/13999192) |
| 36 | 6mm pushbutton switch | 2 | $0.48 | [SKHHAJA010](https://www.digikey.com/en/products/detail/alps-alpine/SKHHAJA010/19529121) |
| 37 | SPDT slide switch | 1 | $7.16 | [1101M2S3AQE2](https://www.digikey.com/en/products/detail/c-k/1101M2S3AQE2/67052) |
| 38 | Thermal switch/thermostat | 1 | $1.24 | [SA72SB0](https://www.digikey.com/en/products/detail/bourns-inc/SA72SB0/6830067) |
| 39 | Tall pushbutton switch | 4 | $0.68 | [3-1825910-5](https://www.digikey.com/en/products/detail/te-connectivity-alcoswitch-switches/3-1825910-5/1632530) |
| 40 | Schottky diode | 6 | $4.92 | [CDBA140SL-HF](https://www.digikey.com/en/products/detail/comchip-technology/CDBA140SL-HF/3308122) |
| 41 | ESP32 module | 1 | $5.71 | [ESP32-WROOM-32E-N16](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-WROOM-32E-N16/11613144) |
| 42 | DHT20 temp/humidity sensor | 1 | $4.5 | [5183](https://www.digikey.com/en/products/detail/adafruit-industries-llc/5183/15204087) |
| 43 | DW03C battery protection chip | 1 | $0.1 | [DW03C](https://www.digikey.com/en/products/detail/shenzhen-slkormicro-semicon-co-ltd/DW03C/28142983) |
| 44 | Programmable linear regulator | 1 | $1.89 | [LP38511TJ-ADJ/NOPB](https://www.digikey.com/en/products/detail/texas-instruments/LP38511TJ-ADJ-NOPB/2023099) |
| 45 | Battery charging chip | 2 | $2.48 | [MCP73833-CNI/UN](https://www.digikey.com/en/products/detail/microchip-technology/MCP73833-CNI-UN/1223169) |
| 46 | 4-Circuit op-amp | 1 | $0.8 | [MCP6234-E/P](https://www.digikey.com/en/products/detail/microchip-technology/MCP6234-E-P/857555) |
| 47 | 2-Circuit comparator | 1 | $0.62 | [MCP6547T-I/MS](https://www.digikey.com/en/products/detail/microchip-technology/MCP6547T-I-MS/458408) |
| 48 | 10-Pin IDC cable 2.54mm | 1 | $1.12 | [H3DDS-1006G](https://www.digikey.com/en/products/detail/assmann-wsw-components/H3DDS-1006G/1218627) |
| 49 | 3-Pin JST cable 2.00mm | 1 | $1.05 | [A03KR03KR26E152B](https://www.digikey.com/en/products/detail/jst-sales-america-inc/A03KR03KR26E152B/6708616) |
| 50 | 4-Pin JST SH connector | 1 | $0.95 | [4210](https://www.digikey.com/en/products/detail/adafruit-industries-llc/4210/10230021) |
| 51 | Capacitive soil moisture sensor | 1 | $5.67 | [3PCS capacitive soil moisture sensor](https://www.amazon.com/VKLSVAN-Capacitive-Corrosion-Resistant-Detection/dp/B0DQSCD5CV/ref=sr_1_8?crid=2T3W8GRPEPAZO&dib=eyJ2IjoiMSJ9.VDQmMTvA56NE3sh9LcnMjGEhAQiaewfNTo0PxVwCtq20CpiVuc_oL4U8zqIeN2SI6TsorswAe4DcuEpW3kwDSQAY8mm10fgVrGpoDDBsUGHwlLMTe_0tMgvywFzmPk4HuteChVSr-0URo3wA7EzUY787M0kiv-VemaUegofTH7UwfMTeryxjkYHyxM7BOS9Vw_Nq4K6onMX47QMQvUe-syFdiFBW1ohNYQjR-CtfuROxCKTlcSYXfY_iLdHu-LCjD08GmoRkG9fq-5Pi9IY9zhBk_CT5Y_BQhETmEpO4YTs.eDALyc1xMEA73WswnEQ_UNPWlAWnb9vSysE4Zj-Z4Gc&dib_tag=se&keywords=capacitive%2Bmoisture%2Bsensor&qid=1771442737&sprefix=capacitve%2Bmoisture%2Caps%2C156&sr=8-8&th=1) |
| 52 | 1.3" monochrome OLED display | 1 | $19.95 | [938](https://www.adafruit.com/product/938?srsltid=AfmBOopGujMlQ19b1HjKU9Gz_qpD8YPfFucayyibxzuMLd3uAxlGnX66) |
| 52 | Stripboard/perfboard |  |  |  |
| 53 | PETG filament |  |  |  |
| 54 | RTV silocne sealant |  |  |  |
| 55 | Kapton tape |  |  |  |
| 56 | M2 Screws |  |  |  |
| 52 | M2 nuts |  |  |  |

**Notes on Bill of Materials:**

All items listed are from the “V1” electrical design, constructed in the project’s second semester. With minor modifications this version is functional, but not all features were fully realized. Of particular note is item 40 on this list, the 6 Schottky diodes which are primarily used for reverse blocking, as this part did not function as expected in our testing and should not be used for this project. The current replacement candidate (untested as of yet) is the CRS10I30C from Toshiba Semiconductor, although this does have a different footprint. Also of note are items 12 and 49, which are a header and cable respectively meant for connection of the capacitive moisture sensor. The pitch of this cable’s connector is incorrect for mating with the headers on the sensors used in the Plant-Saver, and so requires modification to be used. These should be replaced with 2.54mm pitch XH or SH-style connectors, such as the 25SH-B-03-TR shrouded 3-pin header (and accompanying compatible cable).

# Tools used
| Component | Use | Link |
| --------- | --- | ---- |
| Soldering station | Populate PCB | [Hako 888](https://www.ifixit.com/products/soldering-station-hakko-fx888d-23by?lnid=CjwKCAjwwJzPBhBREiwAJfHRnct3H0tR7nU-cJzkjclwdoABVduTF6vOiET7RmJEt9MUSL2tfSOEwhoCr4MQAvD_BwE&utm_source=google&utm_medium=cpc&utm_campaign=21364903818&utm_term=&utm_content=&tw_source=google&tw_adid=&tw_campaign=21364903818&gad_source=1&gad_campaignid=21371193557&gbraid=0AAAAACuwhq5AtuEWQldLTa-Vttl-HQ26L&gclid=CjwKCAjwwJzPBhBREiwAJfHRnct3H0tR7nU-cJzkjclwdoABVduTF6vOiET7RmJEt9MUSL2tfSOEwhoCr4MQAvD_BwE) |
| Multimeter | Measure voltage and current for PCB validation | [Klein MM325](https://www.kleintools.com/catalog/multimeters/digital-multimeter-manual-ranging-600v-0) |
| Sealant | Adhesive to affix solar panels and UI stripboard | [Clear Silicone RTV Sealant](https://www.amazon.com/Permatex-80050-Silicone-Adhesive-Sealant/dp/B0002UEPVI/ref=sr_1_3?crid=300SS11OFUJAU&dib=eyJ2IjoiMSJ9.lMHOvPXrJL-erz2RLlp5h9Bj5Gugm79FTFMztWvVJAZTfAEDilmMbuq9QWJcelpgCGfmew0R9XLf_4lIiHwveiQKjniAkaROHKqgHcO30IK5zvZ4GxpMcoj4Q9GvhXKbrDnI2Mgf5uMaYyHFEYBsLIHCok_H4aZiN-ZhkOvPqln6VJJmi2BtlZE4frDc2ngy_OdBQsurh65VRnla6rk2yrjv2fKwx0wdvsTYFyyhU9I.N76MvPLgIPCmebUMwT9mRWcgPc7rcxGBiP9dn3Hc3-U&dib_tag=se&keywords=rtv+silicone+sealant+clear&qid=1776825413&sprefix=rtv+silicone+sealant+clea%2Caps%2C198&sr=8-3) |
| 3D printer | Create housing | [Prusa Core One](https://www.prusa3d.com/product/prusa-core-one/) |
| KiCad | Create PCB and schematic | [KiCad Download](https://www.kicad.org/download/) |
| Onshape | Create housing model | [Onshape](https://www.onshape.com/en/) |
| Arduino IDE | Develop and upload program | [IDE Download](https://www.arduino.cc/en/software/) |
| FTDI breakout | Convert USB data to UART for programming the microcontroller | [USB to UART Bridge](https://www.amazon.com/ALMOCN-Converter-Adapter-Breakout-Compatible/dp/B0FWBLW58D?crid=36AOETKEREG2C&dib=eyJ2IjoiMSJ9.9N_ITnsSrm6AsLlIA8zdMjdRkmCJCPpsct6Gm5-LhFhtt0it2NA1_HJ54N89o4667oOHAKTTrQCjL3_qIoLIPlhIC-bsue3BNtu9NKNOpOQQ6KNpf-FXgDVGdgKhCi-p--L0JEZXr-55ZzsA2iLMd6bad30lvakcnc1YJtWxYh8wqVRl5JQ56RjrKZE9LgGR93lqQOooDiW9QdIABYsWZWFmi3kWKTqgauo1wbiVdig.JjUBWDxI5ogOyk18AHwMYy6pHYDV_K6hNzl7mmBij-w&dib_tag=se&keywords=ftdi+esp32&qid=1776436845&sprefix=ftdi+esp3%2Caps%2C452&sr=8-3) |

# Assembly instructions
## PCB Assembly
The PCB can be assembled in two ways. The most straight forward assembly method is to begin with complex components that require more finesse. This would be the ESP32 Processor and U15/U13 (the battery charge ICs). Almost all components present in the BOM will be used to populate the main PCB pictured below in figure XXX. Hand soldering is acceptable for all components, but the assembly process can be greatly sped up with the use of reflow ovens for the above-mentioned components and other ICs. If the assembler wants to test each individual circuit during assembly to verify operation, the process will look a little different.

<img src="/Assets/PCB_image.png" alt="Screenshot of the Plant-Saver PCB." width="400"/>

*Plant-Saver PCB design*

**USB Power Path:**

When assembling to test each sub circuit it is still acceptable to begin with the fine pitch components. Ensure SW2 is not attached to the PCB to prevent any incorrect voltages from affecting the ESP32. Beginning with the charge and power circuits, the USB module, R7, and R8 can be added to the circuit for usb power. The linear regulator circuit will be added next to connect the USB input to the microcontroller. For this, add diodes U9 and U11 then R16-18, C6-8, and U12. This creates a path to ensure the microcontroller will see its expected input voltage. To validate, observe the USB1 test point to check for 5V and the Bus1 test point for 3.3V.

**USB Battery Charge:**

The USB charge circuit will also need to be installed, this consists of C14, C15, R38, and the previously added U15. This would be a good time to add in the charge logic transistor paths. Using 6 of G09P02L, solder Q5-10 and resistors R13 and R14. Finally, add R15, SW4, C5, and U10. These are safety measures added close to the battery for protection. This will complete the path from the USB input to both the battery pins and the voltage regulator. Ensure the BAT1 test point is receiving proper voltage for the battery being used, this should be around 3.7-4.2 volts in this application. In our testing, the schottky diodes used were not sufficient for this project and had to be replaced. This will require finding independent diodes of similar footprint, modifying the PCB slightly to utilize a diode of a different footprint, or other creative solutions.

**Solar UVLO & Charge:**

With the power path complete, the next circuit to test is the Solar UVLO and charge. During testing use wires to simulate solar voltage inputs, it is much easier than testing with the panels attached. The UVLO circuit is relatively simple to integrate. Add the comparator IC to U16, R39-45, and C16. These components make up the comparator circuit, but will need Q11 and Q12 to pass the input voltage through to the power circuit. Ensure the voltage seen at the drain pin of Q12 is only high when between the input ranges of 4.5 to 5.3 V. Once this circuit is validated, add Q3, Q4, R9, and R10 to connect the UVLO to the solar charge circuit. Solar charge is very simple to integrate, just add C11, C12, and R26 to their positions near U13 to complete. With this, the power circuit is complete.

**Solar Voltage Measurement:**

A final complex circuit to validate is the solar measurement system. This steps down the solar input voltage to be read and used by the microcontroller. All this system needs is the U14 op amp, R30-37, and C13. This system uses 4 op amps to adjust each solar input source. Once this circuit is added, test to see the voltage at VSolar1 is approximately equal to the voltage at Sol1 * 0.6. With this, all of the complex circuits are validated.  The rest of the Plant-Saver PCB is able to be assembled using hand soldering techniques, with a few exceptions detailed below.

**Assembly Warnings:**

The DHT20 is a temperature sensitive device that can be impacted by soldering heat. The adafruit datasheet suggests no more than 5 seconds of contact @ 300 °C. This was avoided by using wire extensions and positioning the sensor towards a vent on the housing. Additionally, J2 is a 10 pin connector to connect a UI board to the main pcb. This board will need to be assembled on a stripboard or a user made PCB. Ensure the positioning of the buttons and LEDs will fit inside the UI board housing by adjusting their positioning in a 3D modeling program. The pinout is pictured below in the below table. Additionally, J7 and J6 are utilized for chip programming purposes only. They do not need to be physically attached to the PCB at all times, only when updating the device’s code.

| Pin | Function |
| --- | -------- |
| 1 | 3.3V | 
| 2 | Charge in progress LED (yellow) |
| 3 | Charge complete LED (green) | 
| 4 | Error LED (red) |
| 5 | Up button | 
| 6 | Down button |
| 7 | Change screen button |
| 8 | Select button |
| 9 | Ground |
| 10 | Not used |

## Code Upload/configuration
To upload this project’s code to the ESP32, start by downloading and installing the Arduino IDE if you have not already. Instructions for doing so can be found here: https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE
Additionally, you will need to download the project code and filesystem. These can be found at the project’s github repository [1] or OSF page [2]. In order to communicate with the ESP32 you will need to first install the CP210x USB to UART bridge driver [3]. Following that, open the Arduino IDE and navigate to “File” > “Preferences” > “Settings”. Add the following link to the “Additional boards manager URLS” field: https://espressif.github.io/arduino-esp32/package_esp32_index.json.

<img src="/Assets/Additional_board_mgr_image.png" alt="Screenshot of the Arduino IDE settings page highlighting the location to add additional boards manager URLs." width="600"/>

*Arduino IDE additional boards manager URLs*

Click “OK” to save this change, then click “File” > “Open…” and select the main project file (Plant_Saver_Spring_2026.ino) in the project folder downloaded from the GitHub repository. As long as the other project files (PlantSaverClasses.h, plantSaverClasses.cpp, plantServer.h, plantServer.cpp) are also in the same folder as this file, they should also be opened automatically. At the top of each file are a list of Macros used within that file. These are names which the compiler automatically substitutes with the associated value when building the project. They are convenient to prevent having to change constant values in multiple locations, since updating the definition will alter all uses of the same macro. In the main file, the most pertinent value is DISPLAY_TIMEOUT_M, which determines the time in minutes before the device returns to sleep mode when no activity is detected. There is also a constant value, samplingPeriodM, which sets the time between samples in minutes. Additionally, all pin definitions for the project are set here should you need to alter them. If so, keep in mind that certain pins on the ESP32 have limited functionality. A good reference for determining which pins to use can be found here: https://lastminuteengineers.com/esp32-pinout-reference/. 

The PlantSaverClasses.h file also contains a number of macros which are less likely to need editing, but may be helpful in implementing simple changes to this project such as storing more sensor readings. It is recommended that you only edit these if you are confident in your ability to understand and debug C/C++ code, as the functionality of this project has not been tested with alternate values for the vast majority of these macros.

There is a line of five holes near the power switch intended for vertical header pins. The order of these pins matches that of common USB to Serial converters, although it may be more convenient to connect these together using wires rather than plugging the converter directly into the board.

<img src="/Assets/FTDI_connections_image.png" alt="Close-up of the bottom edge of the Plant-Saver PCB, highlighting the six connection points for the USB to UART converter." width="600"/>

*USB to UART converter connection points*

The labels on these pins indicate the pins on the converter that should be connected to them. The converter Tx should connect to the pin labeled Tx, Rx to Rx, and Ground to GND. These are the only three pins that need to be connected. After connecting the converter to the board and connecting it to your computer via a USB or USB-C cable, provide power to the microcontroller. To put the controller into programming mode:
1. Press and hold the boot button
2. Press and release the reset button
3. Release the boot button

To upload code to the ESP32, first open the board selection dropdown located to the right of the “debug” button (“Select other board and port”). For the board, select “ESP32 Dev Module”. The Arduino IDE should be able to detect that a device is plugged into one of your computer’s ports automatically, in which case it will show up under the “Ports” tab, and can be selected to use. Otherwise, to double check that your device has been recognized and the drivers are properly installed open the Device Manager application and expand the Ports (COM & LPT) section. If this section is not shown or there is not a device listed under the connected port, it is likely that either the device has not been recognized, the drivers are not installed properly, or some other issue is hindering the connection process (such as a faulty cable). Next to the name of this device will be the word “COM#”, where # is the port number that it is plugged into. This should be the same as the port selected in the Arduino IDE.

<img src="/Assets/Board_selection.png" alt="Screenshot of the Arduino IDE board selection dropdown highlighting the correct selection." width="600"/>

*Arduino IDE board selection dropdown*

Finally, to upload code to the board click the “Upload” button located at the top left of the IDE (a right-facing arrow).

# Characterization Data
## Sensor Calibration
Each sensor on the Plant-Saver should be calibrated/tested, although some do not require it for basic operation. Should you desire to calibrate your own Plant-Saver with values other than the defaults, the methods we used are listed below. A breadboard test setup will be easiest to use for this process, but the Plant-Saver board can be used by reprogramming the microcontroller for each test if needed.

**Soil Moisture:**

1. Spread a sample of potting soil over a wide, flat, oven-safe container such as a baking tray lined with foil. 
2. Place the sample in an oven at approximately 200 degrees Fahrenheit until little to no moisture remains. This took about 6-8 hours in our testing, but the best way to be certain is to weigh the sample periodically. When the weight stops changing it is mostly dry.
3. Find a vessel such as a drinking glass or tin can. Using a kitchen scale, tare the empty container and record the weight. Then transfer the sample and take the weight again, subtracting the empty value to find the total dry sample weight. Record this value.
4. Insert the capacitive sensor into the soil (do  not go past the white line). Wait until the readings stabilize and record the steady-state value.
5. Add approximately 10 percent of the dry soil weight in water to the sample. Mix thoroughly. If the water is not dispersed fully, wait until it is relatively well distributed.
6. Repeat steps 4 and 5. We collected up to 170% total added weight in data, as at this point the water began to pool and become poorly distributed, invalidating results following this level of saturation.
7. Perform a linear regression using a tool such as Wolfram|Alpha or Desmos. ADC readings are the independent variable (X), and fraction of dry weight (out of 1, ex: 110% = 1.1) are the dependent variable (Y). Record the coefficients (m and b).

**Light:**

Calibrating the solar cells is a twofold process. To translate ADC readings to solar cell voltages, perform the following steps after populating the PCB but before adding the solar cells:

1. Connect an adjustable voltage source such as a bench power supply to any solar cell connection point with the voltage set to 0V.
2. Power the PCB using either the battery or by injecting a constant voltage source into the supply rails.
3. Record the ADC reading on the corresponding microcontroller pin.
4. Increase the voltage by 0.1 V.
5. Repeat 3 and 4 up to ~5.3 V.
6. Perform a linear regression using a tool such as Wolfram|Alpha or Desmos. ADC readings are the independent variable (X), input voltages are the dependent variable (Y). Record the coefficients (m and b).
	
To translate solar voltages into irradiance values, perform the following steps:
	
1. Using tape or some other method, mark a point on a flat workspace.
2. Place a light source such as a flashlight at a constant height above this point. We used clamps and the side of a desk for this, but any solution which allows the light to be easily adjusted in height will work. 
3. Place a light sensor such as the LTR390 underneath the light source at the marked point and measure the light read.
4. Place a solar cell at the same point and measure the voltage across its positive and negative terminals.
5. Repeat steps 3 and 4 at various heights. Try to get a broad range of light readings, from several hundred Lux up to several tens of thousands.
6. Perform an exponential regression (y =a*c^(x)) using a tool such as Wolfram|Alpha or Desmos. Voltage readings are the independent variable (X), and light readings (in Lux) are the dependent variable (Y). Record the coefficients (a and c).

**Temperature:**

1. Apply a low-intensity heat source to the DHT20.
2. Wait until the reported temperature stabilizes, then record the value (in degrees Celsius).
3. Measure the temperature of the DHT20 using another sensor such as an IR thermometer or thermocouple and record the value (in degrees Celsius).
4. Repeat 1-3, slowly increasing the temperature applied. 10 data points over a span of 20 or so degrees celsius is sufficient. 5. 5. Take care not to apply too much heat, as this may damage the sensor.
6. Perform a linear regression using a tool such as Wolfram|Alpha or Desmos. DHT20 temperature readings are the independent variable (X), and actual temperature readings are the dependent variable (Y). Record the coefficients (m and b).

**Humidity:**

1. Fill a small container such as a bottle cap with a mixture of approximately 75% salt, 25% water. When mixed,  this should form a slurry with the consistency of wet sand.
2. Seal this container in a plastic bag with the DHT20 and a separate hygrometer in such a way that allows measurements to be taken from the sensor while still sealed in the bag (such as attaching leads that extend outside the bag). Take care not to let any water spill onto the DHT20, and do not press the air out of the bag before sealing it.
3. Leave the bag overnight (8-10 hours) to allow the humidity inside to settle
4. Take a humidity reading from the DHT20 and the hygrometer, record both values, and remove them from the bag. The actual humidity value should be around 75%.
5. Wait a few minutes for both devices to readjust to the room humidity, and then record the humidity read by both.
6. Perform a linear regression using a tool such as Wolfram|Alpha or Desmos with the two points of data collected. DHT20 humidity readings are the independent variable (X), and actual humidity readings are the dependent variable (Y). Record the coefficients (m and b).

When each sensor has been tested, open the “params” file within the plant-saver’s root directory on the Micro-SD. The “empty” filesystem includes the default values for calibrating each sensor collected by our enterprise team, which can now be replaced. All parameters correspond to coefficients of polynomials of form y = m*x + b, save for the light parameters “a” and “c”. These correspond to the two values found in the second step of the light calibration. 

## Battery Draw / Power Consumption Characterization
The target for the Plant-Saver’s single-charge battery life was set at 7 days of normal use. After determining the discharge curve of the batteries used, the device was tested for a cumulative period of 24 hours without charging the battery. Extrapolating this test data using the discharge curve led to a maximum life of 9 days, with 7-8 being more likely. The methods used in our testing are as follows:
1. Connect the battery to a constant impedance. We tested with 6 parallel 100 Ohm resistors, which in combination created a total resistance of 16.6 Ohms. This led to a draw of 190-240 mA over the course of the test, which allowed the battery to be discharged over the course of 8.5 hours without putting excessive load on it.
2. Take current and voltage readings from the battery roughly every hour. The current readings can be used to determine the capacity usage of the battery at the time of each voltage reading.
3. Plot the voltage-capacity data to get an approximate discharge curve. A quartic regression works well to approximate this data, which is fairly consistent with standard LiPo discharge curves.
4. Find the low-voltage cutoff point of the device on this graph, which is determined by the dropout of the linear regulator (0.26 V + 3.3 V = 3.56 V). The point at which the curve crosses this point is the total capacity available to the device (1469 mAh).
5. Recharge the battery, then use it to power the Plant-Saver for 24 hours. We took voltage measurements periodically for validation, but this is not strictly necessary. 
6. Measure the voltage of the battery after the 24-hour period. Then find where this voltage intersects the discharge curve. This is the capacity usage after 1 day (154 mAh). The electronics on the Plant-Saver are relatively consistent in their current usage, so this value can be used to extrapolate capacity usage for any period simply by multiplying. After 7 days, the expected battery voltage would be 3.72 V, which intersects the curve at 1078 mAh.

<img src="/Assets/Battery_graph.png" alt="Graph showing the battery's discharge curve along with the measured 24-hour capacity usage and predicted 7-day capacity usage." width="400"/>

*Plant-Saver power consumption characteristics*

## Housing
When creating an enclosure for the PCB, it was necessary to ensure that each sensor could obtain accurate readings while maintaining environmental resistance and a form factor suitable for potted plant use. As a baseline design, a structure resembling a birdhouse with a hip roof was developed to balance functionality and aesthetics. The four angled sides of the roof allow for straightforward integration of inlays for solar panels, enabling the collection of sunlight data from multiple directions.

The DHT20 temperature sensor is housed in a pocket located toward the front of the enclosure, positioned between the user interface (UI) screen and buttons. To ensure accurate environmental readings while maintaining protection, a grated opening was incorporated, allowing airflow without exposing the sensor to damage. The moisture sensor, which must be inserted into soil for proper operation, is accommodated by a slit at the bottom of the enclosure. A supporting ledge is included to secure the sensor by the rear of its female connector.

The OLED display is mounted to the front of the enclosure using bolts, with an additional outcrop to accommodate wiring connections. The buttons and LEDs are soldered onto a stripboard, which is then affixed to a designated inlay, with corresponding openings for user interaction and visibility.

To allow access to the PCB, a removable back panel is implemented. The PCB is secured to this panel using two screws, and the assembled panel is then fastened to the main housing once all necessary connections are made.

For accessibility and ease of manufacturing, the enclosure is designed for 3D printing. The model can be produced in two parts with minimal error. PETG is recommended as the printing material due to its durability and improved environmental resistance.

## User Interface / Web App

The device makes use of two different user interfaces, a physical one, and a web app. The physical interface is operated using the buttons on the front of the device, and the web interface can be accessed by connecting to the device’s access point using standard WIFI. Both of these interfaces are synced together, and start up automatically when the device is powered on entering sleep mode upon 60 seconds of inactivity for power saving purposes. The physical interface is made up of four independent push buttons and an OLED display. Each of these buttons serves a different function: one to change screens, one to ‘scroll’ up, one to ‘scroll’ down, and a ‘select’ button, which also serves as the ‘wakeup’ button for when the device is in sleep mode. The physical interface itself consists of three primary screens: 
- A “main menu” to display sensor data averages and recommendations. Recommendations are provided in a graphical form. For each data type, a set of two tall vertical lines represents the minimum and maximum measurable values. A set of two shorter lines represents the recommended thresholds for the selected plant, and a flower-shaped marker represents the average readings. If this marker is outside the recommended thresholds, an arrow will be present showing the desired direction. Pressing the “select” (left) button focuses on this menu, allowing the up/down buttons to be used for highlighting one of the four data types. Pressing select again moves to a display showing more information on the highlighted data type, including the average value, recommendation thresholds, units, and additionally the primary direction if the value selected is sunlight.
- A “info menu” to display additional information on the plant such as scientific name and environmental preferences in text form. 
- A “search menu” to lookup plants from the database. Pressing select focuses on this display, allowing the up/down buttons to be used for alphabetically scrolling the highlighted letter in the displayed five-character query. Pressing the change screen button with focus on this menu changes the highlighted character. Pressing and holding the change screen button exits focus. Pressing select while focused on this menu initiates a search of the database for the query, moving to a scrollable list of plants with similar names. 

Additionally, there is a recommendation request screen which is only available if the device has enough data to recommend a suitable plant. Pressing select on this menu brings up a scrollable list of candidate plants to choose from. Pressing select on either scrollable menu (recommendation or post-search) sets the highlighted plant as the active plant, clearing out existing sensor data.

The web interface is a locally hosted website, accessible through the device’s access point as mentioned earlier. The default SSID and password for the device are set as ‘Plant-Saver’ and ‘plants123’ respectively, both of which could be altered if desired. The website itself displays much of the same information as the physical interface, the main difference found in the comparison between a plant’s environmental requirements and the current sensor readings. The web interface displays both the requirements and the sensor readings interpreted in accordance with the calibration of our sensors. This allows for a more specific idea of the relation between the requirements and the actual environment. The website additionally displays the same list of recommended plants as the physical interface. In search of a more efficient plant selection method the web interface implements a ‘search by name’ function that makes use of a prefix-autocomplete feature to ease the selection process. This search box allows the user to type the name of the plant they are interested in. As they type, the device sorts through the database based on their current input and displays plants with similar prefixes in a list below the search box. This was done in an attempt to remove the headache of determining if a plant exists on the device’s database, as well as smoothen the overall process of changing selected plants. Finally, in the interest of user convenience, as mentioned above the physical and web interfaces are synchronized. This means that selecting a plant on one will display the selection on both, ensuring consistent, accurate name and requirement displays across both interfaces. 

## Plant Database
The Plant-Saver uses four pieces of data to determine plant health, with recommended values for three of these in the form of USDA hardiness zone ,water requirements, and light requirements being provided by permapeople’s database. The plant ID, common name, and scientific name are also sourced from this database. The plant parsing script ensures that the data that is taken from permapeople can be used by the on-board data processing. It does this by first calling permapeople's API until all available plants have been received, as each call returns roughly 100 plants worth of data. After all of the data is obtained, each plant entry is combed through to find all of the plants that have the requisite information to be used with the Plant-Saver. While the plants are being searched through, all of the data provided by permapeople that isn’t used in our analysis is removed to save on storage space on-board the Plant-Saver. Additionally, when removing extraneous data, all of the text-based data is converted to enumerated numerical representations that correlate to each of the options for hardiness zone, light requirement, and water requirement for simpler use onboard the Plant-Saver. After each plant is determined to have adequate data, each of the plants that do not have adequate data are removed from the list. After the unwanted plants have been culled, the remaining plants are saved to a JSON file that is ready to be manually loaded onto the device's SD card.

# Acknowledgements
- Permapeople database at permapeople.org
- Dr. Shane Oberloier for his advisorship
- Michigan Technological University for the facilities and funding
- All OSHE members for their direct and indirect support

# References
[1] OSHE, “GitHub - OSHE-Github/Plant-Saver,” GitHub, 2025. https://github.com/OSHE-Github/Plant-Saver (accessed Dec. 05, 2025).

[2] Plant-Saver, “OSF,” Osf.io, 2025. https://osf.io/kzrm2/overview (accessed Dec. 05, 2025).

[3] “Silicon Labs,” Silabs.com, 2025. https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads
