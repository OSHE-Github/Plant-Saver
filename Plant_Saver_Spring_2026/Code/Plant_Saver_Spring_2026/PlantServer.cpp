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

/*--------------------------------------WiFi Configuration---------------------------------------------*/
const char* ssid = "Plant-Saver";
const char* password = "plants123";
AsyncWebServer server(80);  //create Web Server on port 80

/*--------------------------------------Plant Object and Vector Storage--------------------------------*/
struct Plant {
    String name;
    int hardiness_zone_low;
    int hardiness_zone_high;
    int light_requirement_low;
    int light_requirement_high;
    int water_requirement_low;
    int water_requirement_high;
};

//global dynamic vector to store all plants
std::vector<Plant> plants;
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
        Serial.println("Failed to open file for reading");
        return false;
    }

    plantNames.clear();
    plants.clear();
    plantIndex.clear();

    //skip to the next plant object
    while (file.available()) {
        
        char c = file.read();
        if (c == '['){

            break;
        }
    }

    //sized for one plant object
    DynamicJsonDocument doc(1024); 
    size_t plantCount = 0;

    //read each plant object until the end of the array
    while (file.available()) {

        char c = 0;
        while(c!='{'){

            if (!file.available()) {
                
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
            continue;
        }

        JsonObject plant = doc.as<JsonObject>();
        Plant p;
        p.name = plant["name"].as<String>();

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
Plant* findPlantByName(const String& name) {

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
const std::vector<Plant>& getAllPlants() {
    return plants;
}

/*--------------------------------------Print Plants (Debugging)---------------------------------------*/
void printPlant(const Plant& plant) {
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
//API: Get plant by name
void handleGetPlantByName(AsyncWebServerRequest *request) {
    
    //make sure that the request has a name, this should just be true because of the search but better safe than sorry
    if (request->hasParam("name")) {

        //get that name parameter and search the database for it
        String name = request->getParam("name")->value();
        Plant* plant = findPlantByName(name);
        
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

            //send the JSON
            String response;
            serializeJsonPretty(doc, response);
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
    
    //defined API endpoint for the search function
    server.on("/api/plant/name", HTTP_GET, handleGetPlantByName);
    
    //API for all plants, primarily using for the autocomplete, could maybe be used as a list of all plants as well?
    server.on("/api/plants", HTTP_GET, [](AsyncWebServerRequest *request){
        
        //Get all plants names and return as JSON array
        DynamicJsonDocument doc(4096);
        JsonArray plantsArray = doc.createNestedArray("plants");
        
        for (const auto& name : plantNames) {
            plantsArray.add(name);
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
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

        //stop it forever, can't do anything without the database
        Serial.println("SD Card initialization failed!");
        while (1);
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
/*-------------------------------------Handle Plant Server (called in loop for sensor readings)--------------------------------*/
void handlePlantServer() {
    
}
