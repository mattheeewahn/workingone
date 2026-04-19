// ==========================================
// 4WD Delivery Robot Motor Control + Ultrasonic Stop
// Arduino Mega 2560
// ==========================================

// -----------------------------
// Motor 1: Front-Left (FL)
// -----------------------------
const int FL_RPWM = 2;
const int FL_LPWM = 3;
const int FL_LEN  = 22;
const int FL_REN  = 23;

// -----------------------------
// Motor 2: Front-Right (FR)
// -----------------------------
const int FR_RPWM = 4;
const int FR_LPWM = 5;
const int FR_LEN  = 24;
const int FR_REN  = 25;

// -----------------------------
// Motor 3: Rear-Left (RL)
// -----------------------------
const int RL_RPWM = 6;
const int RL_LPWM = 7;
const int RL_LEN  = 26;
const int RL_REN  = 27;

// -----------------------------
// Motor 4: Rear-Right (RR)
// -----------------------------
const int RR_RPWM = 8;
const int RR_LPWM = 9;
const int RR_LEN  = 28;
const int RR_REN  = 29;

// -----------------------------
// Ultrasonic Sensor (HC-SR04)
// 원하는 핀으로 바꿔도 됨
// -----------------------------
const int TRIG_PIN = 30;
const int ECHO_PIN = 31;

// -----------------------------
// Settings
// -----------------------------
int motorSpeed = 150;              // 0 ~ 255
const int STOP_DISTANCE_CM = 100;  // 100cm 이하이면 정지

// -----------------------------
// State
// -----------------------------
char currentMotion = 'Q';   // W, A, S, D, Q
bool obstacleLock = false;  // 장애물 때문에 잠긴 상태
unsigned long lastStatusMs = 0;

// ==========================================
// Setup
// ==========================================
void setup() {
  Serial.begin(9600);

  // Motor PWM pins
  pinMode(FL_RPWM, OUTPUT); pinMode(FL_LPWM, OUTPUT);
  pinMode(FR_RPWM, OUTPUT); pinMode(FR_LPWM, OUTPUT);
  pinMode(RL_RPWM, OUTPUT); pinMode(RL_LPWM, OUTPUT);
  pinMode(RR_RPWM, OUTPUT); pinMode(RR_LPWM, OUTPUT);

  // Motor enable pins
  pinMode(FL_LEN, OUTPUT); pinMode(FL_REN, OUTPUT);
  pinMode(FR_LEN, OUTPUT); pinMode(FR_REN, OUTPUT);
  pinMode(RL_LEN, OUTPUT); pinMode(RL_REN, OUTPUT);
  pinMode(RR_LEN, OUTPUT); pinMode(RR_REN, OUTPUT);

  // Enable all drivers
  digitalWrite(FL_LEN, HIGH); digitalWrite(FL_REN, HIGH);
  digitalWrite(FR_LEN, HIGH); digitalWrite(FR_REN, HIGH);
  digitalWrite(RL_LEN, HIGH); digitalWrite(RL_REN, HIGH);
  digitalWrite(RR_LEN, HIGH); digitalWrite(RR_REN, HIGH);

  // Ultrasonic pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  stopMotor();

  Serial.println("System Ready: 4WD + Ultrasonic Stop Enabled.");
  Serial.println("Commands: W, A, S, D, Q");
  sendStatus();
}

