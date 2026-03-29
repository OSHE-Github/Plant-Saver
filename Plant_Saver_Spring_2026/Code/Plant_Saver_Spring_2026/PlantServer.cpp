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
#include <cstring>
#include "PlantSaverClasses.h"

/*--------------------------------------WiFi Configuration---------------------------------------------*/
const char* ssid = "Plant-Saver";
const char* password = "plants123";
AsyncWebServer server(80);  //create Web Server on port 80

struct serverPlant{

    char name[40];
    uint16_t id;
    uint8_t hardiness_zone_low;
    uint8_t hardiness_zone_high;
    uint8_t light_requirement_low;
    uint8_t light_requirement_high;
    uint8_t water_requirement_low;
    uint8_t water_requirement_high;
};

struct plantPrefix{

    char prefix[5];
    uint32_t offset;
};

//case conversion
static void toLowerCase(char* str){

    for(size_t i = 0; str[i]; i++){

        if(str[i] >= 'A' && str[i] <= 'Z'){

            str[i] = str[i] + 32;

        }
    }
}

//prefix autocomplete index
plantPrefix prefixes[6000]; //~number of plants
size_t plantCount = 0;

bool buildPrefixIndex(const char* filename = PLANT_DB_PATH){

    File file = SD.open(filename, FILE_READ);
    if(!file){

        Serial.println("Failed to open plantDB file for reading");
        if(globalContainer) globalContainer->error.addError(fileOperation);
        return false;
    }

    plantCount = 0;
    StaticJsonDocument<256> doc;

    while(file.available() && plantCount < 6000){

        if(!file.find("{\"id\":")) break;
        uint32_t offset = (uint32_t)file.position() - 6;
        

        //just the name
        StaticJsonDocument<32> filter;
        filter["name"] = true;
        DeserializationError deserError = deserializeJson(doc, file, DeserializationOption::Filter(filter));
        if(deserError){

            doc.clear();
            globalContainer->error.addError(jsonError);
            continue;
        }

        const char* name = doc["name"] | "";
        if(!name[0]) continue;

        //storing first 4 letters for autocomplete
        for(int i = 0; i < 4; i++){

            prefixes[plantCount].prefix[i] = name[i] ? tolower(name[i]) : '\0';
        }
        prefixes[plantCount].prefix[4] = '\0';
        prefixes[plantCount].offset = offset;
        plantCount++;
        doc.clear();
    }
    file.close();
    return plantCount > 0;
}

//autocomplete search --> returns vector of file offsets for matching plants, much smaller than storing names
void searchPlantByPrefix(const char* input, std::vector<uint32_t>& results){

    results.clear();
    char lowerInput[5];
    strncpy(lowerInput, input, 4);
    lowerInput[4] = '\0';
    toLowerCase(lowerInput);

    //technically not the most efficent but for ~6000 plants shouldnt take more than a few ms
    size_t len = strlen(lowerInput);
    for(size_t i = 0; i < plantCount; i++){

        if(strncmp(prefixes[i].prefix, lowerInput, len) == 0){

            results.push_back(prefixes[i].offset);
        }
    }
}

//parse whole plant
static bool parsePlantAtOffset(uint32_t offset, serverPlant& p){

    File file = SD.open(PLANT_DB_PATH, FILE_READ);
    if(!file){

        Serial.println("Failed to open plantDB file for reading");
        if(globalContainer) globalContainer->error.addError(fileOperation);
        return false;
    }

    file.seek(offset);
    StaticJsonDocument<512>doc;
    DeserializationError deserError = deserializeJson(doc, file);
    if(deserError){

        doc.clear();
        globalContainer->error.addError(jsonError);
        file.close();
        return false;
    }

    
    const char* nameStr = doc["name"] | "";
    if(!nameStr[0]){

        doc.clear();
        file.close();
        return false;
    }

    strncpy(p.name, nameStr, 39);
    p.name[39] = '\0';
    p.id = doc["id"];

    JsonArray data = doc["data"];

    for(int i = 0; i < data.size(); i++){

        const char* key = data[i]["key"] | "";
        JsonArray values = data[i]["value"];

        if(strcmp(key, "USDA Hardiness zone") == 0){

            p.hardiness_zone_low = values[0].as<uint8_t>();
            p.hardiness_zone_high = (values.size() > 1) ? values[1].as<uint8_t>() : p.hardiness_zone_low;
        }
        else if(strcmp(key, "Light requirement") == 0){

            p.light_requirement_low = values[0].as<uint8_t>();
            p.light_requirement_high = (values.size() > 1) ? values[1].as<uint8_t>() : p.light_requirement_low;
        }
        else if(strcmp(key, "Water requirement") == 0){

            p.water_requirement_low = values[0].as<uint8_t>();
            p.water_requirement_high = (values.size() > 1) ? values[1].as<uint8_t>() : p.water_requirement_low;
        }
    }
    doc.clear();
    file.close();
    return true;
}

