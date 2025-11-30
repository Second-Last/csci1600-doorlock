#include <Servo.h>

#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "WiFiS3.h"
#include "arduino_secrets.h"

Servo myservo;
ArduinoLEDMatrix matrix;

// Control and feedback pins
const int servoPin = 9;
const int feedbackPin = A0;

// Calibration values
int minDegrees;
int maxDegrees;
int minFeedback;
int maxFeedback;
int tolerance = 5;  // max feedback measurement error

// FSM State Enum
enum State { CALIBRATE_LOCK, CALIBRATE_UNLOCK, UNLOCK, LOCK, BUSY_WAIT, BUSY_MOVE, BAD };

// Command Enum
enum Command { NONE, LOCK_CMD, UNLOCK_CMD };

// FSM State struct
struct FSMState {
  State currentState;
  int lockDeg;
  int unlockDeg;
  unsigned long startTime;
};

// Global FSM state
FSMState fsmState;
State lastDisplayedState = BAD;  // Track last displayed state to avoid unnecessary updates

// Timeout constant (milliseconds)
const unsigned long TOL = 5000;  // 5 second timeout for moves

// Hardcoded lock positions
const int LOCK_ANGLE = 120;
const int UNLOCK_ANGLE = 50;

// Angle tolerance for position checking (degrees)
const int ANGLE_TOLERANCE = 3;

// WiFi setup
char ssid[] = SECRET_SSID;
#ifdef SECRET_PASS
char pass[] = SECRET_PASS;
#endif
int status = WL_IDLE_STATUS;
WiFiServer server(80);

// Function to display text on LED matrix
void displayText(const char* text) {
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(text);
  matrix.endText();
  matrix.endDraw();
}

// Prints MAC address
void printMacAddress(byte mac[]) {
  for (int i = 0; i < 6; i++) {
    if (i > 0) {
      Serial.print(":");
    }
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
  }
  Serial.println();
}

// Function to print WiFi status
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print MAC address
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

// Function to update LED matrix based on FSM state
void updateMatrixDisplay() {
  // Only update if state has changed
  if (fsmState.currentState == lastDisplayedState) {
    return;
  }

  lastDisplayedState = fsmState.currentState;

  switch (fsmState.currentState) {
    case CALIBRATE_LOCK:
      displayText("CL");
      break;
    case CALIBRATE_UNLOCK:
      displayText("CU");
      break;
    case UNLOCK:
      displayText("U");
      break;
    case LOCK:
      displayText("L");
      break;
    case BUSY_WAIT:
      displayText("BW");
      break;
    case BUSY_MOVE:
      displayText("BM");
      break;
    case BAD:
      displayText("!!");
      break;
  }
}

/*
  This function establishes the feedback values for 2 positions of the servo.
  With this information, we can interpolate feedback values for intermediate positions
*/
void calibrate(Servo servo, int analogPin, int minPos, int maxPos) {
  // Move to the minimum position and record the feedback value
  servo.write(minPos);
  minDegrees = minPos;
  delay(2000);  // make sure it has time to get there and settle
  minFeedback = analogRead(analogPin);

  // Move to the maximum position and record the feedback value
  servo.write(maxPos);
  maxDegrees = maxPos;
  delay(2000);  // make sure it has time to get there and settle
  maxFeedback = analogRead(analogPin);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Initialize Arduino LED Matrix FIRST to test it
  matrix.begin();
  Serial.println("Testing LED Matrix - displaying test text");
  displayText("OK");
  delay(2000);  // Show test pattern for 2 seconds
  Serial.println("LED Matrix test complete");

  // WiFi setup
  Serial.println(SECRET_SSID);
#ifdef SECRET_PASS
  Serial.println(SECRET_PASS);
#endif

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);
#ifdef SECRET_PASS
    status = WiFi.begin(ssid, pass);
#else
    status = WiFi.begin(ssid);
#endif
    delay(10000);
  }
  server.begin();
  printWifiStatus();

  myservo.attach(servoPin);

  calibrate(myservo, feedbackPin, 50, 140);  // calibrate for the 20-160 degree range
  Serial.print("minFeedback: ");
  Serial.println(minFeedback);
  Serial.print("maxFeedback: ");
  Serial.println(maxFeedback);
  delay(1000);

  // Initialize FSM state
  fsmState.currentState = CALIBRATE_LOCK;
  fsmState.lockDeg = LOCK_ANGLE;
  fsmState.unlockDeg = UNLOCK_ANGLE;
  fsmState.startTime = 0;

  Serial.println("FSM initialized in CALIBRATE_LOCK state");
  Serial.println("Hardcoded angles - Lock: 120, Unlock: 50");

  // Display initial state
  updateMatrixDisplay();

  // Run through calibration states. Manually rotate the motor to simulate user
  // calibration.
  // TODO: remove once we implement power cutoff for the servo to release
  // control of the motor
  fsmTransition(LOCK_ANGLE, millis(), NONE, NONE);
  fsmTransition(UNLOCK_ANGLE, millis(), NONE, NONE);
  myservo.write(50);
  delay(1000);

  Serial.println("FSM ready - now in UNLOCK state");
}