// ==========================================
// Main Loop
// ==========================================
void loop() {
  float distance = getDistanceCM();

  // 1) 움직이는 중 장애물 감지되면 즉시 정지 + 잠금
  if (isMovingCommand(currentMotion) && distance > 0 && distance <= STOP_DISTANCE_CM) {
    stopMotor();
    currentMotion = 'Q';
    obstacleLock = true;

    Serial.print("ALERT:OBSTACLE:");
    Serial.println(distance, 1);

    sendStatus();
  }

  // 2) 시리얼 명령 처리
  if (Serial.available() > 0) {
    char command = Serial.read();

    if (command == 'w' || command == 'W' ||
        command == 'a' || command == 'A' ||
        command == 's' || command == 'S' ||
        command == 'd' || command == 'D') {

      // 이동 명령 다시 눌렀을 때만 재시도
      handleMoveCommand(toupper(command));
    }
    else if (command == 'q' || command == 'Q') {
      obstacleLock = false;   // 수동 정지는 lock 해제
      currentMotion = 'Q';
      stopMotor();
      Serial.println("[CMD] Stop");
      sendStatus();
    }
  }

  // 3) 잠금 상태인데 장애물이 치워졌는지 안내
  if (obstacleLock && distance > STOP_DISTANCE_CM) {
    // lock은 자동 해제 안 함
    // 사용자가 다시 버튼 눌러야 움직이게 함
    // 다만 정보는 알려줌
    static bool clearedOnce = false;
    if (!clearedOnce) {
      Serial.println("INFO:Obstacle cleared");
      clearedOnce = true;
    }
  } else {
    static bool dummy = false;
    dummy = false; // 의미 없는 경고 방지용 아님
  }

  // 4) 주기적으로 상태 전송
  if (millis() - lastStatusMs >= 300) {
    sendStatus();
    lastStatusMs = millis();
  }

  delay(30);
}

// ==========================================
// Command Handling
// ==========================================
void handleMoveCommand(char command) {
  float distance = getDistanceCM();

  // 장애물이 아직 가까우면 계속 막음
  if (distance > 0 && distance <= STOP_DISTANCE_CM) {
    stopMotor();
    currentMotion = 'Q';
    obstacleLock = true;

    Serial.print("ALERT:BLOCKED:");
    Serial.println(distance, 1);

    sendStatus();
    return;
  }

  // 장애물이 없으면 lock 해제 후 이동
  obstacleLock = false;

  if (command == 'W') {
    Serial.println("[CMD] Forward");
    moveForward();
    currentMotion = 'W';
  }
  else if (command == 'S') {
    Serial.println("[CMD] Backward");
    moveBackward();
    currentMotion = 'S';
  }
  else if (command == 'A') {
    Serial.println("[CMD] Turn Left");
    turnLeft();
    currentMotion = 'A';
  }
  else if (command == 'D') {
    Serial.println("[CMD] Turn Right");
    turnRight();
    currentMotion = 'D';
  }

  sendStatus();
}

// ==========================================
// Distance Measurement
// ==========================================
float getDistanceCM() {
  // 초음파 트리거
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // timeout 30ms -> 약 5m 정도
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  // 측정 실패
  if (duration == 0) {
    return -1;
  }

  float distance = duration * 0.0343 / 2.0;
  return distance;
}

// ==========================================
// Status / Helper
// ==========================================
bool isMovingCommand(char c) {
  return (c == 'W' || c == 'A' || c == 'S' || c == 'D');
}

const char* motionToText(char c) {
  switch (c) {
    case 'W': return "FORWARD";
    case 'S': return "BACKWARD";
    case 'A': return "LEFT";
    case 'D': return "RIGHT";
    default:  return "STOPPED";
  }
}

void sendStatus() {
  float d = getDistanceCM();

  Serial.print("STATUS:");
  Serial.print(motionToText(currentMotion));
  Serial.print(":");

  if (d < 0) Serial.print("-1");
  else Serial.print(d, 1);

  Serial.print(":");
  if (obstacleLock) Serial.println("LOCKED");
  else Serial.println("UNLOCKED");
}

// ==========================================
// Movement Functions
// ==========================================
void moveForward() {
  // Left side forward
  analogWrite(FL_RPWM, motorSpeed); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, motorSpeed); analogWrite(RL_LPWM, 0);

  // Right side forward
  analogWrite(FR_RPWM, motorSpeed); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, motorSpeed); analogWrite(RR_LPWM, 0);
}

void moveBackward() {
  // Left side backward
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, motorSpeed);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, motorSpeed);

  // Right side backward
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, motorSpeed);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, motorSpeed);
}

void turnLeft() {
  // Left side backward, Right side forward
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, motorSpeed);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, motorSpeed);

  analogWrite(FR_RPWM, motorSpeed); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, motorSpeed); analogWrite(RR_LPWM, 0);
}

void turnRight() {
  // Left side forward, Right side backward
  analogWrite(FL_RPWM, motorSpeed); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, motorSpeed); analogWrite(RL_LPWM, 0);

  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, motorSpeed);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, motorSpeed);
}

void stopMotor() {
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, 0);
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, 0);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, 0);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, 0);
}
