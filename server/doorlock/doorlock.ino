#include <ArduinoBearSSL.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <EEPROM.h>
#include <Servo.h>
#include <WDT.h>
#include <WiFiS3.h>

#include "arduino_secrets.h"
#include "testing.h"

#if defined(INTEGRATION_TEST) && defined(UNIT_TEST)
#error "INTEGRATION_TEST and UNIT_TEST cannot be both defined!"
#elif defined(INTEGRATION_TEST) || defined(UNIT_TEST)
#define TESTING
#endif

#include "myservo.hpp"
#include "utils.h"

// FSM State Enum (must be defined before test headers are included)
enum State { CALIBRATE_LOCK, CALIBRATE_UNLOCK, UNLOCK, LOCK, BUSY_WAIT, BUSY_MOVE, BAD };

// Command Enum
enum Command { NONE, LOCK_CMD, UNLOCK_CMD };

// Request Enum that represents the different HTTP requests the server can get
enum Request { EMPTY, UNRECOGNIZED, STATUS, OPTIONS, LOCK_REQ, UNLOCK_REQ };

Command requestToCommand(Request req) {
  switch (req) {
    case EMPTY:
    case UNRECOGNIZED:
    case STATUS:
      return NONE;
    case LOCK_REQ:
      return LOCK_CMD;
    case UNLOCK_REQ:
      return UNLOCK_CMD;
  }
}

// FSM State struct
struct FSMState {
  State currentState;
  int lockDeg;
  int unlockDeg;
  unsigned long startTime;
  Command curCmd;
};

// Timeout constant (milliseconds)
const unsigned long TOL = 5000;  // 5 second timeout for moves

// Hardcoded lock positions
const int LOCK_ANGLE = 110;
const int UNLOCK_ANGLE = 40;

// Angle tolerance for position checking (degrees)
const int ANGLE_TOLERANCE = 5;

// Global FSM state (must be defined before test headers are included)
FSMState fsmState;

ArduinoLEDMatrix matrix;

// Control and feedback pins
const int servoPin = 9;
const int feedbackPin = A0;
const int transistorPin = 5;

MyServo myservo(servoPin, feedbackPin, transistorPin);

State lastDisplayedState = BAD;  // Track last displayed state to avoid unnecessary updates

const long wdtInterval = 2684;

// WiFi setup
char ssid[] = SECRET_SSID;
#ifdef SECRET_PASS
char pass[] = SECRET_PASS;
#endif
int status = WL_IDLE_STATUS;
WiFiServer server(80);

// EEPROM address for last valid timestamp
const int EEPROM_TIMESTAMP_ADDR = 0;
// Replay protection window (seconds)
const unsigned long REPLAY_WINDOW = 5;

// Function to return the state as a string
String stateToString(State st) {
  switch (st) {
    case CALIBRATE_LOCK:
      return "CALIBRATE_LOCK";
    case CALIBRATE_UNLOCK:
      return "CALIBRATE_UNLOCK";
    case UNLOCK:
      return "UNLOCK";
    case LOCK:
      return "LOCK";
    case BUSY_WAIT:
      return "BUSY_WAIT";
    case BUSY_MOVE:
      return "BUSY_MOVE";
    case BAD:
      return "BAD";
  }
}

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

// Convert hex char to value (0-15)
int hexCharToValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Convert hex string to byte array
// Returns true on success, false on invalid hex
bool hexToBytes(const String& hex, unsigned char* output, size_t outputLen) {
  if (hex.length() != outputLen * 2) return false;

  for (size_t i = 0; i < outputLen; i++) {
    int high = hexCharToValue(hex.charAt(i * 2));
    int low = hexCharToValue(hex.charAt(i * 2 + 1));
    if (high < 0 || low < 0) return false;
    output[i] = (high << 4) | low;
  }
  return true;
}

// Compute HMAC-SHA256 signature
void computeHMAC(const String& message, const char* key, unsigned char* output) {
  br_hmac_key_context keyCtx;
  br_hmac_key_init(&keyCtx, &br_sha256_vtable, key, strlen(key));

  br_hmac_context hmacCtx;
  br_hmac_init(&hmacCtx, &keyCtx, 0);
  br_hmac_update(&hmacCtx, message.c_str(), message.length());
  br_hmac_out(&hmacCtx, output);
}

// Constant-time comparison to prevent timing attacks
bool constantTimeCompare(const unsigned char* a, const unsigned char* b, size_t len) {
  unsigned char result = 0;
  for (size_t i = 0; i < len; i++) {
    result |= a[i] ^ b[i];
  }
  return result == 0;
}

