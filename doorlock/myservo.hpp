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

  /**
   * This function initializes the necessary hardware so that the transistor pin can output signals
   * 
   * Input: None
   * Output: None
   * 
   */
  void init() { pinMode(transistorPin, OUTPUT); }

  /**
   * This function attaches the servo motor to the servoPin, allowing the servor motor to be controlled
   * by sending signals through this pin.
   * 
   * Input: None
   * Output: None
   * 
   * Side Effect: Servo motor hardware is configured
   * 
   */
  void attach() {
    if (attached) return;
    digitalWrite(transistorPin, HIGH);
    servo.attach(servoPin);
    attached = true;
  }

  /**
   * This function formally sends a request to the servo motor to turn to a given degree. It assumes that
   * the servo motor is already attached to corresponding hardware. Note that the servo cannot guarantee it would've 
   * turned to the requested position when this method returns.
   * 
   * Input:
   *  - deg (int) : Integer value indicating the desired degree to which the servo motor should be turned
   * 
   * Output: None
   * 
   * Side Effect: Servo motor should rotate to desired degree
   * 
   */
  void write(int deg) {
    assert(attached);
    servo.write(deg);
  }

  /**
   * This function accomplishes the same as the `write()` function above, but attaches the motor to the 
   * correct hardware before attempting to rotate the motor
   * 
   * Input:
   *  - deg (int) : Integer value indicating the desired angle to which the servo motor should be rotated
   * 
   * Output: None
   * 
   * Side Effect: Servo motor should rotate to specified degree
   * 
   */
  void attachAndWrite(int deg) {
    attach();
    write(deg);
  }

  /**
   * This function detaches the servo motor by cutting off its power supply. Consequently, any calls to `write()`
   * will no longer rotate the motor
   * 
   * Input: None
   * 
   * Output: None
   * 
   */
  void detach() {
    if (!attached) return;
    servo.detach();
    digitalWrite(transistorPin, LOW);
    attached = false;
  }

  /**
   * This function returns the current position of the motor in degrees
   * 
   * Input: None
   * 
   * Output: Integer value representing the current motor's position, in degrees
   * 
   */
  int deg() {
    int feedback = analogReadStable(feedbackPin);
    if (attached) {
      return map(feedback, minFeedback, maxFeedback, minDegrees, maxDegrees);
    } else {
      return map(feedback, minPoFeedback, maxPoFeedback, minDegrees, maxDegrees);
    }
  }

  /**
   * This function is responsible for calibrating the servor motor by setting the min and max positions.
   * This function establishes the feedback values for 2 positions of the servo motor. Such information
   * is relevant to enable us to interpolate feedback values for intermediate positions.
   * 
   * Input:
   *  - minPos (int) : The min position of the servo motor in degrees, represented by an int
   *  - maxPos (int) : The max position of the servo motor in degrees, represented by an int
   * 
   * Output: None
   * 
   * Side Effect: Servo motor is calibrated with its min and max positions 
   * 
   */
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
