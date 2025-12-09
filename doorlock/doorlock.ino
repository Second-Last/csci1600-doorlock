#include <ArduinoBearSSL.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <EEPROM.h>
#include <Servo.h>
#include <WDT.h>
#include <WiFiS3.h>

#include "utils.h"
#include "myservo.hpp"

// This files controls whether to run testing, secrets, and other configurations
// of the doorlock.
#include "config.h"

#if defined(INTEGRATION_TEST) && defined(UNIT_TEST)
#error "INTEGRATION_TEST and UNIT_TEST cannot be both defined!"
#elif defined(INTEGRATION_TEST) || defined(UNIT_TEST)
#define TESTING
#endif

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
const int MAX_LOCK_ANGLE = 110;
const int MIN_UNLOCK_ANGLE = 40;

// Angle tolerance for position checking (degrees)
const int ANGLE_TOLERANCE = 5;

// Global FSM state (must be defined before test headers are included)
FSMState fsmState;

ArduinoLEDMatrix matrix;



// Control and feedback pins
const int servoPin = 9;
const int feedbackPin = A0;
const int transistorPin = 5;
const int calibrateBtnPin = 3;

volatile bool calibrateBtnPressed = false;

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

/**
 * This function converts an inputted value of type State into its String equivalent.
 * 
 * Input:
 *  - st (State) : Represents the current state that this function will convert to its String equivalent
 * 
 * Output: String value that represents the current state
 * 
 */
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

/**
 * This function is a helper function that physically displays the current status on the Arduino's LED matrix.
 * 
 * Input:
 *  - text (char*) : String representing the status we aim to display on the LED matrix
 * 
 * Output: None
 * 
 * Side effects: draws the given text on the Arduino's LED matrix.
 */
void displayText(const char* text) {
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(text);
  matrix.endText();
  matrix.endDraw();
}

/**
 * This function prints the MAC address, which is necessary for connecting the Arduino to the "Brown-Guest"
 * wifi
 * 
 * Input:
 *  - mac (byte array) : Byte array representing the MAC address, which will be printed out
 * 
 * Output: None
 * 
 * Side effect: prints the MAC address to the serial console.
 */
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

/**
 * This function prints the current WiFi metadata, including
 *  - Arduino IP Address
 *  - Arduino MAC address
 * 
 * Input: None
 * 
 * Output: None
 *
 * Side effects: prints the aforementioned WiFI metadata to the serial console.
 */
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
}

/**
 * This function updates the Arduino's LED matrix based on the FSM state. It is called after fsmTransition()
 * to ensure the Arduino always displays the correct state. Note that it only
 * writes when the FSM state has changed compared to the last displayed state.
 * 
 * Input: None
 * Output: None
 *
 * Side effect: updates the Arduino's LED matrix to display an abbreviation of
 * the current FSM state only if the current state is different from the last
 * state that was displayed.
 */
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

/**
 * This function serves as the ISR that is called when the button is pressed. It updates the boolean
 * `calibrateBtnPressed` to indicate that the button to calibrate the lock and unlock states have been pressed
 * 
 * Input: None
 * Output: None
 * 
 * Side effects: sets `calibrateBtnPressed` as true.
 */
void calibrateBtnIsr(void){
  calibrateBtnPressed = true;
}

/**
 * This is a helper function to convert a hexadecimal value into a decimal value
 * 
 * Input:
 *  - c (char) : Character representing some digit in hexadecimal
 * 
 * Output: Integer (int) value equivalent to the inputted hexadecimal value, but in decimal format
 */
int hexCharToValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/**
 * This helper function takes in a String that represents a hexadecimal and attempts to 
 * convert it to a byte array. It returns a boolean indicating if it can be converted or not.
 * 
 * Input:
 *  - hex (String&) : Reference to a String value that corresponds to some hexadecimal value
 *  - output (char*) : String value that will eventually stored the converted hexadecimal (now a byte array)
 *  - outputLen (size_t) : size_t indicating the expected size of the `output` variables
 * 
 * Output: bool indicating if the conversion was successful
 * 
 */
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

/**
 * This is a helper function responsible for computing the HMAC value of a message given the message contant
 * and a key for hashing. HMAC is a hash function used to encrypt a message with a shared private key.
 * 
 * Input:
 *  - message (String&) : Reference to a String that corresponds to the message we hope to hash
 *  - key (char*) : String value representing the private key used to hash the message via HMAC
 *  - output (char*) : String where the output of the hashing should be stored
 * 
 * Output: None
 * 
 */
void computeHMAC(const String& message, const char* key, unsigned char* output) {
  br_hmac_key_context keyCtx;
  br_hmac_key_init(&keyCtx, &br_sha256_vtable, key, strlen(key));

  br_hmac_context hmacCtx;
  br_hmac_init(&hmacCtx, &keyCtx, 0);
  br_hmac_update(&hmacCtx, message.c_str(), message.length());
  br_hmac_out(&hmacCtx, output);
}

