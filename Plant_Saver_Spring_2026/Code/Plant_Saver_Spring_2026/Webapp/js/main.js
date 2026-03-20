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

    //start polling device for current selection (updates UI when OLED selection changes)
    setInterval(pollCurrentPlant, 1000);
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

    
    document.getElementById("waterData").textContent = `${plantData.avgWater}`;
    document.getElementById("lightData").textContent = `${plantData.avgLight}`;
    document.getElementById("tempData").textContent = `${plantData.avgTemp}`;
    document.getElementById("humidityData").textContent = `${plantData.avgHumidity}`;
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