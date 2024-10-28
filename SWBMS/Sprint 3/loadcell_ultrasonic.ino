#include "HX711.h"

// Pin assignments for the HX711 (Load Cell)
#define LOADCELL_DOUT_PIN  3
#define LOADCELL_SCK_PIN   2

// Pin assignments for Ultrasonic Sensor
#define trigPin 4   // Trigger pin for the ultrasonic sensor
#define echoPin 5   // Echo pin for the ultrasonic sensor

HX711 scale;
float calibration_factor = -7050; // Adjust this for calibration

long distance;

void setup() {
  Serial.begin(9600); // Initialize serial communication
  
  // Initialize HX711 (Load Cell)
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  Serial.println("Taring the scale...");
  scale.tare(); // Reset the scale to zero
  
  // Set the ultrasonic sensor pins as outputs/inputs
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  // Read raw data from the load cell
  float rawWeight = scale.get_units(10);  // Average of 10 readings

  // Get distance from the ultrasonic sensor
  distance = getDistance(trigPin, echoPin);

  // Print raw weight
  Serial.print("Raw Weight (unscaled): ");
  Serial.print(rawWeight, 2);  
  Serial.println(" g");

  // Print distance from ultrasonic sensor
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  delay(1000); // Delay for readability
}

long getDistance(int trigPin, int echoPin) {
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Set the trigger pin high for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo pin and calculate the duration of the echo
  long duration = pulseIn(echoPin, HIGH);

  // Debug: Print the duration value
  Serial.print("Duration: ");
  Serial.println(duration);

  // Calculate the distance in centimeters
  long distance = duration * 0.034 / 2;

  return distance;
}
