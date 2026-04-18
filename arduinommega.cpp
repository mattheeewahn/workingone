// ==========================================
// 4WD Delivery Robot Motor Control (Mega 2560)
// ==========================================

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

int motorSpeed = 150; // Speed range: 0 to 255

void setup() {
  Serial.begin(9600);
  
  // Set all PWM pins as OUTPUT
  pinMode(FL_RPWM, OUTPUT); pinMode(FL_LPWM, OUTPUT);
  pinMode(FR_RPWM, OUTPUT); pinMode(FR_LPWM, OUTPUT);
  pinMode(RL_RPWM, OUTPUT); pinMode(RL_LPWM, OUTPUT);
  pinMode(RR_RPWM, OUTPUT); pinMode(RR_LPWM, OUTPUT);

  // Set all EN pins as OUTPUT
  pinMode(FL_LEN, OUTPUT); pinMode(FL_REN, OUTPUT);
  pinMode(FR_LEN, OUTPUT); pinMode(FR_REN, OUTPUT);
  pinMode(RL_LEN, OUTPUT); pinMode(RL_REN, OUTPUT);
  pinMode(RR_LEN, OUTPUT); pinMode(RR_REN, OUTPUT);

  // Enable all motor drivers
  digitalWrite(FL_LEN, HIGH); digitalWrite(FL_REN, HIGH);
  digitalWrite(FR_LEN, HIGH); digitalWrite(FR_REN, HIGH);
  digitalWrite(RL_LEN, HIGH); digitalWrite(RL_REN, HIGH);
  digitalWrite(RR_LEN, HIGH); digitalWrite(RR_REN, HIGH);
  
  Serial.println("System Ready: 4WD Mode Enabled.");
  Serial.println("Waiting for commands: W, A, S, D, Q");
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    if (command == 'w' || command == 'W') {
      Serial.println("[CMD] Forward");
      moveForward();
    } 
    else if (command == 's' || command == 'S') {
      Serial.println("[CMD] Backward");
      moveBackward();
    } 
    else if (command == 'a' || command == 'A') {
      Serial.println("[CMD] Turn Left");
      turnLeft();
    } 
    else if (command == 'd' || command == 'D') {
      Serial.println("[CMD] Turn Right");
      turnRight();
    } 
    else if (command == 'q' || command == 'Q') {
      Serial.println("[CMD] Stop");
      stopMotor();
    }
  }
}

// ==========================================
// Movement Functions for 4WD (Skid Steering)
// ==========================================

void moveForward() {
  // Left side moves forward
  analogWrite(FL_RPWM, motorSpeed); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, motorSpeed); analogWrite(RL_LPWM, 0);
  // Right side moves forward
  analogWrite(FR_RPWM, motorSpeed); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, motorSpeed); analogWrite(RR_LPWM, 0);
}

void moveBackward() {
  // Left side moves backward
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, motorSpeed);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, motorSpeed);
  // Right side moves backward
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, motorSpeed);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, motorSpeed);
}

void turnLeft() {
  // Left side moves backward, Right side moves forward
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, motorSpeed);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, motorSpeed);
  
  analogWrite(FR_RPWM, motorSpeed); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, motorSpeed); analogWrite(RR_LPWM, 0);
}

void turnRight() {
  // Left side moves forward, Right side moves backward
  analogWrite(FL_RPWM, motorSpeed); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, motorSpeed); analogWrite(RL_LPWM, 0);
  
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, motorSpeed);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, motorSpeed);
}

void stopMotor() {
  // Stop all PWM signals
  analogWrite(FL_RPWM, 0); analogWrite(FL_LPWM, 0);
  analogWrite(RL_RPWM, 0); analogWrite(RL_LPWM, 0);
  
  analogWrite(FR_RPWM, 0); analogWrite(FR_LPWM, 0);
  analogWrite(RR_RPWM, 0); analogWrite(RR_LPWM, 0);
}
