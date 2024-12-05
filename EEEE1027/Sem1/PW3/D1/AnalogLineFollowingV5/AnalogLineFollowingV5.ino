#include <string.h>
#include <LiquidCrystal.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

const int loopDelay = 30;

// // Pin definitions
// Encoders
const int
  ENCODER_LEFT = A3,
  ENCODER_RIGHT = 2;

// IR Sensors
const int
  IR_LEFT = A4,
  IR_RIGHT = A5;

// Motor controls
const int
  ENA = 11,  // Enable right-side pair of motors -- Pair A
  ENB = 3,   // Enable left-side pair of motors -- Pair B
  IN1 = A1,  // Command Pair A forwards
  IN3 = 12,  // Command Pair A backwards
  IN2 = A2,  // Command Pair B forwards
  IN4 = 13;  //Command Pair B backwards

// ------------------------------------------------
// ------------------------------------------------

// // LCD
// LCD control pins
const int
  REGISTER_SELECT = 8,
  ENABLE = 9,
  DB4 = 4,
  DB5 = 5,
  DB6 = 6,
  DB7 = 7;
// Initialize the LCD
LiquidCrystal lcd(REGISTER_SELECT, ENABLE, DB4, DB5, DB6, DB7);

// ------------------------------------------------
// ------------------------------------------------

//// Distance & Time

// Variables for measuring distance traveled
// Minimum distance = 955 cm
// Maximum distance = 993 cm
float distance = 0.0;  // Calculated distance in centimeters
const float
  WHEEL_CIRCUM = 20.46,          // Circumference of the wheels of the line following car in centimeters
  PULSES_PER_REVOLUTION = 40.0;  // Number of pulses generated by the encoder wheel for one full revolution.
volatile int
  encLeft_highCount = 0,   // Count of '1's for ENCODER_LEFT
  encRight_highCount = 0;  // Count of '1's for ENCODER_RIGHT

// Time tracking variables
unsigned long startTime = 0;
volatile unsigned long elapsedTime = 0;
bool inMotion = false;

// Display distance function
void displayDistance() {
  // Print distance on the 2nd row of the LCD
  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print(distance, 2);  // Print to 2 decimal places
  lcd.print(" cm   ");
}

// Display time function
void displayTime(volatile unsigned long time) {
  // Calculate time in seconds
  const unsigned long seconds = time / 1000;

  // Print time on the 1st row of the LCD
  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  lcd.print(seconds);
  lcd.print("s     ");
}

//// Distance Calculations Interrupts
// Interrupt Service Routine (ISR) for ENCODER_LEFT
void encoderLeft_ISR() {
  encLeft_highCount++;  // Increase count of encoder highs if rising edge is detected
}
// Interrupt Service Routine (ISR) for ENCODER_RIGHT
void encoderRight_ISR() {
  encRight_highCount++;  // Increase count of encoder highs if rising edge is detected
}

// ------------------------------------------------
// ------------------------------------------------

//// Line following
// Speeds
const int
  mainSpeed = 80,   // main speed of the LFC (Line Following Car)
  highSpeed = 255,  // higher speed of the LFC
  lowSpeed = 0;     // lower speed of the LFC

const int IR_THRESHOLD = 200;  // Threshold for line detection

