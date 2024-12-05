#include <LiquidCrystal.h>
#include <util/atomic.h>

//// Motor/Encoder/Sensor Pin definitions
// Encoders
const int
  ENCODER_LEFT = 2,
  ENCODER_RIGHT = 3;

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

//// LCD
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
  encLeft_highCount = 0,  // Count of '1's for ENCODER_LEFT
  encLeft_highCountStore = 0,
  encRight_highCount = 0,  // Count of '1's for ENCODER_RIGHT
  encRight_highCountStore = 0;

// Time tracking variables
unsigned long
  startTime = 0,
  elapsedTime = 0;
bool inMotion = false;

//// Distance Calculations Interrupts
// Interrupt Service Routine (ISR) for ENCODER_LEFT
void encoderLeft_ISR() {
  encLeft_highCount++;  // Increase count of encoder highs if rising edge is detected
}
// Interrupt Service Routine (ISR) for ENCODER_RIGHT
void encoderRight_ISR() {
  encRight_highCount++;  // Increase count of encoder highs if rising edge is detected
}

// Display distance function
void displayDistance() {
  // Print distance on the 2nd row of the LCD
  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print(distance, 2);  // Print to 2 decimal places
  lcd.print(" cm   ");
}

// ------------------------------------------------
// ------------------------------------------------

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

const int TARGET_DIST = 80;

void setup() {
  // Set up Encoder Pin as inputs
  pinMode(ENCODER_LEFT, INPUT);
  pinMode(ENCODER_RIGHT, INPUT);

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
  lcd.begin(16, 2);     // Set up the LCD with 16 columns and 2 rows
  lcd.setCursor(0, 0);  // Set cursor to the first character of the first row

  delay(500);  // Delay for a bit to show the "MPU6050 Found" message

  startTime = millis();
}

void loop() {
  elapsedTime = millis() - startTime;

  // Disable interrupts when using the encoder high counts to do math
  // From https://docs.arduino.cc/language-reference/en/variables/variable-scope-qualifiers/volatile/#example-code
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // Calculate distance
    encoderAverage = ((float)(encLeft_highCount + encRight_highCount)) / 2.0;
    distance = ((float)encoderAverage / PULSES_PER_REVOLUTION) * WHEEL_CIRCUM;  // distance is in centimeters
    // distance = ((float)encLeft_highCount / PULSES_PER_REVOLUTION) * WHEEL_CIRCUM;  // distance is in centimeters
  }

  if (distance <= TARGET_DIST) {
    moveForward(159);
  } else {
    moveStop();
  }

  if (inMotion == true) { displayDistance(); }

  delay(20);  // Update every half second
}
