let allPlants = [];
let currentDisplayedPlant = "";

//ESP32 API endpoint (change if needed)
const ESP32_IP = "http://192.168.4.1"; //default ESP32 AP IP

//wait for DOM to load before attaching event listeners, this is especially important with the esp as the AP, can be finicky with initial start-up load times
document.addEventListener("DOMContentLoaded", function() {
    
    const form = document.getElementById("searchForm");
    const searchInput = document.getElementById("searchField");
    const listContainer = document.getElementById("autocomplete-list");

    //event listener for the autocomplete of the searchbox
    searchInput.addEventListener("input", function() {

        //get the current value of the search box and trim/format it for comparisons
        const inputValue = this.value.trim().toLowerCase();
        listContainer.innerHTML = "";

        //if there is nothing in the box, don't show any autocompletes
        if (!inputValue) {
            return;
        }

        //filter all the plants based on the current input
        const matches = allPlants.filter(plant => plant.toLowerCase().startsWith(inputValue));
        
        //for any matches, make a new entry in the auto complete list, capped at 5 maximum, can be adjusted if needed
        matches.slice(0, 5).forEach(match => {

            const item = document.createElement("div");
            item.textContent = match;
            
            //if the entry is clicked, fill the search box with the plant name and clear the autocomplete list
            item.addEventListener("click", function() {
                searchInput.value = match;
                listContainer.innerHTML = "";
                form.dispatchEvent(new Event("submit"));
            });

            //add the entry to the autocomplete list container
            listContainer.appendChild(item);
        });
    });

    //search submission listener, either clicking the button or pressing enter
    form.addEventListener("submit", function(e){
        
        //prevent form from submitting and refreshing the page
        e.preventDefault();        
        //get the plane name from the search box, trimmed white space to be safe
        const plantName = searchInput.value.trim();
        console.log("Searching for plant:", plantName);
        
        //clear old inputs/data
        document.getElementById("plantName").textContent = "Searching...";
        document.getElementById("hardinessZone").textContent = "--";
        document.getElementById("lightRequirement").textContent = "--";
        document.getElementById("waterRequirement").textContent = "--";

        //clear the autocomplete list on submit
        listContainer.innerHTML = "";
        
        //should be true, but just in case check if the plant name is not empty before making the API call
        if (plantName) {
            getPlantFromESP32(plantName);
        }
    });

    //close the autocomplete if the user clicks outside of the search box or the autocomplete list
    document.addEventListener("click", function(e) {
        if (!searchInput.contains(e.target) && !listContainer.contains(e.target)) {
            listContainer.innerHTML = "";
        }
    });

    //loading all the plants found in the database for the autocomplete
    loadAllPlants();

    //start polling device for current selection and status (updates UI when OLED selection changes)
    setInterval(pollCurrentPlant, 1000);
    setInterval(pollStatus, 2000);
});

//fetch plant data from ESP32
async function getPlantFromESP32(plantName) {
    
    //get the api url with the searched plant
    const apiUrl = `${ESP32_IP}/api/plant/name?name=${encodeURIComponent(plantName)}`;
    console.log("Fetching from:", apiUrl);
    
    try {

        //API call to get the plant data based on the name
        const response = await fetch(apiUrl);

        //if the plant isn't in the database, log an error
        if (response.status === 404) {

            console.error("Plant not found:", plantName);
            displayPlantNotFound(plantName);
            return;
        }

        //throw a general error for any other response issues
        if (!response.ok) {

            throw new Error("General fetch error --> status: " + response.status);
        }

        //successful response
        const plantData = await response.json();
        console.log("Plant data received:", plantData);
        displayPlantData(plantData);
        console.log("Plant automatically selected on device");
    } 
    catch (err) {

        //catch the errors thrown above
        console.error("Error fetching plant data:", err);
        displayFetchError(err);
    }
}