// Movement functions
void moveForward(int speed) {
  // Speed control
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  // Direction control
  digitalWrite(IN1, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, LOW);

  if (inMotion == false) { inMotion = true; }
}
void moveLeft() {
  // Speed control
  analogWrite(ENA, highSpeed);
  analogWrite(ENB, highSpeed);
  // Direction control
  digitalWrite(IN1, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, HIGH);

  if (inMotion == false) { inMotion = true; }
}
void moveRight() {
  // Speed control
  analogWrite(ENA, highSpeed);
  analogWrite(ENB, highSpeed);
  // Direction control
  digitalWrite(IN1, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN4, LOW);

  if (inMotion == false) { inMotion = true; }
}
void moveStop() {
  // Speed control
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  // Direction control
  digitalWrite(IN1, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, LOW);

  if (inMotion == true) { inMotion = false; }
}
void lineFollowingLogic(int normalSpeed, int leftSensor, int rightSensor) {
  // Neither sensor is seeing the line, move straight
  if (leftSensor > IR_THRESHOLD && rightSensor > IR_THRESHOLD) {
    moveForward(normalSpeed);
  }
  // Left sensor is seeing the line, turn left
  if (leftSensor < IR_THRESHOLD && rightSensor > IR_THRESHOLD) {
    moveLeft();
  }
  // Right sensor is seeing the line, turn right
  if (leftSensor > IR_THRESHOLD && rightSensor < IR_THRESHOLD) {
    moveRight();
  }
  // Both sensors are seeing the line, stop
  if (leftSensor < IR_THRESHOLD && rightSensor < IR_THRESHOLD) {
    moveStop();
  }
}

// ------------------------------------------------
// ------------------------------------------------

// // MPU-6050 Accelerometer

// Var definitions
// Vars for storing raw accelerometer data
float
  accel_x,  // X-axis raw acceleration data
  accel_y,  // Y-axis raw acceleration data
  accel_z;  // Z-axis raw acceleration data

// Vars for storing calculated roll and pitch
double
  roll = 0,      // Calculated roll angle
  pitch = 0,     // Calculated pitch angle
  maxPitch = 0;  // Maximum recorded pitch angle

// String vars for LCD printing
char messageSer[15] = "";  // Final message to be printed to the serial monitor
char message[15] = "";     // Final message to be printed on LCD
char angle[10] = "";       // Buffer for string representation of the pitch angle

// Initialise the accelerometer
Adafruit_MPU6050 mpu;

// Function to calculate pitch from MPU accelerometer data and return the calculated value
float calcPitch(const float x, const float y, const float z) {
  // Store accelerometer data for each axis as a double
  double x_Buff = (double)x;
  double y_Buff = (double)y;
  double z_Buff = (double)z;

  double rad2deg = (double)180 / (double)PI;                                           // Convert the output of the formula used to degrees
  float result = atan2((-x_Buff), sqrt(y_Buff * y_Buff + z_Buff * z_Buff)) * rad2deg;  // Calculate pitch in degrees from accelerometer data

  return result;  // Return calculated pitch in degrees
}

// The initial distance in centimeters for which the robot should always just keep going forward and following the line
float iniForwDist = 35;

// Function to display the ramp angle on the LCD
void displayAngle(const float angle, const float max) {
  lcd.setCursor(0, 0);   // Set cursor to the first character of the first row
  lcd.print("Angle: ");  // Print "Angle: " indicating it is the current angle
  lcd.print(angle, 2);   // Print input to two decimal places
  lcd.print("DEG   ");   // Print units and clearing space

  lcd.setCursor(0, 1);  // Set cursor to the first character of the second row
  lcd.print("Max: ");   // Print "Max: " indicating it is the maximum recorded angle
  lcd.print(max, 2);    // Print input to two decimal places
  lcd.print("DEG   ");  // Print units and clearing space
}

// ------------------------------------------------
// ------------------------------------------------

// // Setup routine change
// Boolean flags
bool
  reachedTop = false,  // Flag indicating whether or not it has reached the top of the ramp
  hasSpun = false,     // Flag indicating whether or not it has completed the 360 degree spin
  topWaitEnd = false;

// The amount of time in ms that z accel has to be constant to count as having reach the top of the ramp
const int waitInterval = 500;

unsigned long reachedTopTime, fourSecWaitEnd;
unsigned long turnTime = 1250;

float prevZAccel = 0;

// ------------------------------------------------
// ------------------------------------------------

