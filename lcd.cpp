// ==========================================
// 4WD Delivery Robot Motor Control + LCD + Ultrasonic
// Arduino Mega 2560
// ==========================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD 주소는 보통 0x27 또는 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Motor 1: Front-Left (FL) ---
const int FL_RPWM = 2;
const int FL_LPWM = 3;
const int FL_LEN  = 22;
const int FL_REN  = 23;

// --- Motor 2: Front-Right (FR) ---
const int FR_RPWM = 4;
const int FR_LPWM = 5;
const int FR_LEN  = 24;
const int FR_REN  = 25;

// --- Motor 3: Rear-Left (RL) ---
const int RL_RPWM = 6;
const int RL_LPWM = 7;
const int RL_LEN  = 26;
const int RL_REN  = 27;

// --- Motor 4: Rear-Right (RR) ---
const int RR_RPWM = 8;
const int RR_LPWM = 9;
const int RR_LEN  = 28;
const int RR_REN  = 29;

// Ultrasonic sensor
const int TRIG_PIN = 30;
const int ECHO_PIN = 31;

int motorSpeed = 150;     // 0~255
float stopDistance = 150.0; // cm = 1.5m

bool obstacleLatched = false; // 장애물 감지 후 사용자 재입력 전까지 잠금
char currentState = 'Q';      // W A S D Q

unsigned long lastDistanceCheck = 0;
unsigned long lastStatusPrint = 0;
unsigned long lastLcdUpdate = 0;

void setup() {
  Serial.begin(9600);

  // Set motor pins
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

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // LCD
  lcd.init();
  lcd.backlight();
  showLCD("Robot Ready", "Waiting cmd");

  stopMotor();

  Serial.println("STATUS:READY");
  Serial.println("INFO:Waiting for commands W A S D Q");
}

void loop() {
  // 1) 시리얼 명령 처리
  if (Serial.available() > 0) {
    char command = Serial.read();
    handleCommand(command);
  }

  // 2) 전진 중일 때만 초음파로 전방 장애물 체크
  if (millis() - lastDistanceCheck >= 100) {
    lastDistanceCheck = millis();

    if (currentState == 'W') {
      float dist = getDistanceCM();

      if (dist > 0 && dist <= stopDistance) {
        stopMotor();
        currentState = 'Q';
        obstacleLatched = true;

        Serial.print("ALERT:OBSTACLE:");
        Serial.println(dist);

        showLCD("OBSTACLE! STOP", String(dist, 1) + " cm");
      }
    }
  }

  // 주기적으로 상태를 Flask 쪽으로 보내기
  if (millis() - lastStatusPrint >= 1000) {
    lastStatusPrint = millis();
    float dist = getDistanceCM();

    Serial.print("STATUS:");
    Serial.print(stateToText(currentState));
    Serial.print(":");
    Serial.print(dist);
    Serial.print(":");
    Serial.println(obstacleLatched ? "LOCKED" : "OK");
  }
}

void handleCommand(char command) {
  command = toupper(command);

  if (!(command == 'W' || command == 'A' || command == 'S' || command == 'D' || command == 'Q')) {
    return;
  }

  // 정지 명령은 언제나 허용
  if (command == 'Q') {
    stopMotor();
    currentState = 'Q';
    showLCD("Stopped", "Manual stop");
    Serial.println("STATUS:STOPPED");
    return;
  }

  // 장애물로 멈춘 뒤 사용자가 다시 버튼을 눌렀을 때:
  // 거리 재확인 후 안전하면 잠금 해제하고 출발
  if (obstacleLatched) {
    float dist = getDistanceCM();

    if (dist > 0 && dist <= stopDistance) {
      stopMotor();
      currentState = 'Q';

      Serial.print("ALERT:BLOCKED:");
      Serial.println(dist);

      showLCD("Still Blocked", String(dist, 1) + " cm");
      return;
    } else {
      obstacleLatched = false;
      Serial.println("INFO:Obstacle cleared by user retry");
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

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  if (duration == 0) {
    return -1; // no echo
  }

  float distance = duration * 0.0343 / 2.0;
  return distance;
}

void showLCD(String line1, String line2) {
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

// ==========================================
// Movement Functions
// ==========================================

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