//get all the plants, helps with the autocomplete for the search
async function loadAllPlants() {

    try{

        //get the api url for all plants
        const response = await fetch(`${ESP32_IP}/api/plants`);
        
        //if the response has an issue, throw an error
        if(!response.ok){

            throw new Error("Error fetching all plants --> status: " + response.status);
        }

        //if the response is good, pull the plant array and store it in the global variable
        const data = await response.json();
        allPlants = data.plants;

        console.log("All plants loaded:", allPlants);
    }   
    catch (err) {

        console.error("Error loading all plants:", err);
        
        //sshow alert about potential SD card initialization failure
        window.alert(
            "ERROR: Could not load plant database from device.\n\n" +
            "This may indicate:\n" +
            "-SD card failed to initialize\n" +
            "-SD card not detected\n" +
            "-Corrupted plant database file\n\n" +
            "Solutions:\n" +
            "1. Check device display for error details\n" +
            "2. Re-seat the micro SD card\n" +
            "3. Power cycle the device\n" +
            "4. Try a different micro SD card"
        );
    }
}

//display plant data on the page
function displayPlantData(plantData) {

    //update plant name
    document.getElementById("plantName").textContent = `${plantData.name}`;
    
    //update the 3 requirements
    document.getElementById("hardinessZone").textContent = `${plantData.hardiness_zone_low} - ${plantData.hardiness_zone_high}`;
    
    //if the low and high light requirements are the same, just display one value instead of a range
    if(plantData.light_requirement_low === plantData.light_requirement_high){
        
        document.getElementById("lightRequirement").textContent = `${convertLightToKey(plantData.light_requirement_low)}`;
    }
    //if they are different, display the range as normal
    else {
        document.getElementById("lightRequirement").textContent =  `${convertLightToKey(plantData.light_requirement_low)}` + " - " + `${convertLightToKey(plantData.light_requirement_high)}`;
    }

    //same logic for the water requirement, if the low and high are the same just display one value
    if(plantData.water_requirement_low === plantData.water_requirement_high){

        document.getElementById("waterRequirement").textContent = `${convertWaterToKey(plantData.water_requirement_low)}`;
    }
    //otherwise display the range
    else {
        document.getElementById("waterRequirement").textContent = `${convertWaterToKey(plantData.water_requirement_low)}` + " - " + `${convertWaterToKey(plantData.water_requirement_high)}`;
    }

    
    document.getElementById("waterData").textContent = `${convertAvgWaterToKey(plantData.avgWater)}`;
    document.getElementById("lightData").textContent = `${convertAvgLightToKey(plantData.avgLight)}`;
    document.getElementById("tempData").textContent = `${convertAvgTempToKey(plantData.avgTemp)}`;
    document.getElementById("humidityData").textContent = `${convertAvgHumidityToKey(plantData.avgHumidity)}`;
}


//convert the numeric light requirement to the key
function convertLightToKey(plantData){

    if(plantData === 3){

        return "Full Sun";
    }
    else if(plantData === 2){

        return "Partial Sun/Shade";
    }
    else if(plantData === 1){

        return "Full Shade";
    }

    return "Unknown";
}

//convert the numeric water requirement to the key
function convertWaterToKey(plantData){
    
    if(plantData === 4){

        return "Dry";
    }
    else if(plantData === 3){

        return "Moist";
    }
    else if(plantData === 2){

        return "Wet";
    }
    else if(plantData === 1){

        return "Water";
    }

    return "Unknown";
}

//mapping the average readings to the key for UI usefulness
function convertAvgWaterToKey(avgWater){

    if(4095 >= avgWater >= 1650){

        return "Dry";
    }
    else if(1650 > avgWater >= 1100){

        return "Moist";
    }
    else if(1100 > avgWater >= 700){

        return "Wet";
    }
    else if(700 > avgWater >= 0){

        return "Water";
    }

    return "Unknown";
}

function convertAvgLightToKey(avgLight){

    if(0 <= avgLight < 1075){

        return "Full Shade";
    }
    else if(1075 <= avgLight < 10750){

        return "Partial Sun/Shade";
    }
    else if(avgLight >= 10750){

        return "Full Sun";
    }

    return "Unknown";
}

