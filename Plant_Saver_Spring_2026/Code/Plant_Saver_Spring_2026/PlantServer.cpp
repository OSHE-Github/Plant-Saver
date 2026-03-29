//Suppress ESP32 BT pragma messages, WE DO NOT CARE
#define BT_NO_PRAGMA_MESSAGE

/*------------------------------------------------Libraries--------------------------------------------*/
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include <unordered_map>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "PlantSaverClasses.h"

/*--------------------------------------WiFi Configuration---------------------------------------------*/
const char* ssid = "Plant-Saver";
const char* password = "plants123";
AsyncWebServer server(80);  //create Web Server on port 80

/*--------------------------------------Plant Object and Vector Storage--------------------------------*/
struct serverPlant {
    String name;
    int id; 
    int hardiness_zone_low;
    int hardiness_zone_high;
    int light_requirement_low;
    int light_requirement_high;
    int water_requirement_low;
    int water_requirement_high;
};

//global dynamic vector to store all plants
std::vector<serverPlant> plants;
//index for fast (O(1)) lookup by lowercase name
std::unordered_map<std::string, size_t> plantIndex;
//array for autocomplete --> stores only plant names
std::vector<String> plantNames;

/*--------------------------------------Reading JSON---------------------------------------------------*/
//read plants from the JSON, streaming one at a time so ESP32 memory is an issue
bool loadPlantsFromJSON(const char* filename = "/plantDB.txt") {
    
    //open the file in read mode
    File file = SD.open(filename, FILE_READ);

    //make sure the file opened successfully
    if (!file) {
        Serial.println("Failed to open plantDB file for reading");

        if (globalContainer != nullptr) {
            globalContainer->error.addError(fileOperation);
        }
        return false;
    }

    plantNames.clear();
    plants.clear();
    plantIndex.clear();

    // reserve estimated capacity based on file size to reduce repeated reallocations
    size_t fileSize = file.size();
    size_t estimatedCount = max((size_t)25, fileSize / 256);
    plantNames.reserve(min(estimatedCount, (size_t)5000));
    plants.reserve(min(estimatedCount, (size_t)5000));
    plantIndex.reserve(min(estimatedCount, (size_t)5000));

    //skip to the next plant object
    while (file.available()) {
        
        char c = file.read();
        if (c == '['){

            break;
        }
    }

    //sized for one plant object;
    DynamicJsonDocument doc(4096); 
    size_t plantCount = 0;

    //read each plant object until the end of the array
    while (file.available()) {

        char c = 0;
        while(c!='{'){

            if (!file.available()) {
                //end of file
                break;
            }

            c = file.read();

            //end of array
            if (c == ']'){
                
                break;
            }
        }

        if (c != '{') {

            break;
        }

        //read the complete plant object
        String obj;
        obj.reserve(512);
        obj += '{';
        int depth = 1;
        //depth is used to handle nested objects, we want to read until the matching closing brace
        while (file.available() && depth > 0) {

            char ch = file.read();
            obj += ch;
            if (ch == '{') depth++;
            else if (ch == '}') depth--;
        }

        //parse single plant object
        DeserializationError error = deserializeJson(doc, obj);
        if (error) {

            Serial.print("JSON parsing failed for plant #");
            Serial.print(plantCount);
            Serial.print(" : ");
            Serial.println(error.c_str());
            doc.clear();

            if (globalContainer != nullptr) {
                globalContainer->error.addError(jsonError);
            }
            continue;
        }

        JsonObject plant = doc.as<JsonObject>();
        serverPlant p;
        p.name = plant["name"].as<String>();
        p.id = plant["id"].as<int>();

        //setting defaults
        p.hardiness_zone_low = -1;
        p.hardiness_zone_high = -1;
        p.light_requirement_low = -1;
        p.light_requirement_high = -1;
        p.water_requirement_low = -1;
        p.water_requirement_high = -1;

        //parse the data array for all the requirements
        JsonArray dataArray = plant["data"].as<JsonArray>();

        for(JsonObject entry : dataArray){

          String key = entry["key"].as<String>();
          JsonArray values = entry["value"].as<JsonArray>();

          if(values.isNull() || values.size() == 0){

            continue;

          }

          if(key == "USDA Hardiness zone"){

            p.hardiness_zone_low = values[0].as<int>();
            if(values.size() == 1){

              p.hardiness_zone_high = p.hardiness_zone_low;

            }
            else{

              p.hardiness_zone_high = values[1].as<int>();

            }
          }
          else if(key == "Light requirement"){

            p.light_requirement_low = values[0].as<int>();
            if(values.size() == 1){

              p.light_requirement_high = p.light_requirement_low;

            }
            else{

              p.light_requirement_high = values[1].as<int>();

            }
          }
          else if(key == "Water requirement"){

            p.water_requirement_low = values[0].as<int>();
            if(values.size() == 1){

              p.water_requirement_high = p.water_requirement_low;

            }
            else{

              p.water_requirement_high = values[1].as<int>();

            }
          }
        }
        
        //add the plant to the vector and index it by vector-location in the hashmap, add just the name to the names array for autocomplete
        plants.push_back(p);
        plantNames.push_back(p.name);
        String lookup = p.name;
        lookup.toLowerCase();
        plantIndex[std::string(lookup.c_str())] = plants.size() - 1;
        plantCount++;

        //clear the document so it can be reused
        doc.clear();

        //skip any commas or whitespace between objects
        while (file.available()) {

            char nc = file.peek();
            if (nc == ',' || nc == '\n' || nc == '\r' || nc == ' ' || nc == '\t') {

                file.read();
                continue;
            }
            break;
        }
    }


    file.close();
    Serial.print("Successfully loaded ");
    Serial.print(plants.size());
    Serial.println(" plants from JSON file");
    return true;
}

