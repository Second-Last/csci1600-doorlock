#pragma once

#include <Arduino.h>
#include <Servo.h>
#include "utils.h"

/**
 * This is a wrapper around the official `Servo` class that provides additional
 * functionality to:
 *
 * - measure the current position of the motor, regardless of whether the motor
 *   is powered or not.
 * - calibrate the lock so that the position reading is as precise as possible.
 * - cut the power to the servo motor using a BJT transistor.
 */
struct MyServo {
  int servoPin = 9;
  int feedbackPin = A0;
  int transistorPin = 5;
  Servo servo;
  bool attached = false;
  int minDegrees;
  int maxDegrees;
  int minFeedback;
  int maxFeedback;
  int minPoFeedback;
  int maxPoFeedback;

  MyServo(int servoPin, int feedbackPin, int transistorPin)
      : servoPin(servoPin), feedbackPin(feedbackPin), transistorPin(transistorPin) {}

  // Initialize the necessary hardware.
  void init() { pinMode(transistorPin, OUTPUT); }

  // Attach to the servo.
  void attach() {
    if (attached) return;
    digitalWrite(transistorPin, HIGH);
    servo.attach(servoPin);
    attached = true;
  }

  // Send a request to the servo to turn to the given degree. Assumes that the
  // servo is already attached. Note that the servo cannot guarantee it would've
  // turned to the requested position when this method returns.
  void write(int deg) {
    assert(attached);
    servo.write(deg);
  }

  // Same as `write` but attaches the servo if it has not been attached.
  void attachAndWrite(int deg) {
    attach();
    write(deg);
  }

  // Detach the servo by cutting its power supply. This means the motor can no
  // longer control itself.
  void detach() {
    if (!attached) return;
    servo.detach();
    digitalWrite(transistorPin, LOW);
    attached = false;
  }

  // Return the current position of the motor.
  int deg() {
    int feedback = analogReadStable(feedbackPin);
    if (attached) {
      return map(feedback, minFeedback, maxFeedback, minDegrees, maxDegrees);
    } else {
      return map(feedback, minPoFeedback, maxPoFeedback, minDegrees, maxDegrees);
    }
  }

  // Establish the feedback values for 2 positions of the servo.
  // With this information, we can interpolate feedback values for intermediate positions
  void calibrate(int minPos, int maxPos) {
	Serial.print("Calibrating with minPos=");
	Serial.print(minPos);
	Serial.print(", maxPos=");
	Serial.println(maxPos);

    bool prevAttached = attached;

    // Move to the minimum position and record the feedback value
    attachAndWrite(minPos);
    minDegrees = minPos;
    delay(2000);  // make sure it has time to get there and settle
    minFeedback = analogReadStable(feedbackPin);
    detach();
    delay(500);
    minPoFeedback = analogReadStable(feedbackPin);

    // Move to the maximum position and record the feedback value
    attachAndWrite(maxPos);
    maxDegrees = maxPos;
    delay(2000);  // make sure it has time to get there and settle
    maxFeedback = analogReadStable(feedbackPin);
    detach();
    delay(500);
    maxPoFeedback = analogReadStable(feedbackPin);

    if (prevAttached) attach();
  }
};
