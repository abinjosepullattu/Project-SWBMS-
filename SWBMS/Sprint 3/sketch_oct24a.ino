#include <Servo.h> 
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <SPI.h>
#include <MFRC522.h>

// Pin Definitions
#define TRIG1 4           // Ultrasonic sensor for waste level
#define ECHO1 5
#define TRIG2 6           // New ultrasonic sensor for approach detection
#define ECHO2 7
#define BUZZER 9          // Buzzer on digital pin 9
#define DOUT 3            // HX711 Data pin
#define CLK 2             // HX711 Clock pin
#define SERVOPIN 8        // Servo motor pin

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
MFRC522 mfrc522; // Initialize RFID

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

  // 7. Check if weight exceeds 5 kg
  if (weight > 1000) {
    digitalWrite(BUZZER, HIGH);  // Activate buzzer
    lcd.setCursor(0, 1);
    lcd.print("Weight Exceed!");
  } else if (wastePercentage < 100) {
    digitalWrite(BUZZER, LOW);  // Deactivate buzzer if neither condition met
  }

  // 8. Check if someone approaches the bin
  if (distance2 < approachDistance) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Your RFID");
    
    // 9. RFID Check to Open Bin
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      // Concatenate UID values into a single number
      String uidString = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        uidString += String(mfrc522.uid.uidByte[i]);
      }

      // Print concatenated UID for debugging
      Serial.print("UID: ");
      Serial.println(uidString);
      
      // Send concatenated UID to ESP for validation
      Serial.println(uidString);  // Send to ESP
      
      // Wait for ESP's response (expected response: "ACCESS_GRANTED" or "ACCESS_DENIED")
      String response = "";
      unsigned long startTime = millis();
      const unsigned long timeout = 5000; // Timeout duration in milliseconds

      while (millis() - startTime < timeout) {
        if (Serial.available() > 0) {
          char c = Serial.read();
          response += c; // Build the response string
        }
        // If response has been received, break the loop
        if (response.length() > 0) {
          break;
        }
      }

      // Check if access is granted
      if (response.equals("ACCESS_GRANTED")) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("User Identified");
        binServo.write(0);  // Open bin
        delay(5000);        // Keep open for 5 seconds
        
        // Keep the bin open while the user is present
        while (distance2 < approachDistance) {
          distance2 = measureApproachDistance();  // Read distance continuously
          delay(100); // Short delay to avoid rapid polling
        }
        
        binServo.write(180);  // Close bin when user moves away
      } else if (response.equals("ACCESS_DENIED")) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Unauthorized");
        lcd.setCursor(0, 1);
        lcd.print("Access");
      } else {
        // If no response was received within the timeout period
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("TIME'S UP!");
        delay(10000); // Display for 2 seconds
      }

      mfrc522.PICC_HaltA();
      delay(2000); // Delay to show the message before the next loop
      lcd.clear();
    }
  }

  delay(1000);  // Delay before the next loop
}

// Function to measure approach distance
int measureApproachDistance() {
  digitalWrite(TRIG2, LOW);
  delay(2);
  digitalWrite(TRIG2, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG2, LOW);
  duration2 = pulseIn(ECHO2, HIGH);
  return duration2 * 0.034 / 2; // Return distance
}
