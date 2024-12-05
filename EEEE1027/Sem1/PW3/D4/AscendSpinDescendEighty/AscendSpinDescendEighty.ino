#include <util/atomic.h>
#include <string.h>
#include <LiquidCrystal.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

const int LOOP_DELAY = 15;

// // Pin definitions
// Encoders
const int
  ENCODER_LEFT = 2,
  ENCODER_RIGHT = 3;

// IR Sensors
const int
  IR_LEFT = A1,
  IR_RIGHT = A3;

// Motor controls
const int
  ENA = 5,   // Enable right-side pair of motors -- Pair A
  ENB = 6,   // Enable left-side pair of motors -- Pair B
  IN1 = 4,   // Command Pair A forwards
  IN3 = A0,  // Command Pair A backwards
  IN2 = 7,   // Command Pair B forwards
  IN4 = A2;  // Command Pair B backwards

// ------------------------------------------------
// ------------------------------------------------

// // LCD
// LCD control pins
const int
  REGISTER_SELECT = 12,
  ENABLE = 13,
  DB4 = 8,
  DB5 = 9,
  DB6 = 10,
  DB7 = 11;
// Initialize the LCD
LiquidCrystal lcd(REGISTER_SELECT, ENABLE, DB4, DB5, DB6, DB7);

// ------------------------------------------------
// ------------------------------------------------

//// Common constants
const double RAD2DEG = (double)180 / (double)PI;  // Convert whatever it's being multiplied with from radians to degrees

// ------------------------------------------------
// ------------------------------------------------

//// Distance & Time

// Distance calculation vars
float
  encoderAverage = 0.0,  // Calculated distance in centimeters
  distance = 0.0,        // Calculated distance in centimeters
  currDistTravelled = 0.0;
const int
  WHEEL_CIRCUM = 22,           // Circumference of the wheels of the line following car in centimeters
  PULSES_PER_REVOLUTION = 80;  // Number of pulses generated by the encoder wheel for one full revolution.
volatile int
  // encLeft_highCountStore = 0,
  encLeft_highCount = 0,  // Count of '1's for ENCODER_LEFT
  // encRight_highCountStore = 0,
  encRight_highCount = 0;  // Count of '1's for ENCODER_RIGHT

// Time tracking variables
unsigned long
  startTime = 0,
  elapsedTime = 0,
  currStopTime = 0;
bool inMotion = false;

// Display distance function
void displayDistance(float distance) {
  // Print distance on the 2nd row of the LCD
  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print(distance, 2);  // Print to 2 decimal places
  lcd.print(" cm   ");
}

// Display time function
void displayTime(const unsigned long time) {
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
  encLeft_highCount++; /* Increase count of encoder highs if rising edge is detected */
}
// Interrupt Service Routine (ISR) for ENCODER_RIGHT
void encoderRight_ISR() {
  encRight_highCount++; /* Increase count of encoder highs if rising edge is detected */
}

// ------------------------------------------------
// ------------------------------------------------

//// Line following/Movement

// Speeds
const int
  MAIN_SPD = 80,  // main speed of the LFC (Line Following Car)
  HI_SPD = 255,   // higher speed of the LFC
  LO_SPD = 0;     // lower speed of the LFC

// Thresholds for line detection
const int IR_THRESHOLD_RIGHT = 200;  // Threshold for the sensor on the right side
const int IR_THRESHOLD_LEFT = 460;   // Threshold for the sensor on the left side

