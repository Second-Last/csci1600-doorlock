#include <Servo.h> 

Servo myservo;  

// Control and feedback pins
const int servoPin = 9;
const int feedbackPin = A0;
const int lockPin = 2;
const int unlockPin = 3;

// Calibration values
int minDegrees;
int maxDegrees;
int minFeedback;
int maxFeedback;
int tolerance = 5; // max feedback measurement error

/*
  This function establishes the feedback values for 2 positions of the servo.
  With this information, we can interpolate feedback values for intermediate positions
*/
void calibrate(Servo servo, int analogPin, int minPos, int maxPos)
{
  // Move to the minimum position and record the feedback value
  servo.write(minPos);
  minDegrees = minPos;
  delay(2000); // make sure it has time to get there and settle
  minFeedback = analogRead(analogPin);

  // Move to the maximum position and record the feedback value
  servo.write(maxPos);
  maxDegrees = maxPos;
  delay(2000); // make sure it has time to get there and settle
  maxFeedback = analogRead(analogPin);
}


void setup() 
{ 
  Serial.begin(9600);
  while (!Serial);

  myservo.attach(servoPin); 

  calibrate(myservo, feedbackPin, 50, 140);  // calibrate for the 20-160 degree range
  Serial.print("minFeedback: ");
  Serial.println(minFeedback);
  Serial.print("maxFeedback: ");
  Serial.println(maxFeedback);
} 

void Seek(Servo servo, int analogPin, int pos)
{
  // Start the move...
  servo.write(pos);

  // Calculate the target feedback value for the final position
  int target = map(pos, minDegrees, maxDegrees, minFeedback, maxFeedback); 
  Serial.print("Target: ");
  Serial.println(target);

  // Wait until it reaches the target
  while(abs(analogRead(analogPin) - target) > tolerance)
  {
    Serial.print("Current reading: ");
    Serial.println(analogRead(analogPin));
  } // wait...
}

void testAngle()
{
  int deg;
  while (deg = Serial.parseInt())
  {
    Serial.print("Turning to ");
    Serial.println(deg);
    Seek(myservo, feedbackPin, deg);
    delay(5000);
  }
}

void loop()
{
  testAngle();
}
