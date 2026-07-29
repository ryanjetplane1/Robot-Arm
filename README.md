# 4-Axis Robotic Arm

A BLE controlled robotic arm built on the ESP32-C5. Control it with the website on desktop or on iphone using the Bluefy app.

Four axes of movement, three servos and a stepper for base rotation, controlled from two joysticks over Web Bluetooth or hardcoded macros.

## Overview

This project is a robotic arm with four axes of movement. 2 Bluetooth Low Energy joysticks controls the arm. The arm uses an ESP32-C5 microcontroller. The microcontroller sends commands to three servo motors and one stepper motor. You can control the arm through a web browser or preprogramming with a macro. The web browser uses Web Bluetooth to connect to the arm.

**Features**
- Base, Elbow, Arm, Fingers. Three axes use servo motors. One axis (Base) uses a stepper motor.
- Adjustable control from 2 joysticks.
- Browser based control through Web Bluetooth.
- A preset movement sequence that runs without input.
- A reset command that returns the arm to its home position.
- A command timeout that stops the arm if it stops receiving data.

## Safety

Read this before you build or use the arm.

- The servo motors and the stepper motor move with high force. Keep your hands away from the arm when it is powered.
- Do not exceed 8.4 V on the servo power rail. A higher voltage can damage and fry the servos. (I learned from experience)
- Disconnect the battery before you change wiring.
- If the BLE connection drops, the arm stops all joystick driven movement after 400 milliseconds. This is a safety timeout. Do not rely on it to stop the arm when a macro is running.

## Hardware

### Bill of Materials

| Component | Description | Qty |
|---|---|---|
| Seeed XIAO ESP32-C5 SuperMini | Main microcontroller with BLE support | 1 |
| 35kg Servo motor | Drives arm | 3 |
| 28BYJ-48 stepper motor | Drives base | 1 |
| ULN2003 driver board | Drives the stepper motor coils | 1 |
| 10A 8.4v Power supply | Powers all motors | 1 |
| XL6019 Buck Boost Converter | Stepper motor power | 1 |

The base requires low torque so most steppers will work.

### Wiring

**Servos**

| Servo | Board Pin |
|---|---|
| Servo 1 | D9 |
| Servo 2 | D5 |
| Servo 3 | D4 |

**Stepper (base rotation)**

| Driver | Board Pin |
|---|---|
| IN1 | D0 |
| IN2 | D1 |
| IN3 | D2 |
| IN4 | D3 |

**Power**
|---|---|
| Converter | Stepper Pin |
| IN- | Gnd |
| IN+ | 8.4v |
| Out- | Gnd |
| Out+ | 5v |



Connect all grounds together including microcontroller and power supply. Connect all servo power wires to 8.4 V. The stepper motor should be powered with 5v. Do not connect the motor power rail to the microcontroller power pin.

## Firmware

The firmware runs on the ESP32-C5. The firmware controls the three servos directly while the stepper motor is controlled with a 4-step sequence.

**Install**
1. Open the firmware project in the Arduino IDE.
2. Install the `ESP32Servo` library and the ESP32 BLE libraries.
3. Select the board type "XIAO ESP32-C5" from the board manager.
4. Connect the microcontroller to your computer with a USB cable.
5. Select the correct serial port.
6. Click Upload to load the firmware onto the microcontroller.

## Usage

**BLE Joystick**

The BLE joystick sends data to the arm over Bluetooth Low Energy. Pair the joystick with the arm before use.

**Web Bluetooth**

Control page: [ryanjetplane1.github.io/robot-arm](https://ryanjetplane1.github.io/robot-arm/)

1. Open the link above.
2. On iPhone, use the Bluefy browser. Standard iOS browsers do not support Web Bluetooth.
3. Tap Connect on the web page.
4. Select `RobotArm` from the device list.
5. Use the on-screen controls to move the arm.

If the connection fails with a "failed undefined" error, close and reopen the Bluefy browser, then try again.