// Movement functions
void moveForward(const int speed) {  // Move car forward
  // Speed control
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  // Direction control
  digitalWrite(IN1, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, LOW);
  // Set inMotion to indicate robot is moving
  if (inMotion == false) { inMotion = true; }
}
void moveLeft(const int speed) {
  // Speed control
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  // Direction control
  digitalWrite(IN1, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, HIGH);
  // Set inMotion to indicate robot is moving
  if (inMotion == false) { inMotion = true; }
}
void moveRight(const int speed) {
  // Speed control
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
  // Direction control
  digitalWrite(IN1, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN4, LOW);
  // Set inMotion to indicate robot is moving
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
  // Set inMotion to indicate robot is stationary
  if (inMotion == true) { inMotion = false; }
}
void lineFollowingLogic(const int normalSpeed, const int leftSensor, const int rightSensor) {
  // Neither sensor is seeing the line, move straight
  if (leftSensor > IR_THRESHOLD_LEFT && rightSensor > IR_THRESHOLD_RIGHT) { moveForward(normalSpeed); }
  // Left sensor is seeing the line, turn left
  if (leftSensor < IR_THRESHOLD_LEFT && rightSensor > IR_THRESHOLD_RIGHT) { moveLeft(HI_SPD); }
  // Right sensor is seeing the line, turn right
  if (leftSensor > IR_THRESHOLD_LEFT && rightSensor < IR_THRESHOLD_RIGHT) { moveRight(HI_SPD); }
  // Both sensors are seeing the line, stop
  if (leftSensor < IR_THRESHOLD_LEFT && rightSensor < IR_THRESHOLD_RIGHT) { moveStop(); }
}

// ------------------------------------------------
// ------------------------------------------------

//// MPU-6050 Accelerometer

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
  maxPitch = 0,  // Maximum recorded pitch angle
  minPitch = 0;  // Minimum recorded pitch angle (can be negative)

// Initialise the accelerometer
Adafruit_MPU6050 mpu;

// Function to calculate pitch from MPU accelerometer data and return the calculated value
float calcPitch(const float x, const float y, const float z) {
  // Store accelerometer data for each axis as a double
  double x_Buff = (double)x;
  double y_Buff = (double)y;
  double z_Buff = (double)z;

  // Calculate pitch in degrees from accelerometer data
  float result = atan2((-x_Buff), sqrt(y_Buff * y_Buff + z_Buff * z_Buff)) * RAD2DEG;

  return result;  // Return calculated pitch in degrees
}

// SPEED : Speed to rotate at
// ROTATE_TIMEOUT : Maximum time allowed for rotation
void rotate360(const int SPEED, const unsigned long ROTATE_TIMEOUT) {
  float amountRotated = 0;  // Variable to track the total amount rotated

  // Timekeeping variables
  unsigned long
    prevTime = millis(),
    startRotate = millis();

  // Start rotating
  moveLeft(SPEED);

  // While loop to measure the amount rotated
  while (amountRotated < 340 && (millis() - startRotate) < ROTATE_TIMEOUT) {
    lcd.setCursor(0, 0);
    lcd.print(amountRotated);
    lcd.print("     ");

    sensors_event_t a, g, temp;   // Initialise vars for getting MPU data
    mpu.getEvent(&a, &g, &temp);  // Get MPU data

    // Get raw z-axis gyroscope data and convert it to degrees per second
    double rawGyroZDegs = (double)g.gyro.z * RAD2DEG;

    // Get current time as a reference point
    unsigned long currTime = millis();

    // calculate the DT used to integrate the degrees per second data to get total degrees
    float deltaT = (float)(currTime - prevTime) / (float)1000;

    // Calculate the total amount rotated (in degrees per second) via integration
    amountRotated += rawGyroZDegs * deltaT;

    // Set the prevTime var to the current time so as to be used in the next loop iteration to calculate DT
    prevTime = currTime;

    // If the amount rotated is greater than or equal to 360,
    // Break out of the while loop
    if (amountRotated > 360) {
      break;
    }

    // Delay iteration of while loop for stability
    delay(10);
  }

  // Stop movement after the while loop has been completed
  moveStop();
}