void setupWebServer(){

    Serial.println("Setting up Web Server API routes...");

    //API endpoint for autocomplete
    server.on("/api/plants", HTTP_GET, [](AsyncWebServerRequest *request){

        if(!request -> hasParam("prefix")){

            request -> send(400, "application/json", "{\"error\": \"Missing prefix parameter\"}");
            return;
        }

        String prefix = request-> getParam("prefix")->value();
        prefix.toLowerCase();

        int limit = 10;
        if(request -> hasParam("limit")){

            limit = request -> getParam("limit")->value().toInt();
        }

        std::vector<uint32_t> offsets;
        searchPlantByPrefix(prefix.c_str(), offsets);
        
        StaticJsonDocument<512> doc;
        JsonArray plantsArray = doc.createNestedArray("plants");
        File file = SD.open(PLANT_DB_PATH, FILE_READ);
        
        int count = 0;
        for(uint32_t offset : offsets){

            if(count >= limit)break;
            file.seek(offset);
            StaticJsonDocument<256> tempDoc;
            deserializeJson(tempDoc, file);

            const char* name = tempDoc["name"];
            if(name) plantsArray.add(name);
            count++;
            tempDoc.clear();
        }
        
        file.close();
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    //API endpoint for search by name
    server.on("/api/plant/name", HTTP_GET, [](AsyncWebServerRequest *request){

        if(!request ->hasParam("name")){

            request ->send(400, "application/json", "{\"error\": \"Missing name parameter\"}");
            return;
        }

        String name = request ->getParam("name") ->value();
        name.toLowerCase();

        std::vector<uint32_t> offsets;
        searchPlantByPrefix(name.c_str(), offsets);

        serverPlant plant;
        bool found = false;

        for(uint32_t offset : offsets){

            if(parsePlantAtOffset(offset, plant)){
                
                String plantName = String(plant.name);
                plantName.toLowerCase();
                
                if(plantName == name){

                    found = true;
                    if (globalContainer) {
                        globalContainer->activePlant.id = plant.id;
                        strncpy(globalContainer->activePlant.commonName, plant.name, NUM_CHARS_NAME - 1);
                        globalContainer->activePlant.commonName[NUM_CHARS_NAME - 1] = '\0';
                        globalContainer->activePlant.hardiness[0] = plant.hardiness_zone_low;
                        globalContainer->activePlant.hardiness[1] = plant.hardiness_zone_high;
                        globalContainer->activePlant.lightReq[0]  = plant.light_requirement_low;
                        globalContainer->activePlant.lightReq[1]  = plant.light_requirement_high;
                        globalContainer->activePlant.waterReq[0]  = plant.water_requirement_low;
                        globalContainer->activePlant.waterReq[1]  = plant.water_requirement_high;
                        globalContainer->header.plantSelected = 1;
                        globalContainer->activePlant.plantPulled = 1;
                    }
                    break;
                }
            }
        }

        if(!found){

            request->send(404, "application/json", "{\"error\":\"Plant not found\"}");
            return;
        }

        StaticJsonDocument<256> doc;
        doc["name"] = plant.name;
        doc["hardiness_zone_low"] = plant.hardiness_zone_low;
        doc["hardiness_zone_high"] = plant.hardiness_zone_high;
        doc["light_requirement_low"] = plant.light_requirement_low;
        doc["light_requirement_high"] = plant.light_requirement_high;
        doc["water_requirement_low"] = plant.water_requirement_low;
        doc["water_requirement_high"] = plant.water_requirement_high;
        doc["avgLight"] = globalContainer->activePlant.avgLight;
        doc["avgTemp"] = globalContainer->activePlant.avgTemp;
        doc["avgWater"] = globalContainer->activePlant.avgWater;
        doc["avgHumidity"] = globalContainer->activePlant.avgHumidity;

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    server.on("/api/current-plant", HTTP_GET, [](AsyncWebServerRequest *request){

        StaticJsonDocument<512> doc;

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
    });

    //API for device states/error handling
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        
        char response [128];
        const char* status = globalContainer ? "ready" : "starting";
        int errorCode = globalContainer ? globalContainer->error.highestPriority : 0;

        snprintf(response, sizeof(response), "{\"status\": \"%s\", \"errorCode\": %d}", status, errorCode);

        request->send(200, "application/json", response);
    });

    //API for recommended plants via pullWebRecs()
    server.on("/api/recommendations", HTTP_GET, [](AsyncWebServerRequest *request){
        
        if (!globalContainer) {
            
            request->send(500, "application/json", "{\"error\": \"Device not ready\"}");
            return;
        }

        globalContainer->interface.pullWebRecs();
            
        StaticJsonDocument<512> doc;
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

    //Serve static files from SD card, set index.html as default
    server.serveStatic("/", SD, "/").setDefaultFile("index.html");
    server.begin();
    Serial.println("Web Server started");
}

void setupWifi(){

    Serial.println("Setting up ESP32 Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    Serial.println("WiFi Access Point Created");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("IP Address: ");
    Serial.println(IP);
}

void initializePlantServer(){

    Serial.println("\n\n=== PlantSaver ESP32 Initialization ===");

    if(!SD.begin(5)){

        Serial.println("Failed to initialize SD card");
        if(globalContainer) globalContainer->error.addError(SDInit);

        setupWifi();
        setupWebServer();
        
        Serial.println("=== Initialization Complete (SD Card Error) ===\n");
        return;
    }

    Serial.println("SD Card Initalized Successfully");

    buildPrefixIndex();

    setupWifi();
    setupWebServer();
    Serial.println("=== Initialization Complete ===\n");
}