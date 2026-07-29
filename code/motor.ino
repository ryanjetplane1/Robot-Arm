#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHARACTERISTIC_UUID "19B10001-E8F2-537E-4F6C-D104768A1214"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
String pendingData = "";
volatile bool hasNewData = false;

Servo servo1; Servo servo2; Servo servo3;
float current_s1 = 90.0; float current_s2 = 90.0; float current_s3 = 90.0;

const int IN1 = D0; const int IN2 = D1; const int IN3 = D2; const int IN4 = D3;
int stepIndex = 0;
long stepperCount = 0;
bool isResetActive = false;

int target_lx = 0, target_ly = 0, target_rx = 0, target_ry = 0;
unsigned long lastStepTime = 0;
unsigned long lastServoTime = 0;
const unsigned long STEP_INTERVAL_US = 2500;
const unsigned long SERVO_INTERVAL_MS = 20;

unsigned long lastCommandMillis = 0;
const unsigned long COMMAND_TIMEOUT_MS = 400;

enum MacroState {
  MACRO_IDLE,
  MACRO_RAMP_UP,
  MACRO_LOWER_WAIT,
  MACRO_TILT_DOWN,
  MACRO_TILT_DOWN_WAIT,
  MACRO_TILT_UP_WAIT,
  MACRO_STEP_CCW_1,
  MACRO_WAIT_1,
  MACRO_STEP_CW,
  MACRO_WAIT_2,
  MACRO_STEP_CCW_2,
  MACRO_WAIT_3,
  MACRO_RAMP_DOWN
};

MacroState macroState = MACRO_IDLE;
unsigned long macroTimer = 0;
int macroOffset = 0;
long macroStepsDone = 0;
unsigned long macroStepTimer = 0;

void takeSingleStep(bool clockwise) {
  int lookup[4][4] = {
    {HIGH, LOW, LOW, LOW}, {LOW, HIGH, LOW, LOW},
    {LOW, LOW, HIGH, LOW}, {LOW, LOW, LOW, HIGH}
  };
  if (clockwise) {
    stepIndex++; if (stepIndex > 3) stepIndex = 0;
    stepperCount++;
  } else {
    stepIndex--; if (stepIndex < 0) stepIndex = 3;
    stepperCount--;
  }
  digitalWrite(IN1, lookup[stepIndex][0]);
  digitalWrite(IN2, lookup[stepIndex][1]);
  digitalWrite(IN3, lookup[stepIndex][2]);
  digitalWrite(IN4, lookup[stepIndex][3]);
}

void disableStepperCoils() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void startMacro1() {
  if (macroState != MACRO_IDLE) return;
  macroOffset = 0;
  macroState = MACRO_RAMP_UP;
}

void serviceMacro1() {
  unsigned long now = millis();

  switch (macroState) {
    case MACRO_IDLE:
      return;

    case MACRO_RAMP_UP:
      servo3.write(90);
      if (now - lastServoTime >= 40) {
        lastServoTime = now;
        servo2.write(90 - macroOffset);
        servo1.write(90 + macroOffset);
        macroOffset++;
        if (macroOffset > 45) {
          macroTimer = now;
          macroState = MACRO_LOWER_WAIT;
        }
      }
      break;

    case MACRO_LOWER_WAIT:
      if (now - macroTimer >= 1000) {
        servo3.write(0);
        macroTimer = now;
        macroState = MACRO_TILT_DOWN_WAIT;
      }
      break;

    case MACRO_TILT_DOWN_WAIT:
      if (now - macroTimer >= 2000) {
        servo3.write(90);
        macroTimer = now;
        macroState = MACRO_TILT_UP_WAIT;
      }
      break;

    case MACRO_TILT_UP_WAIT:
      if (now - macroTimer >= 2000) {
        macroStepsDone = 0;
        macroStepTimer = now;
        macroState = MACRO_STEP_CCW_1;
      }
      break;

    case MACRO_STEP_CCW_1:
      servo3.write(90);
      if (now - macroStepTimer >= 4) {
        macroStepTimer = now;
        takeSingleStep(false);
        macroStepsDone++;
        if (macroStepsDone >= 200) {
          disableStepperCoils();
          macroTimer = now;
          macroState = MACRO_WAIT_1;
        }
      }
      break;

    case MACRO_WAIT_1:
      if (now - macroTimer >= 1000) {
        macroStepsDone = 0;
        macroStepTimer = now;
        macroState = MACRO_STEP_CW;
      }
      break;

    case MACRO_STEP_CW:
      servo3.write(90);
      if (now - macroStepTimer >= 4) {
        macroStepTimer = now;
        takeSingleStep(true);
        macroStepsDone++;
        if (macroStepsDone >= 400) {
          disableStepperCoils();
          macroTimer = now;
          macroState = MACRO_WAIT_2;
        }
      }
      break;

    case MACRO_WAIT_2:
      if (now - macroTimer >= 1000) {
        macroStepsDone = 0;
        macroStepTimer = now;
        macroState = MACRO_STEP_CCW_2;
      }
      break;

    case MACRO_STEP_CCW_2:
      servo3.write(90);
      if (now - macroStepTimer >= 4) {
        macroStepTimer = now;
        takeSingleStep(false);
        macroStepsDone++;
        if (macroStepsDone >= 200) {
          disableStepperCoils();
          macroTimer = now;
          macroState = MACRO_WAIT_3;
        }
      }
      break;

    case MACRO_WAIT_3:
      if (now - macroTimer >= 1000) {
        macroOffset = 45;
        macroState = MACRO_RAMP_DOWN;
      }
      break;

    case MACRO_RAMP_DOWN:
      if (now - lastServoTime >= 40) {
        lastServoTime = now;
        servo2.write(90 - macroOffset);
        servo1.write(90 + macroOffset);
        macroOffset--;
        if (macroOffset < 0) {
          current_s1 = 90.0; current_s2 = 90.0; current_s3 = 90.0;
          target_lx = 0; target_ly = 0; target_rx = 0; target_ry = 0;
          macroState = MACRO_IDLE;
        }
      }
      break;
  }
}