/**
 * This helper function checks whether two strings are equal (have equal content)
 * using cosntant-time comparison. In reality no one is going to execute timing
 * attacks on our Arduino, but it's not like this function will make our code
 * worse so why not!
 *
 * Assumes the two strings being compared have the same length.
 * 
 * Input:
 *  - a (unsigned char*): a string
 *  - b (unsigned char*): a string
 *  - len (size_t): length of `a` and `b`. `a` and `b` are assumed to have the
 *  same length.
 *
 * Output: bool value indicating whether the two strings are equal.
 */
bool constantTimeCompare(const unsigned char* a, const unsigned char* b, size_t len) {
  unsigned char result = 0;
  for (size_t i = 0; i < len; i++) {
    result |= a[i] ^ b[i];
  }
  return result == 0;
}

/**
 * This function ensure that the signature of the nonce was signed using the
 * secret key using HMAC-SHA256, and that the nonce is at least 5 seconds
 * after the last successful request's nonce.
 *
 * Note that by defining the `SKIP_AUTH` macro, this function always returns
   * true (i.e. skips authentication).
 *
 * Input:
 *  - nonce (const String&): the nonce (unix timestamp) formatted as a string
 *  - signature (const String&): the signature of the nonce in hex.
 *
 * Output: bool value that indicates whether the authentication was successful.
 *
 * Side effect:
 * If the authentication succeeds, updates the last nonce stored EEPROM to be
 * the current nonce.
 */
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

/**
 * This function checks if the servo motor is currently in the UNLOCKED state. It takes the current degree and compares
 * it to the expected unlock degree, with a bit of tolerance.
 * 
 * Input:
 *  - deg (int) : Integer representing the current degree of the motor
 * 
 * Output: Bool value indicating if the current motor is at the UNLOCK position
 * 
 */
bool isAtUnlock(int deg) { return deg <= (fsmState.unlockDeg + ANGLE_TOLERANCE); }

/**
 * This function checks if the servo motor is currently in the LOCKED state. It takes the current degree and compares
 * it to the expected lock degree, with a bit of tolerance.
 * 
 * Input:
 *  - deg (int) : Integer representing the current degree of the motor
 * 
 * Output: Bool value indicating if the current motor is at the LOCK position
 * 
 */
bool isAtLock(int deg) { return deg >= (fsmState.lockDeg - ANGLE_TOLERANCE); }

/**
 * The fsmTransition() function is the key function responsible for handling the finite-state machine logic.
 * It takes in all the expected inputs and, with the help of the environment variables, determines the next state
 * that the FSM should transition to.
 *
 * The annotatinos for transitions are the `Serial.println`s so no further
 * commenting exists for transitions; the prints should be obvious enough.
 * 
 * Input:
 *  - deg (int) : Integer value representing the current motor position in degrees
 *  - millis (unsigned long) : long value indicating the current time, in milliseconds
 *  - button (bool) : bool value indicating if the calibrate button has been pressed or not
 *  - cmd (Command) : Command object representing the command sent by the user (if one has been sent)
 * 
 * Output: None
 * 
 * Side effect:
 * Update the global `fsmState` with the updated FSM variables and the next state that the FSM should transition to.
 */