function convertAvgTempToKey(avgTemp){

    let lowThreshold = 0;
    let highThreshold = 0;
    const LowList = [80, 75, 68, 64, 61, 54, 50, 45, 39, 32, 26];
    const HighList = [30, 36, 43, 48, 54, 57, 64, 68, 72, 79, 80];

    for (let i = 0; i < LowList.length; i++) {

        if(avgTemp <= LowList[i]){

            lowThreshold = 12 - i;
        }

        if(avgTemp >= HighList[i]){
        
            highThreshold = 2 + i;
        
        }
    }

    if(lowThreshold === highThreshold){
        return lowThreshold;
    }
    else {
        return lowThreshold + " - " + highThreshold;
    }
}   

function convertAvgHumidityToKey(avgHumidity){

    //todo: humidityCheck doesn't exist what do?

}

//poll the device for which plant is currently selected on the OLED
async function pollCurrentPlant() {
    
    try {

        const response = await fetch(`${ESP32_IP}/api/current-plant`);

        if (!response.ok){

            return;
        }

        const data = await response.json();

        //if no plant is currently selected, do nothing
        if (!data.selected){
            return;
        }

        //only update UI if the selection changed
        if (data.name && data.name !== currentDisplayedPlant) {

            currentDisplayedPlant = data.name;
            displayPlantData(data);
        }

        //refresh the averages
        if (data.name === currentDisplayedPlant) {
            document.getElementById("waterData").textContent = data.avgWater;
            document.getElementById("lightData").textContent = data.avgLight;
            document.getElementById("tempData").textContent = data.avgTemp;
            document.getElementById("humidityData").textContent = data.avgHumidity;
        }

    } 
    catch (err) {
        //ignoring these for now
        //console.warn("Polling error:", err);
    }
}

async function pollStatus(){

    try {

        const response = await fetch(`${ESP32_IP}/api/status`);

        if (!response.ok){

            console.error("Status polling error --> status: " + response.status);
            return;
        }

        const data = await response.json();
        console.log("Device status:", data.status, "Error code:", data.errorCode);
        
        //display error banner if device reports an error
        if (data.errorCode && data.errorCode !== 0) {

            displayErrorBanner(data.errorCode);
        } 
        else {

            hideErrorBanner();
        }
    } 
    catch (err) {

        console.error("Error polling status:", err);
    }
}

//display error banner based on error code
function displayErrorBanner(errorCode) {

    const errorBanner = document.getElementById("error-banner");
    const errorMessage = document.getElementById("error-message");
    
    if (!errorBanner) return;
    
    let message = "Device Error: ";
    
    //map error codes from PlantSaverClasses.h enum ErrorStatus
    switch(errorCode) {

        case 1: //displayInit

            message += "Display initialization failed. Check OLED connection.";
            break;
        case 2: //lightSensorInit

            message += "Light sensor initialization failed. Check sensor wiring.";
            break;
        case 3: //tempSensorInit

            message += "Temperature sensor initialization failed. Check DHT20 wiring.";
            break;
        case 4: //moistureSensorInit

            message += "Moisture sensor initialization failed.";
            break;
        case 5: //jsonError

            message += "JSON parsing error. Plant database may be corrupted.";
            break;
        case 6: //fileOperation

            message += "File access error. SD card or file system issue.";
            break;
        case 7: //SDInit

            message += "SD card initialization failed! Re-seat card, check connections, or try a different card.";
            break;
        default:

            message += "Unknown error (code " + errorCode + ").";
    }
    
    errorMessage.textContent = message;
    errorBanner.style.display = "block";
}

//hide error banner
function hideErrorBanner() {
    const errorBanner = document.getElementById("error-banner");
    if (errorBanner) {
        errorBanner.style.display = "none";
    }
}


//display error if plant is not found
function displayPlantNotFound(plantName) {

    document.getElementById("plantName").textContent = `Plant not found: ${plantName}`;
    document.getElementById("hardinessZone").textContent = "--";
    document.getElementById("lightRequirement").textContent = "--";
    document.getElementById("waterRequirement").textContent = "--";
}

//display error if the fetch request fails for some reason other than 404
function displayFetchError(error) {

    console.log("Fetch error details:", error);
    document.getElementById("plantName").textContent = "Error loading plant";
    document.getElementById("hardinessZone").textContent = "--";
    document.getElementById("lightRequirement").textContent = "--";
    document.getElementById("waterRequirement").textContent = "--";
}