// Function to display both the current pitch and the maximum measured pitch on the LCD
void displayAngleMax(const float pitch, const float maxp) {
  lcd.setCursor(0, 0);   // Set cursor to the first character of the first row
  lcd.print("Pitch: ");  // Print "Pitch: " indicating it is the current pitch
  lcd.print(pitch, 2);   // Print input to two decimal places
  lcd.print("DEG  ");    // Print units and clearing space

  lcd.setCursor(0, 1);  // Set cursor to the first character of the second row
  lcd.print("Max: ");   // Print "Max: " indicating it is the maximum recorded pitch
  lcd.print(maxp, 2);   // Print input to two decimal places
  lcd.print("DEG  ");   // Print units and clearing space
}
// Function to display both the current pitch and the minimum measured pitch on the LCD
void displayAngleMin(const float pitch, const float minp) {
  lcd.setCursor(0, 0);   // Set cursor to the first character of the first row
  lcd.print("Pitch: ");  // Print "Pitch: " indicating it is the current pitch
  lcd.print(pitch, 2);   // Print input to two decimal places
  lcd.print("DEG  ");    // Print units and clearing space

  lcd.setCursor(0, 1);  // Set cursor to the first character of the second row
  lcd.print("Max: ");   // Print "Max: " indicating it is the maximum recorded pitch
  lcd.print(minp, 2);   // Print input to two decimal places
  lcd.print("DEG  ");   // Print units and clearing space
}

const int RAMP_ANGLE_POS = 24;
const int RAMP_ANGLE_NEG = -(RAMP_ANGLE_POS);

// ------------------------------------------------
// ------------------------------------------------

// // Setup routine change
// Boolean flags
bool
  reachedTop = false,       // Flag indicating whether or not it has reached the top of the ramp (Event happens 1st)
  topWaitEnd = false,       // Flag indicating that the car has finished waiting for 4s at the top of the ramp (Event happens 2nd)
  hasSpun = false,          // Flag indicating whether or not it has completed the 360 degree spin (Event happens 3rd)
  rampAscent = false,       // Flag indicating whether or not it has gone up the ramp
  rampDescent = false,      // Flag indicating whether or not it has gone down the ramp
  atStartMark = false,      // Flag indicating whether or not it is correctly positioned at the 0cm start mark
  travelledEighty = false;  // Flag indicating whether or not it is correctly positioned at the 0cm start mark

const unsigned long
  TOP_WAIT_DURATION = 4000,       // Amount of time in milliseconds to wait at the top of the ramp
  STOP_PHASE_DUR = 3000;          // Amount of time in milliseconds to wait after the robot has travelled 80cm
