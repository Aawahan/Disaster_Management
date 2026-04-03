#include <Wire.h>
#include <MPU6050.h>
#include <Servo.h>
#include <DHT.h>
#include <math.h>

// -------- MPU --------
MPU6050 mpu;

// -------- DHT --------
#define DHTPIN A1
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// -------- SERVOS --------
Servo scanServo;
Servo pumpServo;

#define SERVO_SCAN 13
#define SERVO_PUMP A4

// -------- RELAY --------
#define RELAY_PIN A2

// -------- SPEAKER --------
#define SPEAKER_PIN A3

// -------- ULTRASONIC --------
#define TRIG_LEFT 2
#define ECHO_LEFT 3
#define TRIG_RIGHT 4
#define ECHO_RIGHT 5

// -------- SENSORS --------
#define FLAME_PIN 6
#define MQ135_PIN A0

// -------- MOTORS --------
#define IN1 8
#define IN2 9
#define IN3 10
#define IN4 11
#define ENA 7
#define ENB 12

// -------- VARIABLES --------
float temperature, humidity, airPPM;
char command = 'S';

float roll, pitch, yaw;

// distance (approx)
float velocity = 0;
float distanceTravelled = 0;
unsigned long prevTime = 0;

// modes
String roverMode = "MANUAL";
String pumpMode = "AUTO";

bool pumpState = false;
String currentSound = "";

// servo scan
int scanPos = 45;
int scanDir = 1;
unsigned long lastScan = 0;
int targetAngle = 90;

// -------- FUNCTIONS --------

// Ultrasonic
int getDistance(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(5);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 30000);
  if (duration == 0) return 400;

  int dist = duration * 0.034 / 2;
  if (dist < 2 || dist > 400) return 400;

  return dist;
}

// MQ135
float readMQ135PPM() {
  int adc = analogRead(MQ135_PIN);
  float voltage = adc * (5.0 / 1023.0);
  if (voltage <= 0.1) return 0;

  float Rs = (5.0 - voltage) / voltage;
  float ratio = Rs / 10.0;
  return 116.6 * pow(ratio, -2.76);
}

// MPU
void updateMPU() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  roll = atan2(ay, az) * 180 / PI;
  pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180 / PI;
  yaw += gz / 131.0 * 0.02;

  float accel = sqrt(ax*ax + ay*ay + az*az)/16384.0 - 1.0;

  unsigned long t = millis();
  float dt = (t - prevTime)/1000.0;
  prevTime = t;

  velocity += accel * dt;
  distanceTravelled += velocity * dt;

  if (abs(velocity) < 0.01) velocity = 0;
}

// 🔊 SOUND
void playSound() {
  if (currentSound == "alert") {
    tone(SPEAKER_PIN, 1000);
  } 
  else if (currentSound == "horn") {
    tone(SPEAKER_PIN, 600);
  } 
  else {
    noTone(SPEAKER_PIN);
  }
}

// 🔥 FIRE TRACKING (NO JITTER)
void fireTracking(int flame) {

  if (flame == LOW) {
    targetAngle = scanPos;
    pumpServo.write(targetAngle);

    currentSound = "alert";

    if (pumpMode == "AUTO") {
      digitalWrite(RELAY_PIN, HIGH);
    }

  } else {

    if (millis() - lastScan > 30) {
      lastScan = millis();

      scanPos += scanDir * 5;

      if (scanPos >= 135 || scanPos <= 45) {
        scanDir *= -1;
      }

      scanServo.write(scanPos);
      pumpServo.write(scanPos);
    }

    if (pumpMode == "AUTO") {
      digitalWrite(RELAY_PIN, LOW);
    }
  }
}

// 🤖 AUTO NAVIGATION
void autoNavigation(int leftDist, int rightDist, int flame) {

  if (flame == LOW) {
    stopMotors();
    fireTracking(flame);
    return;
  }

  if (leftDist < 20 && rightDist < 20) {
    moveBackward();
    delay(150);
    turnRight();
  }
  else if (leftDist < 20) {
    turnRight();
  }
  else if (rightDist < 20) {
    turnLeft();
  }
  else {
    moveForward();
  }
}

// MOTOR
void moveForward(){ digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);}
void moveBackward(){ digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH); digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);}
void turnLeft(){ digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);}
void turnRight(){ digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);}
void stopMotors(){ digitalWrite(IN1,LOW); digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);}

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  Wire.begin();
  mpu.initialize();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(FLAME_PIN, INPUT);

  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  analogWrite(ENA, 150);
  analogWrite(ENB, 150);

  scanServo.attach(SERVO_SCAN);
  pumpServo.attach(SERVO_PUMP);

  dht.begin();
  prevTime = millis();
}

// -------- LOOP --------
void loop() {

  // SERIAL COMMANDS
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');

    if (cmd == "AUTO") roverMode = "AUTO";
    else if (cmd == "MANUAL") roverMode = "MANUAL";

    else if (cmd == "F") command = 'F';
    else if (cmd == "B") command = 'B';
    else if (cmd == "L") command = 'L';
    else if (cmd == "R") command = 'R';
    else if (cmd == "S") command = 'S';

    else if (cmd == "P1"){ pumpMode="MANUAL"; pumpState=true; }
    else if (cmd == "P0"){ pumpMode="MANUAL"; pumpState=false; }
    else if (cmd == "A"){ pumpMode="AUTO"; }

    else if (cmd.startsWith("SOUND:")){
      currentSound = cmd.substring(6);
    }
  }

  // SENSOR READ
  int leftDist = getDistance(TRIG_LEFT, ECHO_LEFT);
  int rightDist = getDistance(TRIG_RIGHT, ECHO_RIGHT);

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  airPPM = readMQ135PPM();

  int flame = digitalRead(FLAME_PIN);

  updateMPU();

  // SAFETY STOP
  if (flame == LOW) {
    stopMotors();
  }

  // CONTROL
  if (roverMode == "AUTO") {
    autoNavigation(leftDist, rightDist, flame);
  } else {
    switch(command){
      case 'F': moveForward(); break;
      case 'B': moveBackward(); break;
      case 'L': turnLeft(); break;
      case 'R': turnRight(); break;
      case 'S': stopMotors(); break;
    }

    fireTracking(flame); // manual mode still tracks fire
  }

  // PUMP (manual override)
  if (pumpMode == "MANUAL") {
    digitalWrite(RELAY_PIN, pumpState);
  }

  // SOUND
  playSound();

  // SERIAL OUTPUT
  Serial.print("T:"); Serial.print(temperature);
  Serial.print(",H:"); Serial.print(humidity);
  Serial.print(",PPM:"); Serial.print(airPPM);
  Serial.print(",L:"); Serial.print(leftDist);
  Serial.print(",R:"); Serial.print(rightDist);
  Serial.print(",F:"); Serial.print(flame);

  Serial.print(",ROLL:"); Serial.print(roll);
  Serial.print(",PITCH:"); Serial.print(pitch);
  Serial.print(",YAW:"); Serial.print(yaw);
  Serial.print(",DIST:"); Serial.print(distanceTravelled);

  Serial.print(",MODE:"); Serial.print(roverMode);
  Serial.print(",PUMP:"); Serial.print(pumpMode);

  Serial.println();

  delay(200);
}  