/*--------------------------------------Search for a plant---------------------------------------------*/
//Search for a plant by name (case-insensitive), this is the important one for the 'search' function on the webpage
serverPlant* findPlantByName(const String& name) {

    //O(1) search with a hashmap
    String key = name; 
    key.toLowerCase();
    auto it = plantIndex.find(std::string(key.c_str()));

    if(it != plantIndex.end()){

      return &plants[it->second];

    }

    //Plant not found
    return nullptr;
}

//Get all plants, maybe used for a 'list' of plants on the webpage? (menu button or something similar)
const std::vector<serverPlant>& getAllPlants() {
    return plants;
}

/*--------------------------------------Print Plants (Debugging)---------------------------------------*/
void printPlant(const serverPlant& plant) {
    Serial.print(" | Name: ");
    Serial.print(plant.name);
    Serial.print(" | Hardiness Zone: ");
    Serial.print(plant.hardiness_zone_low);
    Serial.print(" - ");
    Serial.print(plant.hardiness_zone_high);
    Serial.print(" | Light: ");
    Serial.print(plant.light_requirement_low);
    Serial.print(" - ");
    Serial.print(plant.light_requirement_high);
    Serial.print(" | Water: ");
    Serial.print(plant.water_requirement_low);
    Serial.print(" - ");
    Serial.println(plant.water_requirement_high);
}

void printAllPlants() {
    Serial.println("\n=== All Plants ===");
    for (const auto& plant : plants) {
        printPlant(plant);
    }
    Serial.println("==================\n");
}

