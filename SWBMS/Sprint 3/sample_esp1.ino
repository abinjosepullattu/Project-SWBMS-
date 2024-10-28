#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Firebase project details
#define FIREBASE_HOST "smartbasket-5ec0f-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "kdLLikzdgfavS0HOf7wVsIwMF3Q0SFqvjpgjsoII"

// WiFi credentials
const char* ssid = "Johnwick";
const char* password = "12357890";

// Firebase objects
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// NTP Client objects
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 5.5 * 3600;  // Set your local timezone offset (5.5 hours for IST)
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000); // Update every 60 seconds (60000 ms)

void setup() {
    Serial.begin(9600);
  
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    // Setup Firebase configuration
    firebaseConfig.host = FIREBASE_HOST;   // Correct assignment of the host string
    firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;  // Set auth token
  
    // Initialize Firebase
    Firebase.begin(&firebaseConfig, &firebaseAuth);  // Pass by reference to Firebase.begin
    Firebase.reconnectWiFi(true);

    // Initialize NTP client
    timeClient.begin();
    timeClient.update();  // Fetch the current time
}

void loop() {
    if (Serial.available()) {
        String data = Serial.readStringUntil('\n');
        
        // Print the raw data received for debugging
        Serial.println("Received Data: " + data);
        
        // Extract RFID, waste percentage, and weight from the data
        String rfid = extractValue(data, "RFID: ", ";Waste");
        String wasteLevel = extractValue(data, ";Waste: ", ";Weight");
        String weight = extractValue(data, ";Weight: ", ";end");

        // Print extracted values for debugging
        Serial.print("Extracted RFID: ");
        Serial.println(rfid);
        Serial.print("Extracted Waste Level: ");
        Serial.println(wasteLevel);
        Serial.print("Extracted Weight: ");
        Serial.println(weight);

        // Check if any values are empty
        if (wasteLevel == "" || weight == "") {
            Serial.println("Error parsing data. Please check the input format.");
            return;
        }

        // Update NTP time to get the current time
        timeClient.update();
        String formattedTime = getFormattedTime();

        // Check if RFID is present
        String accessMessage = "Access Denied";  // Default to access denied
        
        if (rfid != "") {
            String rfidPath = "/rfid/" + rfid;
            
            // Check in Firebase if the RFID key exists
            if (Firebase.get(firebaseData, rfidPath)) {
                if (firebaseData.dataType() == "null") {
                    // RFID key doesn't exist
                    Serial.println("RFID not found in database");
                } else {
                    // RFID key exists, grant access
                    accessMessage = "Access Granted";
                    Serial.println("Access Granted: " + rfid);
                }
            } else {
                Serial.println("Error fetching RFID from Firebase: " + firebaseData.errorReason());
            }
            
            // Send access message to Arduino
            Serial.println(accessMessage);
            Serial.flush(); // Ensure the message is sent before any further processing
        }
        
        // Upload waste level and weight data to Firebase
        String dataPath = "/data/" + formattedTime; // Path with timestamp
        if (!Firebase.setString(firebaseData, dataPath + "/wasteLevel", wasteLevel)) {
            Serial.println("Error uploading waste level: " + firebaseData.errorReason());
        }
        if (!Firebase.setString(firebaseData, dataPath + "/weight", weight)) {
            Serial.println("Error uploading weight: " + firebaseData.errorReason());
        }

        // Add RFID, timestamp, and access message to history
        if (rfid != "") {
            String historyPath = "/history/" + formattedTime;
            Firebase.setString(firebaseData, historyPath + "/RFID", rfid);
            Firebase.setString(firebaseData, historyPath + "/timestamp", formattedTime);
            Firebase.setString(firebaseData, historyPath + "/accessMessage", accessMessage);
        }

        // Send acknowledgment back to Arduino
        Serial.println("OK");  // Add this line to send acknowledgment
        Serial.flush(); // Ensure the message is sent before any further processing

        // Print confirmation
        Serial.println("Data uploaded to Firebase:");
        Serial.println("Path: " + dataPath);
    }
}

// Function to extract value between two markers
String extractValue(String data, String startMarker, String endMarker) {
    int startIndex = data.indexOf(startMarker);
    if (startIndex == -1) {
        Serial.print("Start marker not found: ");
        Serial.println(startMarker);
        return "";
    }

    startIndex += startMarker.length();
    int endIndex = data.indexOf(endMarker, startIndex);

    // Debugging output to check indexes
    Serial.print("Start Index: ");
    Serial.println(startIndex);
    Serial.print("End Index: ");
    Serial.println(endIndex);

    // Extract the substring
    String extractedValue;
    if (endIndex == -1) {
        extractedValue = data.substring(startIndex);  // No end marker found
    } else {
        extractedValue = data.substring(startIndex, endIndex);  // Extract between markers
    }
    
    extractedValue.trim(); // Trim whitespace directly on extractedValue
    Serial.print("Extracted Value (before trim): ");
    Serial.println(data.substring(startIndex, endIndex == -1 ? data.length() : endIndex)); // Show raw extracted value
    Serial.print("Extracted Value (after trim): ");
    Serial.println(extractedValue); // Show trimmed extracted value
    return extractedValue;  // Return the trimmed result
}

// Function to get the formatted time (DD-MM-YYYY HH:MM)
String getFormattedTime() {
    // Get epoch time in seconds since Jan 1, 1970
    unsigned long epochTime = timeClient.getEpochTime();

    // Extract hours, minutes, day, month, and year from epoch time
    unsigned long secondsInDay = epochTime % 86400;
    int currentHour = (secondsInDay / 3600);
    int currentMinute = (secondsInDay % 3600) / 60;

    // Calculate the date (number of days since Jan 1, 1970)
    unsigned long daysSinceEpoch = epochTime / 86400;
    
    // Unix time starts on a Thursday, so add 4 to get the correct weekday
    int weekday = (daysSinceEpoch + 4) % 7;

    // Adjust for leap years and non-leap years
    unsigned long year = 1970;
    while (daysSinceEpoch >= (isLeapYear(year) ? 366 : 365)) {
        daysSinceEpoch -= (isLeapYear(year) ? 366 : 365);
        year++;
    }

    // Get the current month and day
    unsigned short monthDays[] = {31, (isLeapYear(year) ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned short month = 0;
    while (daysSinceEpoch >= monthDays[month]) {
        daysSinceEpoch -= monthDays[month];
        month++;
    }
    unsigned short day = daysSinceEpoch + 1;

    // Construct the formatted time string
    char timeString[20];
    sprintf(timeString, "%02d-%02d-%04lu %02d:%02d", day, month + 1, year, currentHour, currentMinute);
    
    return String(timeString);
}

// Function to check if a year is a leap year
bool isLeapYear(unsigned long year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}
