#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h> 

// Pin Definitions
#define TRIG1 4           // Ultrasonic sensor for waste level
#define ECHO1 5
#define TRIG2 6           // New ultrasonic sensor for approach detection
#define ECHO2 7
#define BUZZER 9          // Buzzer on digital pin 9
#define DOUT 3            // HX711 Data pin
#define CLK 2             // HX711 Clock pin
#define SS_PIN 10         // RFID SS pin
#define RST_PIN A0        // Change RST pin to analog pin A0
#define SERVOPIN 8        // Servo motor pin
#define RX_PIN 0          // Arduino RX for communication with ESP
#define TX_PIN 1          // Arduino TX for communication with ESP

SoftwareSerial espSerial(RX_PIN, TX_PIN);
// Function Prototype
void sendToESP(int wastePercentage, float weight, String rfid = "");

// Servo Motor
Servo binServo;

// Ultrasonic Sensor
int distance1, duration1;
int distance2, duration2; // For the new ultrasonic sensor
const int maxDistance = 20; // Distance in cm when the bin is empty
const int minDistance = 5;  // Distance in cm when the bin is full
const int approachDistance = 30; // Distance to trigger the approach detection

// Load Cell
HX711 scale;
float calibration_factor = 221369.00;  // Calibration factor for load cell

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27

// RFID
MFRC522 mfrc522(SS_PIN, RST_PIN); // Initialize RFID

// Predefined RFID UIDs
byte authorizedUID1[] = {0xB0, 0x91, 0x0F, 0x0E};
byte authorizedUID2[] = {0xCA, 0x58, 0xF0, 0x16};
const int numAuthorizedUIDs = 2;

// Setup function
void setup() {
  Serial.begin(9600);
  
  // Servo Setup
  binServo.attach(SERVOPIN);
  binServo.write(180);  // Bin closed by default
  
  // Ultrasonic Sensor Setup
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); // Setup for the new sensor
  pinMode(ECHO2, INPUT);
  
  // Buzzer Setup
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  
  // Load Cell Setup
  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor);
  scale.tare();  // Reset the scale to zero

  // LCD Setup
  lcd.begin();
  lcd.backlight();
  
  // RFID Setup
  SPI.begin();
  mfrc522.PCD_Init();  // Init MFRC522
  
  // Display initial message
  lcd.setCursor(0, 0);
  lcd.print("Smart Bin Ready");
  delay(2000);
  lcd.clear();
}

void loop() {
  // 1. Measure waste level using Ultrasonic Sensor 1
  digitalWrite(TRIG1, LOW);
  delay(2);
  digitalWrite(TRIG1, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG1, LOW);
  duration1 = pulseIn(ECHO1, HIGH);
  distance1 = duration1 * 0.034 / 2;
  
  // 2. Measure distance from Ultrasonic Sensor 2 (Approach detection)
  digitalWrite(TRIG2, LOW);
  delay(2);
  digitalWrite(TRIG2, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG2, LOW);
  duration2 = pulseIn(ECHO2, HIGH);
  distance2 = duration2 * 0.034 / 2;

  // 3. Calculate waste percentage
  int wastePercentage = map(distance1, maxDistance, minDistance, 0, 100);
  wastePercentage = constrain(wastePercentage, 0, 100);

  // 4. Display waste level on LCD
  lcd.setCursor(0, 0);
  lcd.print("Waste Level: ");
  lcd.print(wastePercentage);
  lcd.print("%");
  
  // 5. Check if bin is full or 90% full
  if (wastePercentage >= 100) {
    digitalWrite(BUZZER, HIGH);  // Activate buzzer
    lcd.setCursor(0, 1);
    lcd.print("Bin is FULL!");
  } else if (wastePercentage >= 90) {
    digitalWrite(BUZZER, HIGH);  // Activate buzzer at 90%
    lcd.setCursor(0, 1);
    lcd.print("Bin 90% Full!");
  } else {
    digitalWrite(BUZZER, LOW);
  }

  // 6. Read and display weight from load cell
  float weight = scale.get_units() * 1000;  // Weight in grams
  if (weight < 0) weight = 0;  // Adjust negative weights to zero

  lcd.setCursor(0, 1);
  lcd.print("Weight: ");
  lcd.print(weight / 1000);  // Convert to kg
  lcd.print(" kg");

  // 7. Send data to ESP (RFID, waste level, and weight)
  sendToESP(wastePercentage, weight);
  
  // 8. Check if someone approaches the bin
  if (distance2 < approachDistance) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Your RFID");
    
    // 9. RFID Check and pass it to ESP
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String rfidUID = getRFIDString(mfrc522.uid.uidByte, mfrc522.uid.size);
      sendToESP(wastePercentage, weight, rfidUID);
      
      // Wait for ESP's response
      if (Serial.available() > 0) {
        String response = Serial.readString();
        if (response.indexOf("access granted") >= 0) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Access Granted");
          binServo.write(0);  // Open bin
          delay(5000);        // Keep open for 5 seconds
          
          // Keep the bin open while the user is present
          while (distance2 < approachDistance) {
            distance2 = measureApproachDistance();  // Read distance continuously
            delay(100); // Short delay to avoid rapid polling
          }
          
          binServo.write(180);  // Close bin when user moves away
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Access Denied");
        }
      }

      mfrc522.PICC_HaltA();
      delay(2000); // Delay to show the message before the next loop
      lcd.clear();
    }
  }

  delay(1000);  // Delay before the next loop
}

// Function to compare two UIDs and return a string format
String getRFIDString(byte *uid, byte uidSize) {
  String rfidStr = "";
  for (byte i = 0; i < uidSize; i++) {
    rfidStr += String(uid[i], HEX);
  }
  return rfidStr;
}

// Function to send data to ESP and wait for acknowledgment
void sendToESP(int wastePercentage, float weight, String rfid = "") {
  String message = "RFID: " + rfid + ";waste: " + String(wastePercentage) + ";weight: " + String(weight) + ";end";
  Serial.println(message);
  
  // Wait for ESP to send an acknowledgment
  unsigned long startTime = millis();
  while (!Serial.available()) {
    if (millis() - startTime > 3000) {  // Wait for 3 seconds for response
      Serial.println("ERROR: No response from ESP");
      return;
    }
  }
  
  // Read the response from ESP
  String response = Serial.readString();
  if (response.indexOf("OK") >= 0) {
    Serial.println("Data sent successfully to ESP");
  } else {
    Serial.println("ERROR: Failed to send data to ESP");
  }
}

// Function to measure approach distance
int measureApproachDistance() {
  digitalWrite(TRIG2, LOW);
  delay(2);
  digitalWrite(TRIG2, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG2, LOW);
  duration2 = pulseIn(ECHO2, HIGH);
  return duration2 * 0.034 / 2;
}