/*-----------------------------------WEB SERVER API HANDLERS-------------------------------------------*/
//api endpoint for the currently selected plant, used for webapp to OLED sync
void handleGetCurrentPlant(AsyncWebServerRequest *request) {
    
    DynamicJsonDocument doc(512);

    if (globalContainer == nullptr) {

        request->send(500, "application/json", "{\"error\": \"Device not ready\"}");
        return;
    }

    //if no plant is selected yet, return empty
    if (!globalContainer->header.plantSelected) {
        
        request->send(200, "application/json", "{\"selected\": false}");
        return;
    }

    //return selected plant info (from activePlant)
    doc["selected"] = true;
    doc["name"] = globalContainer->activePlant.commonName;
    doc["hardiness_zone_low"] = globalContainer->activePlant.hardiness[0];
    doc["hardiness_zone_high"] = globalContainer->activePlant.hardiness[1];
    
    doc["light_requirement_low"] = globalContainer->activePlant.lightReq[0];
    
    if(globalContainer->activePlant.lightReq[1] == 0){
        
        doc["light_requirement_high"] = globalContainer->activePlant.lightReq[0];
    }
    else{
        
        doc["light_requirement_high"] = globalContainer->activePlant.lightReq[1];
    }

    doc["water_requirement_low"] = globalContainer->activePlant.waterReq[0];

    if(globalContainer->activePlant.waterReq[1] == 0){

        doc["water_requirement_high"] = globalContainer->activePlant.waterReq[0];
    }
     else{
    
        doc["water_requirement_high"] = globalContainer->activePlant.waterReq[1];
    }

    //include the sensor averages for current plant
    float avgLight, avgTemp, avgWater, avgHumidity;
    getActivePlantAverages(avgLight, avgTemp, avgWater, avgHumidity);
    doc["avgLight"] = avgLight;
    doc["avgTemp"] = avgTemp;
    doc["avgWater"] = int(((globalContainer->sensorReading.waterM * double(globalContainer->activePlant.avgWater)) + globalContainer->sensorReading.waterB) *100);;
    doc["avgHumidity"] = avgHumidity;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}


void handleGetPlantByName(AsyncWebServerRequest *request) {
    
    //make sure that the request has a name, this should just be true because of the search but better safe than sorry
    if (request->hasParam("name")) {

        //get that name parameter and search the database for it
        String name = request->getParam("name")->value();
        serverPlant* plant = findPlantByName(name);
        
        //if the plant exists in the database, make and return a JSON object with the correct information
        if (plant) {

            //make the JSON object
            DynamicJsonDocument doc(512);
            doc["name"] = plant->name;
            doc["hardiness_zone_low"] = plant->hardiness_zone_low;
            doc["hardiness_zone_high"] = plant->hardiness_zone_high;
            doc["light_requirement_low"] = plant->light_requirement_low;
            doc["light_requirement_high"] = plant->light_requirement_high;
            doc["water_requirement_low"] = plant->water_requirement_low;
            doc["water_requirement_high"] = plant->water_requirement_high;

            //searching for a plant also selects it on the device
            if (globalContainer != nullptr) {

                globalContainer->activePlant.id = plant->id;
                plant->name.toCharArray(globalContainer->activePlant.commonName, NUM_CHARS_NAME);

                globalContainer->activePlant.hardiness[0] = plant->hardiness_zone_low;
                globalContainer->activePlant.hardiness[1] = plant->hardiness_zone_high;
                globalContainer->activePlant.lightReq[0] = plant->light_requirement_low;
                globalContainer->activePlant.lightReq[1] = plant->light_requirement_high;
                globalContainer->activePlant.waterReq[0] = plant->water_requirement_low;
                globalContainer->activePlant.waterReq[1] = plant->water_requirement_high;

                globalContainer->header.plantSelected = 1;
                globalContainer->activePlant.plantPulled = 1;
                Serial.println("Plant searched/selected: " + name);
            }

            //include current sensor averages (will be for the newly selected plant)
            float avgLight, avgTemp, avgWater, avgHumidity;
            getActivePlantAverages(avgLight, avgTemp, avgWater, avgHumidity);

            doc["avgLight"] = avgLight;
            doc["avgTemp"] = avgTemp;
            doc["avgWater"] = avgWater;
            doc["avgHumidity"] = avgHumidity;

            //send the JSON
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        }

        //send an error if plant doesn't exist in the database, this is caught in the webpage 
        else {
            request->send(404, "application/json", "{\"error\": \"Plant not found\"}");
        }
    } 

    //send an error if the name parameter is missing
    else {
        request->send(400, "application/json", "{\"error\": \"Missing name parameter\"}");
    }
}

/*--------------------------------------Setup WiFi as Access Point-------------------------------------*/
//Setup the ESP32 as a WiFi Access Point, this is what the webpage will connect to in order to access the API and the webpage files
void setupWiFi() {

    Serial.println("Setting up ESP32 Access Point...");
    
    //disconnect any existing WiFi as insurance
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    
    //get and print the IP address of the access point, convenient for testing, could remove later
    IPAddress IP = WiFi.softAPIP();
    Serial.print("Access Point IP address: ");
    Serial.println(IP);
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password); //obviously not secure, can remove specifically this part if wanted
}