const int RAMP_ASCENT_NUDGE = 5;  // distance in cm to travel forward by once car fully ascents the ramp in order to be in the middle of the ramp
const int START_MARK_NUDGE = 3;   // distance in cm to travel forward by so that the robot is aligned with the 0cm start mark

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
  // Compare minPitch and pitch and assign the lower value to minPitch
  minPitch = min(minPitch, pitch);

  // Disable interrupts when using the encoder high counts to do math
  // From https://docs.arduino.cc/language-reference/en/variables/variable-scope-qualifiers/volatile/#example-code
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // Calculate distance
    encoderAverage = ((float)(encLeft_highCount + encRight_highCount)) / 2.0;
    distance = ((float)encoderAverage / (float)PULSES_PER_REVOLUTION) * (float)WHEEL_CIRCUM;  // distance is in centimeters
    // distance = ((float)encLeft_highCount / PULSES_PER_REVOLUTION) * WHEEL_CIRCUM;  // distance is in centimeters
  }

  // Ascending the ramp
  if (rampAscent == false) {
    // Checking whether the maximum measured pitch is greater than the minimum ramp angle of the ramp

    displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp

    // Keep moving forward if the maximum measured pitch is less than or equal to the minimum ramp angle
    if (maxPitch <= RAMP_ANGLE_POS) {
      displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp
      moveForward(191);                  // 75% of maximum power
    }

    // If maximum measured pitch is greater than the minimum ramp angle
    // and z-axis acceleration is less than a value slightly lower than the normal raw value for level ground,
    // the car is fully on the upward slope of the ramp and
    // the car needs to get to the level ground at the top of the ramp and know its at the top
    if (maxPitch > RAMP_ANGLE_POS && accel_z <= 10.4) {
      displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp
      moveForward(191);                  // 75% of maximum power
    }

    // If maximum measured pitch is greater than the minimum ramp angle
    // and z-axis acceleration is greater than a value slightly lower than the normal raw value for level ground
    // and the currently measured pitch is less than 3 degrees upward,
    // the car has ascended the ramp and is now at the top of the ramp
    if (maxPitch > RAMP_ANGLE_POS && accel_z > 10.4 && pitch < 1.0) {
      // moveStop();
      displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp
      rampAscent = true;

      // Make a reference distance measurement to be used to
      // get the robot more onto the middle of the top of the ramp
      currDistTravelled = distance;
    }
  }

  // If distance travelled after ascending the ramp is less than the nudge distance, keep moving
  if (rampAscent == true && reachedTop == false && (distance - currDistTravelled) < RAMP_ASCENT_NUDGE) {
    displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp
    moveForward(159);
  }
  // If distance travelled after ascending the ramp is greater than or equal to the nudge distance, stop moving and set reachedTop to be true
  if (rampAscent == true && reachedTop == false && (distance - currDistTravelled) >= RAMP_ASCENT_NUDGE) {
    displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp
    reachedTop = true;
    currStopTime = elapsedTime;  // Record the time at which the car reaches the top as a reference
    moveStop();
  }

  // Wait for 4 seconds at the top of the ramp
  if (reachedTop == true && topWaitEnd == false && hasSpun == false && (elapsedTime - currStopTime) < TOP_WAIT_DURATION) {
    displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp
    moveStop();
  }
  if (reachedTop == true && topWaitEnd == false && hasSpun == false && (elapsedTime - currStopTime) >= TOP_WAIT_DURATION) {
    displayAngleMax(pitch, maxPitch);  // Display the pitch and maximum pitch while ascending the ramp``
    moveStop();
    topWaitEnd = true;
  }

  // Turning 360 degrees at the top of the ramp
  if (reachedTop == true && topWaitEnd == true && hasSpun == false) {
    rotate360(203, 5000);
    hasSpun = true;
    delay(1000);  // wait for 1 second after rotating 360 degrees
  }

  // Descending the ramp
  if (reachedTop == true && topWaitEnd == true && hasSpun == true && rampDescent == false) {
    if (minPitch >= RAMP_ANGLE_NEG) {
      moveForward(159);  // 62.5% of max power
    }

    // is now on the downwards slope of the ramp
    // needs to get to level ground and know its on level ground
    if (minPitch < RAMP_ANGLE_NEG && accel_z <= 10.4) {
      moveForward(159);  // 62.5% of max power
    }

    if (minPitch < RAMP_ANGLE_NEG && accel_z > 10.4 && pitch > -1.0) {
      moveStop();
      // Has descended the ramp
      rampDescent = true;
      // Make a reference distance measurement to be used to
      // get the front of the robot to the 0cm start mark
      currDistTravelled = distance;
    }
  }
  if (rampDescent == true && atStartMark == false && (distance - currDistTravelled) < START_MARK_NUDGE) { moveForward(159); }
  if (rampDescent == true && atStartMark == false && (distance - currDistTravelled) >= START_MARK_NUDGE) {
    // Set flag to indicate robot is ready to start the line following logic for the rest of the track
    atStartMark == true;
    moveStop();
  }
  if (rampDescent == true && atStartMark == true) {
    moveStop();
    // Delay for 1.0s so that the TAs can note
    // where the robot is once it has descended
    delay(1000);
    // Make a reference distance measurement to be used to
    // measure the distance the robot travels
    // starting from the 0cm start mark
    currDistTravelled = distance;
  }

  // Keep following the line after descending the ramp until
  // the robot has travelled 80cm starting from the 0cm start mark
  if (atStartMark == true && travelledEighty == false && (distance - currDistTravelled) <= 80) {
    lineFollowingLogic(127, leftSensor, rightSensor);
  }
  // After the robot has travelled 80cm,
  // stop and wait for 3 seconds,
  // Set the travelledEighty flag to be true
  if (atStartMark == true && travelledEighty == false && (distance - currDistTravelled) > 80) {
    moveStop();
    delay(STOP_PHASE_DUR);
    travelledEighty = true;
  }

  // Display the pitch and maximum pitch until the robot completes the 360 degree turn at the top of the ramp
  if (hasSpun == false) {
    displayAngleMax(pitch, maxPitch);
  }
  // Display distance only after reaching the 0cm start mark
  if (inMotion == true && (atStartMark == true)) {
    displayDistance(currDistTravelled);
  }

  delay(LOOP_DELAY);  // Add a delay for stability
}