void Seek(Servo servo, int analogPin, int pos) {
  // Start the move...
  servo.write(pos);

  // Calculate the target feedback value for the final position
  int target = map(pos, minDegrees, maxDegrees, minFeedback, maxFeedback);
  Serial.print("Target: ");
  Serial.println(target);

  // Wait until it reaches the target
  while (abs(analogRead(analogPin) - target) > tolerance) {
    Serial.print("Current reading: ");
    Serial.println(analogRead(analogPin));
  }  // wait...
}

// Get current servo position from feedback
int getCurrentDeg() {
  int feedback = analogRead(feedbackPin);
  return map(feedback, minFeedback, maxFeedback, minDegrees, maxDegrees);
}

// Check if at unlock position (open-ended tolerance: only check upper bound)
bool isAtUnlock(int deg) { return deg <= (fsmState.unlockDeg + ANGLE_TOLERANCE); }

// Check if at lock position (open-ended tolerance: only check lower bound)
bool isAtLock(int deg) { return deg >= (fsmState.lockDeg - ANGLE_TOLERANCE); }

// FSM Transition Function
void fsmTransition(int deg, unsigned long millis, Command button, Command cmd) {
  State nextState = fsmState.currentState;

  switch (fsmState.currentState) {
    case CALIBRATE_LOCK:
      // Automatically advance to next calibration state
      fsmState.lockDeg = deg;
      nextState = CALIBRATE_UNLOCK;
      Serial.println("FSM: CALIBRATE_LOCK -> CALIBRATE_UNLOCK");
      break;

    case CALIBRATE_UNLOCK:
      // Automatically advance to UNLOCK state
      fsmState.unlockDeg = deg;
      nextState = UNLOCK;
      Serial.println("FSM: CALIBRATE_UNLOCK -> UNLOCK");
      break;

    case UNLOCK:
      if (isAtUnlock(deg) && cmd == LOCK_CMD) {
        nextState = BUSY_MOVE;
        fsmState.startTime = millis;
        myservo.write(fsmState.lockDeg);
        Serial.println("FSM: UNLOCK -> BUSY_MOVE (locking)");
      } else if (isAtLock(deg)) {
        nextState = LOCK;
        Serial.println("FSM: UNLOCK -> LOCK");
      } else if (!isAtLock(deg) && !isAtUnlock(deg)) {
        nextState = BUSY_WAIT;
        Serial.println("FSM: UNLOCK -> BUSY_WAIT (manual turn detected)");
      }
      break;

    case LOCK:
      if (isAtLock(deg) && cmd == UNLOCK_CMD) {
        nextState = BUSY_MOVE;
        fsmState.startTime = millis;
        myservo.write(fsmState.unlockDeg);
        Serial.println("FSM: LOCK -> BUSY_MOVE (unlocking)");
      } else if (isAtUnlock(deg)) {
        nextState = UNLOCK;
        Serial.println("FSM: LOCK -> UNLOCK");
      } else if (!isAtLock(deg) && !isAtUnlock(deg)) {
        nextState = BUSY_WAIT;
        Serial.println("FSM: LOCK -> BUSY_WAIT (manual turn detected)");
      }
      break;

    case BUSY_WAIT:
      // No timeout - user can manually turn for as long as they want
      if (isAtUnlock(deg)) {
        nextState = UNLOCK;
        Serial.println("FSM: BUSY_WAIT -> UNLOCK");
      } else if (isAtLock(deg)) {
        nextState = LOCK;
        Serial.println("FSM: BUSY_WAIT -> LOCK");
      }
      // Otherwise stay in BUSY_WAIT
      break;

    case BUSY_MOVE:
      if (millis - fsmState.startTime > TOL) {
        nextState = BAD;
        Serial.println("FSM: BUSY_MOVE -> BAD (timeout)");
      } else if (isAtUnlock(deg)) {
        nextState = UNLOCK;
        Serial.println("FSM: BUSY_MOVE -> UNLOCK");
      } else if (isAtLock(deg)) {
        nextState = LOCK;
        Serial.println("FSM: BUSY_MOVE -> LOCK");
      }
      break;

    case BAD:
      // Stay in BAD state - requires manual reset
      Serial.println("FSM: In BAD state - reset required");
      break;
  }

  fsmState.currentState = nextState;
}

void testAngle() {
  int deg;
  while (deg = Serial.parseInt()) {
    Serial.print("Turning to ");
    Serial.println(deg);
    Seek(myservo, feedbackPin, deg);
    delay(5000);
  }
}

void loop() {
  // Handle WiFi clients
  WiFiClient client = server.available();
  Command cmd = NONE;
  if (client) {
    Serial.println("new client");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // HTTP headers
            // TODO(gz): just return HTTP 200 if cmd != NONE
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Access-Control-Allow-Origin: *");
            client.println();
            client.println("<p>Doorlock server</p>");
            client.println();
            break;
          } else {
            // Check for commands
            if (currentLine.startsWith("POST /lock")) {
              cmd = LOCK_CMD;
              Serial.println("Received LOCK command");
            } else if (currentLine.startsWith("POST /unlock")) {
              cmd = UNLOCK_CMD;
              Serial.println("Received UNLOCK command");
            }

            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("client disconnected");
  }

  // Get current servo position
  int currentDeg = getCurrentDeg();

  // Run FSM transition
  fsmTransition(currentDeg, millis(), NONE, cmd);

  // Update LED matrix display
  updateMatrixDisplay();

  // Small delay
  delay(100);
}
