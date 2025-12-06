#pragma once

#include <Arduino.h>
#include <Servo.h>
#include "utils.h"

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

  void init() { pinMode(transistorPin, OUTPUT); }

  void attach() {
    if (attached) return;
    digitalWrite(transistorPin, HIGH);
    servo.attach(servoPin);
    attached = true;
  }

  void write(int deg) {
    assert(attached);
    servo.write(deg);
  }

  void attachAndWrite(int deg) {
    attach();
    write(deg);
  }

  void detach() {
    if (!attached) return;
    servo.detach();
    digitalWrite(transistorPin, LOW);
    attached = false;
  }

  int deg() {
    int feedback = analogReadStable(feedbackPin);
    if (attached) {
      return map(feedback, minFeedback, maxFeedback, minDegrees, maxDegrees);
    } else {
      return map(feedback, minPoFeedback, maxPoFeedback, minDegrees, maxDegrees);
    }
  }

  /*
    This function establishes the feedback values for 2 positions of the servo.
    With this information, we can interpolate feedback values for intermediate positions
  */
  void calibrate(int minPos, int maxPos) {
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
