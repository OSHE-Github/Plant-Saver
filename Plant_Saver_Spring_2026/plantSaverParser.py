from ast import dump
import requests
#get file
#make to numbers
#determine what plants to keep
#kill the bad ones
#merge files
#save to new file SLAB FORM

## HELPER FUNCTIONS
def has_key(plant, search_key):
    return any(item["key"] == search_key for item in plant["data"])

def killBad(badArray, dataPart):
    badArray = list(dict.fromkeys(badArray));
    badArray.reverse(); #reverse the array so the indexes don't break
    for index in badArray:
        dataPart.pop(index); #remove each index in the array
        #print("Bad Removed"); #for debugging
def parser(callData):
    ## START PARSING

    arrayOfBadPlants = [];
    badIndex = 0;
    index = 0;
    for plant in callData['plants']:
        #Setup Plant Viability Tracker
        eyeD = False;
        hard = False;
        light = False;
        water = False;
        sciName = False;

        if plant.get("id") is not None:
            eyeD = True;

        if plant.get("scientific_name") is not None:
            sciName = True;

        hard = has_key(plant, "USDA Hardiness zone")
        light = has_key(plant, "Light requirement")
        water = has_key(plant, "Water requirement")
    
        if False in (eyeD, hard, light, water, sciName): #keep track of bad plants to kill after fixing numbers
            arrayOfBadPlants.append(index);
     #       print(index + 1); #for debugging
     #   print("ID: ", eyeD, "Hardiness: ", hard, "Light Req: ", light, "Water Req: ", water, "Sci Name: ", sciName); #for debeggin

        ## REMOVE EXTRANEOUS INFO
        infoArray = ['created_at', 'updated_at', 'parent_id', 'adopter_id', 'version', 'type', 'link', 'images', 'description', 'slug'];
        for info in infoArray:
            plant.pop(info, None); #removes info if it exists and doesn't throw an error if it doesn't exist

        ## FIX NUMBERS
        dataIndex = 0;
        arrayOfBadData = [];
        for dataPart in plant['data']:
            match dataPart['key']:
                case "USDA Hardiness zone":
                    newPart = dataPart['value'].split('-'); #split text over dash
                    intArray = []; # make an array to keep the ints in
                    for item in newPart:
                        try:
                            item = int(item); #turn each small string into an int
                            intArray.append(item); # fill the array with ints
                        except:
                            arrayOfBadData.append(dataIndex); #at least one part of the requirement is bad so we kill the plant
                            #print("Something is strange: ", item); #for debugging
                    dataPart['value'] = sorted(intArray); #replace the old data
    #                print(dataPart['value']); #For Debugging
                case "Light requirement":
                    newPart = dataPart['value'].split(', '); #split text over comma
                    intArray = []; #make an array to keep the ints in
                    for item in newPart:
                        match item.lower(): # add an associated variable to the array for each option for light requirements
                            case "full sun":
                                intArray.append(3);
                            case "partial sun/shade":
                                intArray.append(2);
                            case "full shade":
                                intArray.append(1);
                            case _:
                                arrayOfBadData.append(dataIndex); #at least one part of the requirement is bad so we kill the plant
                                #print("Remove Light"); #for debugging
                                #print(plant); #for debugging
                    dataPart['value'] = sorted(intArray); #replace the old data
     #               print(dataPart['value']); #For Debugging
                case "Water requirement":
                    newPart = dataPart['value'].split(', '); #split text over comma
                    intArray = []; #make an array to keep the ints in
                    for item in newPart:
                        match item.lower(): # add an associated variable to the array for each option for Water requirements
                            case "dry":
                                intArray.append(4);
                            case "moist":
                                intArray.append(3);
                            case "wet":
                                intArray.append(2);
                            case "water":
                                intArray.append(1);
                            case "low":
                                intArray.append(4);
                            case _:
                                arrayOfBadData.append(dataIndex); #at least one part of the requirement is bad so we kill the plant
                                #print("Remove Water"); #for debugging
                                #print(plant); #for debugging
                    dataPart['value'] = sorted(intArray); #replace the old data
    #                print(dataPart['value']); #For Debugging
                case _:
                    arrayOfBadData.append(dataIndex);
            dataIndex = dataIndex + 1;
        killBad(arrayOfBadData, plant['data']);

        index = index + 1;
    #print(arrayOfBadPlants); #for debugging
    killBad(arrayOfBadPlants, callData['plants'])

import json
mode = input("Pull from API (a) or enter file path manually (m)? ").strip('"');

## MANUAL FILE INPUT
if mode == "m":
    ## OPEN FILE

    userFilePath = input("File Path Here: ").strip('"');

    try:
        with open(userFilePath, 'r') as file:

            try:
                callData = json.load(file)
                print("File loaded successfully.");
                print(callData);

            except json.JSONDecodeError:
                print("Error: The file is not a valid JSON.");

    except FileNotFoundError:
        print("File not found. Please check the path and try again.")
        exit()
    parser(callData);
elif mode == "a":

## CALL API
    bigData ={};
    tempList = [];
    last_id = 0;
    recent_id = None;
    params = {};
    params['last_id'] = last_id;
    headers = {
        "x-permapeople-key-id": input("Key ID: "),
        "x-permapeople-key-secret": input("Secret ID: ")
    }
    while last_id != recent_id: #call API as many times as it takes to get all plants
        params['last_id'] = last_id;
        print("Last ID: ", last_id);
        print("Recent ID: ", recent_id);
        print()
        call = requests.get("https://permapeople.org/api/plants", headers = headers, params = params);
        if call.status_code == 200:
            callData = call.json();
            print("API Called successfully!");
        else:
            print("Request Failed");
            exit();
        tempList.extend(callData['plants']);
        #print(tempList[0]['id'], tempList[-1]['id']); #for debugging
        #print(tempList); #for debugging
        #print(tempList[-1]['id']) #for debugging
        recent_id = last_id;
        last_id = tempList[-1]['id'];

    bigData['plants'] = tempList;
    #print(bigData); #for debugging
    parser(bigData);

## SAVE FILE
mode = input("Save to file? (y) (n) ").strip('"');
if mode == "y":
    saveLocation = input("File path: ").strip('"');
    file = open (saveLocation, "w")
    json.dump(bigData, file, indent = 4)
elif mode == "n":
    exit();
else:
    exit();