void fsmTransition(int deg, unsigned long millis, bool button, Command cmd) {
  State nextState = fsmState.currentState;

  switch (fsmState.currentState) {
    case CALIBRATE_LOCK:
      if (button) {
        fsmState.lockDeg = deg;
        nextState = CALIBRATE_UNLOCK;
        Serial.print("FSM: CALIBRATE_LOCK -> CALIBRATE_UNLOCK with deg=");
        Serial.println(deg);
      }
      break;

    case CALIBRATE_UNLOCK:
      if (button) {
        fsmState.unlockDeg = deg;
        nextState = UNLOCK;
        Serial.print("FSM: CALIBRATE_UNLOCK -> UNLOCK with deg=");
        Serial.println(deg);
      }
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
      } else if (fsmState.curCmd == UNLOCK_CMD && isAtUnlock(deg)) {
        nextState = UNLOCK;
        fsmState.curCmd = NONE;
#ifndef UNIT_TEST
        myservo.detach();
#endif
        Serial.println("FSM: BUSY_MOVE -> UNLOCK");
      } else if (fsmState.curCmd == LOCK_CMD && isAtLock(deg)) {
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

/**
 * This function is simply responsible for handling all WiFi requests sent by the client to the current Arduino server.
 * Evidently, it parses the request, determines the type of request (GET, POST, OPTIONS, etc.), and sets necessary variables
 * to determine what to send back to the client.
 * 
 * Input:
 *  - client (WiFiClient&) : Reference to a WiFiClient, which represents the Arduino server in our application
 * 
 * Output: Request object that represents the current type of request sent. `Request` is an enum defined with set states 
 * 
 * Side effect: clears the buffer in the `client`.
 */
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
          // Only process the top request in the buffer
          client.flush();

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

/**
 * This function is responsible for generating the proper response to send back to the server based on the results of 
 * a previous request. It is a general helper function that can be used for any type of request.
 * 
 * Input:
 *  - client (WiFiClient&) : Reference to a WiFiClient that corresponds to a client that sent a request to the server;
 *                           is responsible for receiving requests and sending responses back.
 *  - code (int) : represents the status code that should be sent back to the client
 *  - body (String) : String representing the body that should be sent back to the client
 *  - extraHeaders (String) : String representing header content that will also be sent back to the client in the same response.
 *
 * Output: None
 *
 * Side effect:
 * Append the `client`'s (send) buffer with the header and contents of the HTTP
 * response.
 */
void respondHTTP(WiFiClient& client, int code, String codeName, String body, String extraHeaders) {
  // Line 1
  client.print("HTTP/1.1 ");
  client.print(code);
  client.print(" ");
  client.println(codeName);

  // Line 2
  client.println("Content-type:text/plain");

  // Line 3
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

/**
 * Responds to the HTTP client's request based on the current
 *
 * Input:
 *  - client (WifiClient&): the client that this request came from.
 *  - req (Request): the type of the client's request.
 *  - st (State): the current state of the FSM.
 *
 * Output: None
 *
 * Side effects:
 * Sends a HTTP response back to the client depending on the request and the
 * current FSM's state.
 */
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

/**
 * This is the `setup()` function that the Arduino will always run once code is uploaded. It is responsible for:
 *  - Connecting the Arduino to WiFi to receive requests from clients
 *  - Initializing the FSM state
 *  - Setting up the servo motor
 *  - Attaching interrupts to the button hardware
 * 
 * Input: None
 * Output: None
 * 
 */
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

  myservo.init();
  myservo.calibrate(MIN_UNLOCK_ANGLE, MAX_LOCK_ANGLE);
  Serial.print("minFeedback: ");
  Serial.println(myservo.minFeedback);
  Serial.print("maxFeedback: ");
  Serial.println(myservo.maxFeedback);
  Serial.print("minPoFeedback: ");
  Serial.println(myservo.minPoFeedback);
  Serial.print("maxPoFeedback: ");
  Serial.println(myservo.maxPoFeedback);
  delay(1000);

  // Initialize FSM state
  fsmState.currentState = UNLOCK;
  fsmState.lockDeg = MAX_LOCK_ANGLE;
  fsmState.unlockDeg = MIN_UNLOCK_ANGLE;
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

  // Hardware setup
  pinMode(calibrateBtnPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(calibrateBtnPin), calibrateBtnIsr, FALLING);

  // Servo self-calibration and initialization 
  myservo.init();
  myservo.calibrate(MIN_UNLOCK_ANGLE, MAX_LOCK_ANGLE);
  Serial.print("minFeedback: ");
  Serial.println(myservo.minFeedback);
  Serial.print("maxFeedback: ");
  Serial.println(myservo.maxFeedback);
  Serial.print("minPoFeedback: ");
  Serial.println(myservo.minPoFeedback);
  Serial.print("maxPoFeedback: ");
  Serial.println(myservo.maxPoFeedback);

  // Initialize EEPROM (virtualEEPROM for Uno R4)
  // No explicit begin() needed for Uno R4
  unsigned long storedTimestamp = 0;
  EEPROM.get(EEPROM_TIMESTAMP_ADDR, storedTimestamp);
  Serial.print("Stored timestamp: ");
  Serial.println(storedTimestamp);

  // Initialize FSM state
  fsmState.currentState = CALIBRATE_LOCK;
  fsmState.lockDeg = MAX_LOCK_ANGLE;
  fsmState.unlockDeg = MIN_UNLOCK_ANGLE;
  fsmState.startTime = 0;
  fsmState.curCmd = NONE;

  Serial.println("FSM initialized in CALIBRATE_LOCK state");

  // Display initial state
  updateMatrixDisplay();

  Serial.print("FSM ready - now in ");
  Serial.println(stateToString(fsmState.currentState));

  WDT.begin(wdtInterval);
#endif
}

/**
 * This `loop()` function is executed by the Arduino at most every 100 milliseconds. It is responsible for:
 *  - Processing any requests sent by clients to the current Arduino server
 *  - Calling fsmTransition() to update the current FSM state
 *  - Petting the watchdog to prevent Arduino reset
 * 
 * Input: None
 * Output: None 
 * 
 */
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

  bool btnPressed = false;
  noInterrupts();
  if (calibrateBtnPressed) {
    btnPressed = true; 
    calibrateBtnPressed = false;
  }
  interrupts();

  // Run FSM transition
  fsmTransition(currentDeg, millis(), btnPressed, cmd);

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