// Verify HMAC authentication
// Returns true if authentication succeeds
bool verifyAuthentication(const String& nonce, const String& signature) {
#ifdef SKIP_AUTH
  return true;
#endif
  // Parse nonce as unsigned long
  unsigned long requestTimestamp = nonce.toInt();
  if (requestTimestamp == 0 && nonce != "0") {
    Serial.print("Auth failed: invalid nonce format, nonce=");
    Serial.println(nonce);
    return false;
  }

  // Get last valid timestamp from EEPROM
  unsigned long lastTimestamp = 0;
  EEPROM.get(EEPROM_TIMESTAMP_ADDR, lastTimestamp);

  // Check replay protection with 5-second window
  if (requestTimestamp <= max(5, lastTimestamp) - 5) {
    Serial.print("Auth failed: replay/timestamp check. Request too old. Request: ");
    Serial.print(requestTimestamp);
    Serial.print(", Last: ");
    Serial.println(lastTimestamp);
    return false;
  }

  // Compute expected HMAC
  unsigned char expectedHMAC[32];  // SHA256 produces 32 bytes
  computeHMAC(nonce, REMOTE_LOCK_PASS, expectedHMAC);

  // Convert hex signature to bytes
  unsigned char receivedHMAC[32];
  if (!hexToBytes(signature, receivedHMAC, 32)) {
    Serial.println("Auth failed: invalid signature format");
    return false;
  }

  // Constant-time comparison
  if (!constantTimeCompare(expectedHMAC, receivedHMAC, 32)) {
    Serial.println("Auth failed: signature mismatch");
    return false;
  }

  // Update last valid timestamp in EEPROM
  EEPROM.put(EEPROM_TIMESTAMP_ADDR, requestTimestamp);

  Serial.println("Auth success");
  return true;
}

// Check if at unlock position (open-ended tolerance: only check upper bound)
bool isAtUnlock(int deg, int tol = ANGLE_TOLERANCE) { return deg <= (fsmState.unlockDeg + tol); }

// Check if at lock position (open-ended tolerance: only check lower bound)
bool isAtLock(int deg, int tol = ANGLE_TOLERANCE) { return deg >= (fsmState.lockDeg - tol); }

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
        fsmState.curCmd = cmd;
#ifndef UNIT_TEST
        myservo.attachAndWrite(fsmState.lockDeg);
#endif
        Serial.print("FSM: UNLOCK -> BUSY_MOVE (locking), deg=");
        Serial.println(deg);
      } else if (isAtLock(deg)) {
        nextState = LOCK;
        Serial.print("FSM: UNLOCK -> LOCK, deg=");
        Serial.println(deg);
      } else if (!isAtLock(deg) && !isAtUnlock(deg)) {
        nextState = BUSY_WAIT;
        Serial.print("FSM: UNLOCK -> BUSY_WAIT (manual turn detected), deg=");
        Serial.println(deg);
      }
      break;

    case LOCK:
      if (isAtLock(deg) && cmd == UNLOCK_CMD) {
        nextState = BUSY_MOVE;
        fsmState.startTime = millis;
        fsmState.curCmd = cmd;
#ifndef UNIT_TEST
        myservo.attachAndWrite(fsmState.unlockDeg);
#endif
        Serial.print("FSM: LOCK -> BUSY_MOVE (unlocking), deg=");
        Serial.println(deg);
      } else if (isAtUnlock(deg)) {
        nextState = UNLOCK;
        Serial.print("FSM: LOCK -> UNLOCK, deg=");
        Serial.println(deg);
      } else if (!isAtLock(deg) && !isAtUnlock(deg)) {
        nextState = BUSY_WAIT;
        Serial.print("FSM: LOCK -> BUSY_WAIT (manual turn detected), deg=");
        Serial.println(deg);
      }
      break;

    case BUSY_WAIT:
      // No timeout - user can manually turn for as long as they want
      if (isAtUnlock(deg)) {
        nextState = UNLOCK;
        Serial.print("FSM: BUSY_WAIT -> UNLOCK, deg=");
        Serial.println(deg);
      } else if (isAtLock(deg)) {
        nextState = LOCK;
        Serial.print("FSM: BUSY_WAIT -> LOCK, deg=");
        Serial.println(deg);
      }
      // Otherwise stay in BUSY_WAIT
      break;

    case BUSY_MOVE:
      if (millis - fsmState.startTime > TOL) {
        nextState = BAD;
        myservo.detach();
        Serial.println("FSM: BUSY_MOVE -> BAD (timeout)");
      } else if (fsmState.curCmd == UNLOCK_CMD && isAtUnlock(deg, 0)) {
        // We should be use a stricter tolerance to determine whether to stop
        // moving.
        nextState = UNLOCK;
        fsmState.curCmd = NONE;
#ifndef UNIT_TEST
        myservo.detach();
#endif
        Serial.println("FSM: BUSY_MOVE -> UNLOCK");
      } else if (fsmState.curCmd == LOCK_CMD && isAtLock(deg, 0)) {
        // We should be use a stricter tolerance to determine whether to stop
        // moving.
        nextState = LOCK;
        fsmState.curCmd = NONE;
#ifndef UNIT_TEST
        myservo.detach();
#endif
        Serial.println("FSM: BUSY_MOVE -> LOCK");
      }
      break;

    case BAD:
      // Stay in BAD state - requires manual reset
      Serial.println("FSM: In BAD state - reset required");