void setup() {
  // Set up Encoder Pin as inputs
  pinMode(ENCODER_LEFT, INPUT);
  pinMode(ENCODER_RIGHT, INPUT);

  // Set up IR sensor pins as inputs
  pinMode(IR_LEFT, INPUT);
  pinMode(IR_RIGHT, INPUT);

  // Set up motor pins as outputs
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Attach interrupt to the encoder pins on the rising edge
  attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT), encoderLeft_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_RIGHT), encoderRight_ISR, RISING);

  // Initialize the LCD
  lcd.begin(16, 2);              // Set up the LCD with 16 columns and 2 rows
  lcd.setCursor(0, 0);           // Set cursor to the first character of the first row
  lcd.print("Looking for MPU");  // Print message about looking for MPU
  delay(400);                    // Delay for a bit to show the "Looking for MPU message"

  // Start Serial Comms
  Serial.begin(115200);
  // Clear lines to indicate new serial data
  Serial.println("");
  Serial.println("");

  // Try to initialize MPU
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found");
  Serial.println("");          // Clear line to seperate MPU status print from data print
  lcd.print("MPU6050 Found");  // Indicate on LCD that MPU has been found

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);  // set accelerometer range to +-16G
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);        // set gyro range to +- 500 deg/s
  mpu.setFilterBandwidth(MPU6050_BAND_10_HZ);     // set filter bandwidth to 10 Hz

  delay(500);  // Delay for a bit to show the "MPU6050 Found" message

  lcd.print("Starting run...");  // Print message about starting movement

  delay(500);   // Wait a little before starting to move
  lcd.clear();  // Clear LCD
  // lcd.setCursor(0, 0);       // Set cursor to the first character of the first row
  // lcd.print("Ramp Angle:");  // Print "Ramp Angle" text on the first line

  // Set Starting time of run
  startTime = millis();
}

void loop() {
  // Calculate the amount of time that has passed
  elapsedTime = millis() - startTime;

  // Read IR sensor values
  int leftSensor = analogRead(IR_LEFT);
  int rightSensor = analogRead(IR_RIGHT);

  sensors_event_t a, g, temp;   // Initialise vars for getting MPU data
  mpu.getEvent(&a, &g, &temp);  // Get MPU data

  accel_x = a.acceleration.x;  // Get acceleration in the x-axis
  accel_y = a.acceleration.y;  // Get acceleration in the y-axis
  accel_z = a.acceleration.z;  // Get acceleration in the z-axis

  // Calculate pitch from MPU accelerometer data
  pitch = calcPitch(accel_x, accel_y, accel_z);

  // Compare maxPitch and pitch and assign the higher value to maxPitch
  maxPitch = max(maxPitch, pitch);

  // Calculate distance
  float encoderAverage = ((float)(encLeft_highCount + encRight_highCount)) / 2.0;
  distance = ((float)encoderAverage / PULSES_PER_REVOLUTION) * WHEEL_CIRCUM;  // distance is in centimeters

  // Initial movement routine before reaching the start of the ramp
  if (distance < iniForwDist) {
    lineFollowingLogic(255, leftSensor, rightSensor);
    displayAngle(pitch, maxPitch);
  }
  // Movement routine between reaching the start of the ramp and reaching the top of the ramp
  if (distance > iniForwDist && reachedTop == false && hasSpun == false) {
    int zAccelSameCnt = 0;
    lineFollowingLogic(245, leftSensor, rightSensor);
    displayAngle(pitch, maxPitch);

    if (abs(prevZAccel - accel_z) < 0.05 && accel_z > 10.0) {
      zAccelSameCnt++;
    }
    if (zAccelSameCnt >= (waitInterval / loopDelay)) {
      reachedTopTime = millis();
      reachedTop = true;
    }
  }

  if (reachedTop == true && hasSpun == false && topWaitEnd == false && (millis() - reachedTopTime) <= 4000) {
    moveStop();
    displayAngle(pitch, maxPitch);
    fourSecWaitEnd = millis();
    topWaitEnd = true;
  }

  if (reachedTop == true && hasSpun == false && topWaitEnd == true) {
    // Do the 360
  }

  // Do the line following thing
  // lineFollowingLogic();

  if (inMotion == true) {
    // Print distance & time
    // displayTime(elapsedTime);
    // displayDistance();

    // Print ramp angle
    // displayAngle(pitch, maxPitch);
  }

  delay(loopDelay);  // Adjust the delay for smoother updates
}