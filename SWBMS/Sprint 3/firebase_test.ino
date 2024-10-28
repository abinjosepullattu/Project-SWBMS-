#include <dummy.h>

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

// Wi-Fi credentials
#define WIFI_SSID "John Wick Reloaded"
#define WIFI_PASSWORD "Abin@1235789"

// Firebase credentials
#define FIREBASE_HOST "smart-waste-readings-default-rtdb.firebaseio.com"  // Firebase database URL
#define FIREBASE_AUTH "Rbbr5GGAB4hxbacJnuAd2bnRLZb7zBnWFs5niXDi"  // Firebase secret key

// Initialize Firebase
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;  // Create FirebaseConfig object
FirebaseAuth firebaseAuth;       // Create FirebaseAuth object

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to Wi-Fi");
}

void setup() {
  Serial.begin(9600);

  // Connect to Wi-Fi
  connectWiFi();

  // Set Firebase configuration
  firebaseConfig.database_url = FIREBASE_HOST;  // Set the Firebase host

  // Set the Firebase auth token directly
  firebaseAuth.token = FIREBASE_AUTH; 

  // Initialize Firebase
  Firebase.begin(&firebaseConfig, &firebaseAuth); // Pass the config and auth objects
  Firebase.reconnectWiFi(true);
}

void loop() {
  // Example data: sending waste level and weight data
  int wastePercentage = 50;  // Replace with actual data
  float weight = 2.5;        // Replace with actual data in kg

  // Sending waste percentage to Firebase
  if (Firebase.setInt(firebaseData, "/SmartBin/WasteLevel", wastePercentage)) {
    Serial.println("Waste level updated in Firebase");
  } else {
    Serial.println("Failed to update waste level: " + firebaseData.errorReason());
  }

  // Sending weight to Firebase
  if (Firebase.setFloat(firebaseData, "/SmartBin/Weight", weight)) {
    Serial.println("Weight updated in Firebase");
  } else {
    Serial.println("Failed to update weight: " + firebaseData.errorReason());
  }

  delay(10000);  // Update every 10 seconds
}