#ifndef UNIT_TEST
      myservo.detach();
#endif
      break;
  }

  fsmState.currentState = nextState;
}

Request getTopRequest(WiFiClient& client) {
  if (!client) return EMPTY;

  // Serial.println("new client");
  String nonce = "";
  String signature = "";
  bool isStatus = false;
  bool isPostLock = false;
  bool isPostUnlock = false;
  bool isOptions = false;

  String currentLine = "";
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      // Serial.write(c);

      if (c == '\n') {
        // End of the headers of this request, we ignore request bodies.
        if (currentLine.length() == 0) {
          // // Only process the top request in the buffer
          // client.flush();

          if (isOptions) {
            return OPTIONS;
          } else if (isPostLock && verifyAuthentication(nonce, signature)) {
            return LOCK_REQ;
          } else if (isPostUnlock && verifyAuthentication(nonce, signature)) {
            return UNLOCK_REQ;
          } else if (isStatus && verifyAuthentication(nonce, signature)) {
            return STATUS;
          } else {
            // If authentication fails, treat as an unrecognized request (i.e.
            // 403 access forbidden), similar to how GitHub treats access to
            // private repos when the access token doesn't match.
            return UNRECOGNIZED;
          }
        } else {
          // Parse headers
          if (currentLine.startsWith("OPTIONS /lock") ||
              currentLine.startsWith("OPTIONS /unlock") ||
              currentLine.startsWith("OPTIONS /status")) {
            isOptions = true;
            // Serial.println("Received OPTIONS request");
          } else if (currentLine.startsWith("GET /status")) {
            isStatus = true;
            // Serial.println("Received GET /status");
            // } else if (currentLine.startsWith("POST /connect")) {
            //   isPostConnect = true;
            //   Serial.println("Received POST /connect");
          } else if (currentLine.startsWith("POST /lock")) {
            isPostLock = true;
            // Serial.println("Received LOCK request");
          } else if (currentLine.startsWith("POST /unlock")) {
            isPostUnlock = true;
            // Serial.println("Received UNLOCK request");
          } else if (currentLine.startsWith("X-Nonce: ")) {
            nonce = currentLine.substring(9);
            nonce.trim();
            // Serial.print("Nonce: ");
            // Serial.println(nonce);
          } else if (currentLine.startsWith("X-Signature: ")) {
            signature = currentLine.substring(13);
            signature.trim();
            // Serial.print("Signature: ");
            // Serial.println(signature);
          }

          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }

  // Final fallback, we should never reach here but it doesn't hurt to add a
  // case.
  return UNRECOGNIZED;
}

void respondHTTP(WiFiClient& client, int code, String codeName, String body, String extraHeaders) {
  // Line 1
  client.print("HTTP/1.1 ");
  client.print(code);
  client.print(" ");
  client.println(codeName);

  // Line 3
  client.println("Content-type:text/plain");

  // Line 2
  client.println("Access-Control-Allow-Origin: *");

  // Extra headers:
  if (extraHeaders.length() > 0) {
    client.println(extraHeaders);
  }

  client.println();
  if (body.length() > 0) {
    // Body
    client.println(body);
  }
  client.println();
}

void respondRequest(WiFiClient& client, Request req, State st) {
  if (req == EMPTY) return;
  assert(client);

  if (req == OPTIONS) {
    respondHTTP(client, 204, "No Content", "",
                "Access-Control-Allow-Headers: Content-Type, X-Nonce, "
                "X-Signature\nAccess-Control-Allow-Methods: GET, POST, OPTIONS");
  } else if (req == LOCK_REQ && (st == LOCK || st == BUSY_MOVE)) {
    respondHTTP(client, 200, "OK", stateToString(st), "");
  } else if (req == UNLOCK_REQ && (st == UNLOCK || st == BUSY_MOVE)) {
    respondHTTP(client, 200, "OK", stateToString(st), "");
  } else if (req == UNRECOGNIZED) {
    respondHTTP(client, 403, "Forbidden", "", "");
  } else if (req == STATUS) {
    respondHTTP(client, 200, "OK", stateToString(st), "");
  } else {
    // This is the case where we attempt to lock/unlock but for whatever reason
    // this request cannot be processed (e.g. FSM is in BUSY_WAIT)
    respondHTTP(client, 503, "Service Unavailable", stateToString(st), "");
  }

  client.stop();
  Serial.println("client disconnected");
}

// Include test files if testing is enabled
#ifdef UNIT_TEST
#include "doorlock_unit_tests.h"
#endif

#ifdef INTEGRATION_TEST
#include "doorlock_integration_tests.h"
#endif

void setup() {
  Serial.begin(9600);
  while (!Serial);

#ifdef INTEGRATION_TEST
  // Run integration tests with HTTP server enabled
  Serial.println("Running integration tests...");

  // Initialize EEPROM for authentication
  EEPROM.put(EEPROM_TIMESTAMP_ADDR, 0);

  // WiFi setup for HTTP testing
  Serial.println("Setting up WiFi for integration tests...");
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
    delay(2000);
  }
  server.begin();
  printWifiStatus();

  // Hardware setup
  myservo.init();
  myservo.calibrate(UNLOCK_ANGLE, LOCK_ANGLE);
  Serial.print("minFeedback: ");
  Serial.println(myservo.minFeedback);
  Serial.print("maxFeedback: ");
  Serial.println(myservo.maxFeedback);
  delay(1000);

  // Initialize FSM state
  fsmState.currentState = UNLOCK;
  fsmState.lockDeg = LOCK_ANGLE;
  fsmState.unlockDeg = UNLOCK_ANGLE;
  fsmState.startTime = 0;
  fsmState.curCmd = NONE;

  Serial.println("Integration test setup complete.");
  Serial.println("Server is ready to accept requests.");
  delay(1000);  // Give server a moment to be ready

  // Run integration tests (fetch() will call processServerRequest() to handle requests)
  runIntegrationTests();

#elif defined(UNIT_TEST)
  // Run unit tests
  Serial.println("Running unit tests...");
  runUnitTests();

#else
  // The actual remote door lock!

#ifdef RESET_TIMESTAMP
  EEPROM.put(EEPROM_TIMESTAMP_ADDR, 0);
#endif

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
    delay(2000);
  }
  server.begin();
  printWifiStatus();

  myservo.init();
  myservo.calibrate(UNLOCK_ANGLE, LOCK_ANGLE);
  Serial.print("minFeedback: ");
  Serial.println(myservo.minFeedback);
  Serial.print("maxFeedback: ");
  Serial.println(myservo.maxFeedback);
  Serial.print("minPoFeedback: ");
  Serial.println(myservo.minPoFeedback);
  Serial.print("maxPoFeedback: ");
  Serial.println(myservo.maxPoFeedback);
  delay(1000);

  // Initialize EEPROM (virtualEEPROM for Uno R4)
  // No explicit begin() needed for Uno R4
  unsigned long storedTimestamp = 0;
  EEPROM.get(EEPROM_TIMESTAMP_ADDR, storedTimestamp);
  Serial.print("Stored timestamp: ");
  Serial.println(storedTimestamp);

  // Initialize FSM state
  fsmState.currentState = CALIBRATE_LOCK;
  fsmState.lockDeg = LOCK_ANGLE;
  fsmState.unlockDeg = UNLOCK_ANGLE;
  fsmState.startTime = 0;
  fsmState.curCmd = NONE;

  Serial.println("FSM initialized in CALIBRATE_LOCK state");
  Serial.println("Hardcoded angles - Lock: 110, Unlock: 40");

  // Display initial state
  updateMatrixDisplay();

  // Run through calibration states. Manually rotate the motor to simulate user
  // calibration.
  // TODO: remove once we implement power cutoff for the servo to release
  // control of the motor
  fsmTransition(LOCK_ANGLE, millis(), NONE, NONE);
  fsmTransition(UNLOCK_ANGLE, millis(), NONE, NONE);
  myservo.attachAndWrite(UNLOCK_ANGLE);
  delay(2000);
  myservo.detach();

  Serial.println("FSM ready - now in UNLOCK state");

  WDT.begin(wdtInterval);
#endif
}

void loop() {
#ifndef TESTING
  // Parse HTTP request (if any) and obtain the corresponding command.
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Has client available!");
  }
  Request req = getTopRequest(client);
  Command cmd = requestToCommand(req);

  // Get current servo position
  int currentDeg = myservo.deg();

  // Run FSM transition
  fsmTransition(currentDeg, millis(), NONE, cmd);

  // Respond to request, if any
  respondRequest(client, req, fsmState.currentState);

  // Update LED matrix display
  updateMatrixDisplay();

  // Pet watchdog
  WDT.refresh();

  // Small delay
  delay(100);
#endif
}
