#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ctype.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int FL_RPWM = 2;
const int FL_LPWM = 3;
const int FL_LEN  = 22;
const int FL_REN  = 23;

const int FR_RPWM = 4;
const int FR_LPWM = 5;
const int FR_LEN  = 24;
const int FR_REN  = 25;

const int RL_RPWM = 6;
const int RL_LPWM = 7;
const int RL_LEN  = 26;
const int RL_REN  = 27;

const int RR_RPWM = 8;
const int RR_LPWM = 9;
const int RR_LEN  = 28;
const int RR_REN  = 29;

const int TRIG_PIN = 30;
const int ECHO_PIN = 31;

int motorSpeed = 150;
float stopDistance = 150.0;

bool obstacleLatched = false;
char currentState = 'Q';

unsigned long lastDistanceCheck = 0;
unsigned long lastStatusPrint = 0;

float lastDistance = 999;

String lastL1 = "";
String lastL2 = "";

void setup() {
  Serial.begin(9600);

  pinMode(FL_RPWM, OUTPUT); pinMode(FL_LPWM, OUTPUT);
  pinMode(FR_RPWM, OUTPUT); pinMode(FR_LPWM, OUTPUT);
  pinMode(RL_RPWM, OUTPUT); pinMode(RL_LPWM, OUTPUT);
  pinMode(RR_RPWM, OUTPUT); pinMode(RR_LPWM, OUTPUT);

  pinMode(FL_LEN, OUTPUT); pinMode(FL_REN, OUTPUT);
  pinMode(FR_LEN, OUTPUT); pinMode(FR_REN, OUTPUT);
  pinMode(RL_LEN, OUTPUT); pinMode(RL_REN, OUTPUT);
  pinMode(RR_LEN, OUTPUT); pinMode(RR_REN, OUTPUT);

  digitalWrite(FL_LEN, HIGH); digitalWrite(FL_REN, HIGH);
  digitalWrite(FR_LEN, HIGH); digitalWrite(FR_REN, HIGH);
  digitalWrite(RL_LEN, HIGH); digitalWrite(RL_REN, HIGH);
  digitalWrite(RR_LEN, HIGH); digitalWrite(RR_REN, HIGH);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  lcd.init();
  lcd.backlight();

  showLCD("Robot Ready", "Waiting cmd");

  stopMotor();

  Serial.println("STATUS:READY");
}

void loop() {

  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == '\n' || command == '\r') return;
    handleCommand(command);
  }

  if (millis() - lastDistanceCheck >= 100) {
    lastDistanceCheck = millis();
    lastDistance = getDistanceCM();

    if (currentState == 'W') {
      if (lastDistance > 0 && lastDistance <= stopDistance) {
        stopMotor();
        currentState = 'Q';
        obstacleLatched = true;

        Serial.print("ALERT:OBSTACLE:");
        Serial.println(lastDistance);

        showLCD("OBSTACLE! STOP", String(lastDistance, 1) + " cm");
      }
    }
  }

  if (millis() - lastStatusPrint >= 1000) {
    lastStatusPrint = millis();

    Serial.print("STATUS:");
    Serial.print(stateToText(currentState));
    Serial.print(":");
    Serial.print(lastDistance);
    Serial.print(":");
    Serial.println(obstacleLatched ? "LOCKED" : "OK");
  }
}

void handleCommand(char command) {
  command = toupper(command);

  if (!(command == 'W' || command == 'A' || command == 'S' || command == 'D' || command == 'Q')) {
    return;
  }

  if (command == 'Q') {
    stopMotor();
    currentState = 'Q';
    showLCD("Stopped", "Manual stop");
    Serial.println("STATUS:STOPPED");
    return;
  }

  if (obstacleLatched) {
    if (lastDistance > 0 && lastDistance <= stopDistance) {
      stopMotor();
      currentState = 'Q';

      Serial.print("ALERT:BLOCKED:");
      Serial.println(lastDistance);

      showLCD("Still Blocked", String(lastDistance, 1) + " cm");
      return;
    } else {
      obstacleLatched = false;
      Serial.println("INFO:Obstacle cleared");
    }
  }

  if (command == 'W') {
    moveForward();
    currentState = 'W';
    showLCD("Moving", "Forward");
    Serial.println("STATUS:FORWARD");
  }
  else if (command == 'S') {
    moveBackward();
    currentState = 'S';
    showLCD("Moving", "Backward");
    Serial.println("STATUS:BACKWARD");
  }
  else if (command == 'A') {
    turnLeft();
    currentState = 'A';
    showLCD("Turning", "Left");
    Serial.println("STATUS:LEFT");
  }
  else if (command == 'D') {
    turnRight();
    currentState = 'D';
    showLCD("Turning", "Right");
    Serial.println("STATUS:RIGHT");
  }
}

float getDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999;

  return duration * 0.0343 / 2.0;
}

void showLCD(String line1, String line2) {
  if (line1 == lastL1 && line2 == lastL2) return;

  lastL1 = line1;
  lastL2 = line2;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(trim16(line1));
  lcd.setCursor(0, 1);
  lcd.print(trim16(line2));
}

String trim16(String s) {
  if (s.length() > 16) return s.substring(0, 16);
  return s;
}

const char* stateToText(char s) {
  switch (s) {
    case 'W': return "FORWARD";
    case 'A': return "LEFT";
    case 'S': return "BACKWARD";
    case 'D': return "RIGHT";
    default:  return "STOPPED";
  }
}

void moveForward() {
  analogWrite(FL_RPWM, motorSpeed); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, motorSpeed); analogWrite(RL_LPWM, 0);
  analogWrite(FR_RPWM, motorSpeed); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, motorSpeed); analogWrite(RR_LPWM, 0);
}

void moveBackward() {
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, motorSpeed);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, motorSpeed);
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, motorSpeed);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, motorSpeed);
}

void turnLeft() {
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, motorSpeed);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, motorSpeed);
  analogWrite(FR_RPWM, motorSpeed); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, motorSpeed); analogWrite(RR_LPWM, 0);
}

void turnRight() {
  analogWrite(FL_RPWM, motorSpeed); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, motorSpeed); analogWrite(RL_LPWM, 0);
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, motorSpeed);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, motorSpeed);
}

void stopMotor() {
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, 0);
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, 0);
}