void parseStreamData(String data) {
  data.trim();
  if (data.length() == 0) return;
  if (data == "MACRO1") { startMacro1(); return; }
  if (data == "RESET") {
    macroState = MACRO_IDLE;
    isResetActive = true;
    current_s1 = 90.0; current_s2 = 90.0; current_s3 = 90.0;
    servo1.write(90); servo2.write(90); servo3.write(90);
    return;
  }

  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);
  int thirdComma = data.indexOf(',', secondComma + 1);
  if (firstComma == -1 || secondComma == -1 || thirdComma == -1) return;

  target_lx = data.substring(0, firstComma).toInt();
  target_ly = data.substring(firstComma + 1, secondComma).toInt();
  target_rx = data.substring(secondComma + 1, thirdComma).toInt();
  target_ry = data.substring(thirdComma + 1).toInt();
  lastCommandMillis = millis();
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; };
    void onDisconnect(BLEServer* pServer) { deviceConnected = false; pServer->getAdvertising()->start(); }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();
      if (value.length() > 0) { pendingData = value; hasNewData = true; }
    }
};

void setup() {
  Serial.begin(115200);
  servo1.attach(D9); servo2.attach(D5); servo3.attach(D4);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  disableStepperCoils();
  servo1.write(90); servo2.write(90); servo3.write(90);

  BLEDevice::init("RobotArm");
  BLEDevice::setMTU(247);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

void loop() {
  if (hasNewData) {
    hasNewData = false;
    parseStreamData(pendingData);
  }

  if (macroState == MACRO_IDLE && !isResetActive &&
      lastCommandMillis != 0 &&
      millis() - lastCommandMillis > COMMAND_TIMEOUT_MS) {
    target_lx = 0; target_ly = 0; target_rx = 0; target_ry = 0;
  }

  serviceMacro1();

  unsigned long currentMicros = micros();
  unsigned long currentMillis = millis();

  if (isResetActive) {
    if (stepperCount != 0) {
      if (currentMicros - lastStepTime >= STEP_INTERVAL_US) {
        takeSingleStep(stepperCount <= 0);
        lastStepTime = currentMicros;
      }
    } else {
      disableStepperCoils();
      if (target_lx == 0 && target_ly == 0 && target_rx == 0 && target_ry == 0) {
        isResetActive = false;
      }
    }
  } else if (macroState == MACRO_IDLE) {
    if (target_lx > 30) {
      if (currentMicros - lastStepTime >= STEP_INTERVAL_US) { takeSingleStep(true); lastStepTime = currentMicros; }
    } else if (target_lx < -30) {
      if (currentMicros - lastStepTime >= STEP_INTERVAL_US) { takeSingleStep(false); lastStepTime = currentMicros; }
    } else {
      disableStepperCoils();
    }
  }

  if (!isResetActive && macroState == MACRO_IDLE && (currentMillis - lastServoTime >= SERVO_INTERVAL_MS)) {
    lastServoTime = currentMillis;

    if (abs(target_ly) > 25) {
      current_s1 -= (target_ly / 100.0) * 1.0;
    }
    if (abs(target_ry) > 25) {
      current_s2 -= (target_ry / 100.0) * 1.0;
    }
    if (abs(target_rx) > 25) {
      current_s3 -= (target_rx / 100.0) * 1.5;
    }

    current_s1 = constrain(current_s1, 0.0, 180.0);
    current_s2 = constrain(current_s2, 0.0, 180.0);
    current_s3 = constrain(current_s3, 0.0, 180.0);

    servo1.write((int)current_s1);
    servo2.write((int)current_s2);
    servo3.write((int)current_s3);
  }
}