//Setup Web Server on board
void setupWebServer() {

    Serial.println("Setting up Web Server API route...");
    
    //defined API endpoint for plant search (which also selects the plant on the device)
    server.on("/api/plant/name", HTTP_GET, handleGetPlantByName);

    //API endpoint to get the currently selected plant (for webapp polling)
    server.on("/api/current-plant", HTTP_GET, handleGetCurrentPlant);
    
    //API for plant name autocomplete
    server.on("/api/plants", HTTP_GET, [](AsyncWebServerRequest *request){
        if(!globalContainer){
            request->send(500, "text/plain", "Container not ready");
            return;
        }

        String prefix = "";
        if (request->hasParam("prefix")) {
            prefix = request->getParam("prefix")->value();
            prefix.toLowerCase();
        }

        size_t limit = 50; // default limit
        if (request->hasParam("limit")) {
            long ll = request->getParam("limit")->value().toInt();
            if (ll > 0 && ll <= 200) {
                limit = ll;
            }
        }

        // Construct JSON from filtered list (worst case limited to small number)
        DynamicJsonDocument doc(8192);
        JsonArray plantsArray = doc.createNestedArray("plants");

        size_t count = 0;
        for (const auto& name : plantNames) {
            if (!prefix.length() || name.toLowerCase().startsWith(prefix)) {
                plantsArray.add(name);
                count++;
                if (count >= limit) {
                    break;
                }
            }
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    //API for recommended plants via pullWebRecs()
    server.on("/api/recommendations", HTTP_GET, [](AsyncWebServerRequest *request){
        
        if (!globalContainer) {
            
            request->send(500, "application/json", "{\"error\": \"Device not ready\"}");
            return;
        }

        globalContainer->interface.pullWebRecs();

        DynamicJsonDocument doc(1024);
        doc["numRecCandidates"] = globalContainer->interface.numRecCandidates;
        JsonArray recArray = doc.createNestedArray("recommendations");

        int maxRecs = globalContainer->interface.numRecCandidates;
        if (maxRecs > NUM_WEB_RECS) {

            maxRecs = NUM_WEB_RECS;
        }

        for (int i = 0; i < maxRecs; i++) {
            
            JsonObject rec = recArray.createNestedObject();
            rec["rank"] = i + 1;
            rec["name"] = String(globalContainer->interface.webRecPlants[i]);
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        
        DynamicJsonDocument doc(128);
        
        doc["status"] = globalContainer ? "ready" : "starting";
        doc["errorCode"] = globalContainer ? globalContainer->error.highestPriority : 0;
        
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body);
    });
    
    //Serve static files from SD card, set index.html as default
    server.serveStatic("/", SD, "/").setDefaultFile("index.html");//.setCacheControl("max-age=600");
    
    //Start server
    server.begin();
    Serial.println("Web Server started");
}

/*-----------------------------------------------------------------------------------------------------*/
void initializePlantServer() {
    
    Serial.println("\n\n=== PlantSaver ESP32 Initialization ===");
    
    // Initialize SD card --> have to check the CS pin, but pictures in drive on the prototype look like its on 5
    if (!SD.begin(5)) {
        
        //log SD card initialization failure
        Serial.println("SD Card initialization failed!");
        
        //Add error to the error system for display on OLED
        if (globalContainer != nullptr) {
            globalContainer->error.addError(SDInit);
        }
        
        //Setup WiFi and Web Server anyway so user can see error on webapp
        setupWiFi();
        setupWebServer();
        
        Serial.println("=== Initialization Complete (SD Card Error) ===\n");
        return;
    }
    
    Serial.println("SD Card initialized successfully");
    
    //Load plants from JSON on uSD used for hashmap NOTE: NEED EXTENSION TYPE ON FILENAME
    if (loadPlantsFromJSON("/plantDB.txt")) {
        printAllPlants();
    }
    
    //Setup WiFi Access Point
    setupWiFi();
    
    //Setup Web Server and API
    setupWebServer();
    
    Serial.println("=== Initialization Complete ===\n");
}