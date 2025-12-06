/*
 * INTEGRATION TESTS FOR DOORLOCK SYSTEM
 * 
 * End-to-end tests using HTTP requests to test the complete client-server interaction
 * These tests require actual hardware and test real-time behavior
 */

#ifndef DOORLOCK_INTEGRATION_TESTS_H
#define DOORLOCK_INTEGRATION_TESTS_H

// Test password (should match REMOTE_LOCK_PASS from arduino_secrets.h)
// IMPORTANT: Update this to match your REMOTE_LOCK_PASS in arduino_secrets.h
// Default from LEDwebserver/arduino_secrets.h is "randomlychosenpass"
const char* TEST_PASSWORD = "randomlychosenpass";

// Test result structure
struct HTTPTestResult {
  bool passed;
  String message;
  int statusCode;
  String responseBody;
};

// Helper function to convert byte array to hex string
String bytesToHex(unsigned char* bytes, int len) {
  String hex = "";
  for (int i = 0; i < len; i++) {
    if (bytes[i] < 16) hex += "0";
    hex += String(bytes[i], HEX);
  }
  return hex;
}

// Generate HMAC-SHA256 signature for testing
String generateHMACSignature(const String& nonce, const char* password) {
  unsigned char hmac[32];
  computeHMAC(nonce, password, hmac);
  return bytesToHex(hmac, 32);
}

// Fetch-like function for Arduino (simplified HTTP client)
// Similar to: fetch(url, { method, headers })
// Usage: fetch("/status", "GET", "", "") or fetch("/lock", "POST", nonce, signature)
HTTPTestResult fetch(const String& path, const String& method = "GET",
                     const String& nonce = "", const String& signature = "") {
  HTTPTestResult result;
  result.passed = false;
  result.statusCode = 0;
  result.message = "";
  result.responseBody = "";

  // Connect to localhost (same Arduino)
  // In real client: fetch(`http://${params.serverAddress}/${path}`, ...)
  WiFiClient client;
  IPAddress serverIP = WiFi.localIP();
  
  Serial.print("Connecting to server at ");
  Serial.println(serverIP);
  
  if (!client.connect(serverIP, 80)) {
    result.message = "Failed to connect to server";
    Serial.println(result.message);
    return result;
  }

  // Send HTTP request (mimicking fetch() API behavior)
  // Method and path
  client.print(method);
  client.print(" ");
  client.print(path);
  client.println(" HTTP/1.1");
  
  // Host header (required)
  client.print("Host: ");
  client.println(serverIP);
  
  // Connection header
  client.println("Connection: close");
  
  // Authentication headers (if provided)
  // In real client: headers: { 'X-Nonce': nonce, 'X-Signature': signature }
  if (nonce.length() > 0 && signature.length() > 0) {
    client.print("X-Nonce: ");
    client.println(nonce);
    client.print("X-Signature: ");
    client.println(signature);
  }
  
  // End of headers (empty line)
  client.println();
  client.flush();  // Ensure request is sent

  // Wait for response (with timeout)
  // In real client: fetch() handles this automatically
  // Since tests run in loop() and block it, we need to manually process server requests
  unsigned long timeout = millis() + 5000; // 5 second timeout
  int waitCount = 0;
  while (!client.available() && millis() < timeout) {
    // Manually process server requests since we're blocking loop()
    processServerRequest();
    delay(10);
    waitCount++;
    if (waitCount % 100 == 0) {  // Print every 1 second
      Serial.print("Waiting for response... ");
      Serial.println(millis());
    }
  }

  if (!client.available()) {
    result.message = "Timeout waiting for response";
    Serial.println(result.message);
    client.stop();
    return result;
  }

  // Read response headers and body
  // In real client: response.text() or response.json() handles this
  String response = "";
  unsigned long responseTimeout = millis() + 3000; // 3 second timeout for reading
  unsigned long lastByteTime = millis();
  while (client.connected() && millis() < responseTimeout) {
    if (client.available()) {
      char c = client.read();
      response += c;
      lastByteTime = millis(); // Update last byte time
      responseTimeout = lastByteTime + 500; // Reset timeout, but shorter (500ms after last byte)
    } else {
      // If no data for 500ms after last byte, assume response is complete
      if (millis() - lastByteTime > 500 && response.length() > 0) {
        break;
      }
      // Continue processing server requests while reading response
      processServerRequest();
      delay(10);
    }
  }
  
  // Debug: print raw response (first 200 chars)
  Serial.print("Raw response (first 200 chars): ");
  if (response.length() > 200) {
    Serial.println(response.substring(0, 200));
  } else {
    Serial.println(response);
  }

  // Parse status code (e.g., "HTTP/1.1 200 OK")
  int statusStart = response.indexOf("HTTP/1.1 ");
  if (statusStart >= 0) {
    int codeStart = statusStart + 9;
    int codeEnd = response.indexOf(" ", codeStart);
    if (codeEnd > codeStart) {
      result.statusCode = response.substring(codeStart, codeEnd).toInt();
    }
  }

  // Extract response body (after double newline)
  // Handle both \r\n\r\n and \n\n
  int bodyStart = response.indexOf("\r\n\r\n");
  if (bodyStart < 0) {
    bodyStart = response.indexOf("\n\n");
    if (bodyStart >= 0) bodyStart += 2;
  } else {
    bodyStart += 4;
  }
  
  if (bodyStart >= 0 && bodyStart < (int)response.length()) {
    result.responseBody = response.substring(bodyStart);
    result.responseBody.trim();
  }

  client.stop();
  result.passed = (result.statusCode > 0);
  
  Serial.print("HTTP Response: ");
  Serial.print(result.statusCode);
  Serial.print(" - ");
  Serial.println(result.responseBody);
  
  return result;
}

// Helper: Generate nonce and signature for authenticated requests
struct AuthHeaders {
  String nonce;
  String signature;
};

AuthHeaders generateAuth(const char* password) {
  AuthHeaders auth;
  auth.nonce = String(millis());  // Equivalent to Date.now().toString()
  auth.signature = generateHMACSignature(auth.nonce, password);
  return auth;
}

// Simplified API functions using fetch()
// These match the actual client API from mobile-lock-control/app/api/api.ts

// GET /status - Mimics: getLockStatus()
HTTPTestResult getStatus() {
  return fetch("/status", "GET");
}

// POST /connect - Mimics: pingLockServer()
HTTPTestResult connectToServer(const char* password) {
  AuthHeaders auth = generateAuth(password);
  return fetch("/connect", "POST", auth.nonce, auth.signature);
}

// POST /lock - Mimics: pushMotorCommand() with MotorCommand.Lock
HTTPTestResult sendLockCommand(const char* password) {
  AuthHeaders auth = generateAuth(password);
  return fetch("/lock", "POST", auth.nonce, auth.signature);
}

// POST /unlock - Mimics: pushMotorCommand() with MotorCommand.Unlock
HTTPTestResult sendUnlockCommand(const char* password) {
  AuthHeaders auth = generateAuth(password);
  return fetch("/unlock", "POST", auth.nonce, auth.signature);
}

// OPTIONS request (CORS preflight)
bool sendOPTIONSRequest(const String& path) {
  HTTPTestResult result = fetch(path, "OPTIONS");
  return result.passed && result.statusCode == 204;
}

// Wait for FSM to reach a specific state by polling /status
// Note: loop() must be running for server to process requests
bool waitForState(State targetState, unsigned long timeoutMs) {
  unsigned long startTime = millis();
  
  while (millis() - startTime < timeoutMs) {
    // Process server requests to allow FSM to continue running
    // This is important because FSM needs to check position and transition states
    processServerRequest();
    
    HTTPTestResult result = getStatus();
    
    if (result.passed && result.statusCode == 200) {
      String stateStr = result.responseBody;
      State currentState;
      
      // Parse state string
      if (stateStr == "LOCK") currentState = LOCK;
      else if (stateStr == "UNLOCK") currentState = UNLOCK;
      else if (stateStr == "BUSY_MOVE") currentState = BUSY_MOVE;
      else if (stateStr == "BUSY_WAIT") currentState = BUSY_WAIT;
      else if (stateStr == "BAD") currentState = BAD;
      else continue;
      
      if (currentState == targetState) {
        return true;
      }
    }
    
    delay(200); // Poll every 200ms
  }
  
  return false;
}

/*
 * INTEGRATION TEST 1: HTTP End-to-End Test - LOCK to UNLOCK
 * Action: Send HTTP POST /unlock request and verify motor rotates from LOCK to UNLOCK
 * Expected: HTTP 200 response, state transitions: LOCK -> BUSY_MOVE -> UNLOCK
 */
bool testHTTPLockToUnlock() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 1: HTTP E2E LOCK -> UNLOCK");
  Serial.println("========================================");
  
  // Initialize FSM to LOCK state
  fsmState.currentState = LOCK;
  fsmState.lockDeg = LOCK_ANGLE;
  fsmState.unlockDeg = UNLOCK_ANGLE;
  fsmState.startTime = 0;
  fsmState.curCmd = NONE;
  myservo.write(LOCK_ANGLE);
  delay(2000); // Wait for motor to reach lock position
  
  // Ensure FSM state matches physical position by running a transition
  // This ensures getCurrentDeg() and FSM state are in sync
  int currentDeg = getCurrentDeg();
  fsmTransition(currentDeg, millis(), NONE, NONE);
  
  Serial.print("Starting from LOCK state, current position: ");
  Serial.println(currentDeg);
  
  // Step 1: Send OPTIONS request (CORS preflight)
  Serial.println("Step 1: Sending OPTIONS request...");
  if (!sendOPTIONSRequest("/unlock")) {
    Serial.println("✗ OPTIONS request failed");
    return false;
  }
  Serial.println("✓ OPTIONS request successful");
  delay(100);
  
  // Step 2: Send POST /unlock request
  Serial.println("Step 2: Sending POST /unlock request...");
  HTTPTestResult result = sendUnlockCommand(TEST_PASSWORD);
  
  // Give server time to process request (loop() will handle it)
  delay(100);
  
  if (!result.passed || result.statusCode != 200) {
    Serial.print("✗ POST /unlock failed: ");
    Serial.print(result.statusCode);
    Serial.print(" - ");
    Serial.println(result.message);
    return false;
  }
  Serial.println("✓ POST /unlock request successful");
  
  // Step 3: Poll status until UNLOCK is reached
  Serial.println("Step 3: Polling status until UNLOCK...");
  bool reachedUnlock = waitForState(UNLOCK, 10000); // 10 second timeout
  
  // Step 4: Verify final state
  HTTPTestResult statusResult = getStatus();
  bool finalStateCorrect = (statusResult.passed && 
                           statusResult.statusCode == 200 && 
                           statusResult.responseBody == "UNLOCK");
  
  Serial.println("\n--- Test Results ---");
  Serial.print("HTTP Request Status: ");
  Serial.println(result.statusCode);
  Serial.print("Reached UNLOCK state: ");
  Serial.println(reachedUnlock ? "YES" : "NO");
  Serial.print("Final Status Check: ");
  Serial.println(statusResult.responseBody);
  
  bool testPassed = (result.statusCode == 200) && reachedUnlock && finalStateCorrect;
  
  if (testPassed) {
    Serial.println("✓ TEST PASSED");
  } else {
    Serial.println("✗ TEST FAILED");
  }
  
  return testPassed;
}

/*
 * INTEGRATION TEST 2: HTTP End-to-End Test - UNLOCK to LOCK
 * Action: Send HTTP POST /lock request and verify motor rotates from UNLOCK to LOCK
 * Expected: HTTP 200 response, state transitions: UNLOCK -> BUSY_MOVE -> LOCK
 */
bool testHTTPUnlockToLock() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 2: HTTP E2E UNLOCK -> LOCK");
  Serial.println("========================================");
  
  // Initialize FSM to UNLOCK state
  fsmState.currentState = UNLOCK;
  fsmState.lockDeg = LOCK_ANGLE;
  fsmState.unlockDeg = UNLOCK_ANGLE;
  fsmState.startTime = 0;
  fsmState.curCmd = NONE;
  myservo.write(UNLOCK_ANGLE);
  delay(2000); // Wait for motor to reach unlock position
  
  // Ensure FSM state matches physical position by running a transition
  // This ensures getCurrentDeg() and FSM state are in sync
  int currentDeg = getCurrentDeg();
  fsmTransition(currentDeg, millis(), NONE, NONE);
  
  Serial.print("Starting from UNLOCK state, current position: ");
  Serial.println(currentDeg);
  
  // Step 1: Send OPTIONS request (CORS preflight)
  Serial.println("Step 1: Sending OPTIONS request...");
  if (!sendOPTIONSRequest("/lock")) {
    Serial.println("✗ OPTIONS request failed");
    return false;
  }
  Serial.println("✓ OPTIONS request successful");
  delay(100);
  
  // Step 2: Send POST /lock request
  Serial.println("Step 2: Sending POST /lock request...");
  
  // Check position before sending command
  int degBefore = getCurrentDeg();
  Serial.print("Position before command: ");
  Serial.print(degBefore);
  Serial.print(", isAtUnlock: ");
  Serial.println(isAtUnlock(degBefore));
  
  HTTPTestResult result = sendLockCommand(TEST_PASSWORD);
  
  // Give server time to process request
  delay(100);
  
  // Check FSM state after command
  Serial.print("FSM state after POST /lock: ");
  Serial.println(stateToString(fsmState.currentState));
  Serial.print("Position after command: ");
  Serial.println(getCurrentDeg());
  
  if (!result.passed || result.statusCode != 200) {
    Serial.print("✗ POST /lock failed: ");
    Serial.print(result.statusCode);
    Serial.print(" - ");
    Serial.println(result.message);
    return false;
  }
  Serial.println("✓ POST /lock request successful");
  
  // Step 3: Poll status until LOCK is reached
  Serial.println("Step 3: Polling status until LOCK...");
  bool reachedLock = waitForState(LOCK, 10000); // 10 second timeout
  
  // Step 4: Verify final state
  HTTPTestResult statusResult = getStatus();
  bool finalStateCorrect = (statusResult.passed && 
                           statusResult.statusCode == 200 && 
                           statusResult.responseBody == "LOCK");
  
  Serial.println("\n--- Test Results ---");
  Serial.print("HTTP Request Status: ");
  Serial.println(result.statusCode);
  Serial.print("Reached LOCK state: ");
  Serial.println(reachedLock ? "YES" : "NO");
  Serial.print("Final Status Check: ");
  Serial.println(statusResult.responseBody);
  
  bool testPassed = (result.statusCode == 200) && reachedLock && finalStateCorrect;
  
  if (testPassed) {
    Serial.println("✓ TEST PASSED");
  } else {
    Serial.println("✗ TEST FAILED");
  }
  
  return testPassed;
}

/*
 * INTEGRATION TEST 3: HTTP Authentication Test
 * Action: Test authentication with correct and incorrect passwords
 * Expected: Correct password returns 200, incorrect returns 401
 */
bool testHTTPAuthentication() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 3: HTTP Authentication");
  Serial.println("========================================");
  
  // Test 1: Correct password
  Serial.println("Test 3.1: Testing with correct password...");
  HTTPTestResult correctResult = connectToServer(TEST_PASSWORD);
  bool correctAuth = (correctResult.statusCode == 200);
  
  Serial.print("Status Code: ");
  Serial.println(correctResult.statusCode);
  Serial.print("Response: ");
  Serial.println(correctResult.responseBody);
  
  if (correctAuth) {
    Serial.println("✓ Correct password authentication passed");
  } else {
    Serial.println("✗ Correct password authentication failed");
  }
  
  delay(500);
  
  // Test 2: Incorrect password
  Serial.println("\nTest 3.2: Testing with incorrect password...");
  HTTPTestResult incorrectResult = connectToServer("wrongpassword");
  bool incorrectAuth = (incorrectResult.statusCode == 401);
  
  Serial.print("Status Code: ");
  Serial.println(incorrectResult.statusCode);
  Serial.print("Response: ");
  Serial.println(incorrectResult.responseBody);
  
  if (incorrectAuth) {
    Serial.println("✓ Incorrect password correctly rejected");
  } else {
    Serial.println("✗ Incorrect password not rejected");
  }
  
  bool testPassed = correctAuth && incorrectAuth;
  
  Serial.println("\n--- Test Results ---");
  if (testPassed) {
    Serial.println("✓ TEST PASSED - Authentication working correctly");
  } else {
    Serial.println("✗ TEST FAILED");
    Serial.print("Correct password test: ");
    Serial.println(correctAuth ? "PASS" : "FAIL");
    Serial.print("Incorrect password test: ");
    Serial.println(incorrectAuth ? "PASS" : "FAIL");
  }
  
  return testPassed;
}

/*
 * INTEGRATION TEST 4: HTTP Status Endpoint Test
 * Action: Test GET /status endpoint
 * Expected: Returns current FSM state as text
 */
bool testHTTPStatusEndpoint() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 4: HTTP Status Endpoint");
  Serial.println("========================================");
  
  // Set a known state
  fsmState.currentState = UNLOCK;
  fsmState.curCmd = NONE;
  delay(100); // Allow state to settle
  
  Serial.println("Testing GET /status endpoint...");
  HTTPTestResult result = getStatus();
  
  bool testPassed = (result.statusCode == 200 && 
                     (result.responseBody == "UNLOCK" || 
                      result.responseBody == "LOCK" || 
                      result.responseBody == "BUSY_MOVE" || 
                      result.responseBody == "BUSY_WAIT" ||
                      result.responseBody == "BAD"));
  
  Serial.print("Status Code: ");
  Serial.println(result.statusCode);
  Serial.print("Response Body: ");
  Serial.println(result.responseBody);
  Serial.print("Current FSM State: ");
  Serial.println(stateToString(fsmState.currentState));
  
  Serial.println("\n--- Test Results ---");
  if (testPassed) {
    Serial.println("✓ TEST PASSED - Status endpoint working correctly");
  } else {
    Serial.println("✗ TEST FAILED");
  }
  
  return testPassed;
}

/*
 * INTEGRATION TEST 5: Motor Behavior 3 (Legacy - kept for reference)
 * Action: Motor stops when experiencing human interference
 * Expected States: BUSY_MOVE -> BUSY_WAIT -> (LOCK or UNLOCK)
 * 
 * NOTE: This test requires manual intervention during execution
 */
bool testMotorWithInterference() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 3: Motor with Interference");
  Serial.println("========================================");
  Serial.println("MANUAL INTERVENTION REQUIRED:");
  Serial.println("1. Test will start motor movement");
  Serial.println("2. Manually rotate the lock during movement");
  Serial.println("3. Motor should detect interference and enter BUSY_WAIT");
  Serial.println("4. Wait 5 seconds, then press any key to continue...");
  
  delay(5000);
  
  // Start from UNLOCK state
  fsmState.currentState = UNLOCK;
  fsmState.curCmd = NONE;
  myservo.write(UNLOCK_ANGLE);
  delay(2000);
  
  Serial.println("Test: Manual rotation detection");
  Serial.println("Step 1: Manually rotate lock to intermediate position (between LOCK and UNLOCK)");
  Serial.println("Step 2: System should detect intermediate position and enter BUSY_WAIT");
  Serial.println("Waiting 3 seconds for manual rotation...");
  delay(3000);
  
  // Check if manual rotation was detected (should enter BUSY_WAIT from UNLOCK)
  int currentDeg = getCurrentDeg();
  fsmTransition(currentDeg, millis(), NONE, NONE);
  
  Serial.print("State after manual rotation: ");
  Serial.println(stateToString(fsmState.currentState));
  Serial.print("Position: ");
  Serial.println(currentDeg);
  
  bool enteredBusyWait = (fsmState.currentState == BUSY_WAIT);
  
  if (enteredBusyWait) {
    Serial.println("✓ Detected manual rotation - entered BUSY_WAIT");
    
    // Now manually rotate to LOCK position and verify system recognizes it
    Serial.println("Step 3: Manually rotate to LOCK position...");
    delay(3000);
    
    currentDeg = getCurrentDeg();
    fsmTransition(currentDeg, millis(), NONE, NONE);
    
    Serial.print("State after reaching LOCK: ");
    Serial.println(stateToString(fsmState.currentState));
  }
  
  // Verify BUSY_WAIT was reached and final state is valid
  bool finalStateValid = (fsmState.currentState == LOCK || 
                          fsmState.currentState == UNLOCK || 
                          fsmState.currentState == BUSY_WAIT);
  
  Serial.println("\n--- Test Results ---");
  Serial.print("Entered BUSY_WAIT: ");
  Serial.println(enteredBusyWait ? "YES" : "NO");
  Serial.print("Final State: ");
  Serial.println(stateToString(fsmState.currentState));
  Serial.print("Final State Valid: ");
  Serial.println(finalStateValid ? "YES" : "NO");
  
  bool testPassed = enteredBusyWait && finalStateValid;
  
  if (testPassed) {
    Serial.println("✓ TEST PASSED");
  } else {
    Serial.println("✗ TEST FAILED");
    Serial.println("Did you manually rotate the lock during movement?");
  }
  
  return testPassed;
}

/*
 * INTEGRATION TEST 6: Position Detection (Direct hardware test)
 * Action: Test position detection functions (isAtLock, isAtUnlock) with actual hardware
 * Test Output: Position detection accuracy for LOCK and UNLOCK positions
 * Note: This is a direct hardware/FSM test, not an HTTP test
 */
bool testFSMPositionOutput() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 4: Position Detection");
  Serial.println("========================================");
  
  // Test LOCK position
  Serial.println("Testing LOCK position...");
  myservo.write(LOCK_ANGLE);
  delay(2000);
  
  int lockDeg = getCurrentDeg();
  bool isLockPos = isAtLock(lockDeg);
  
  Serial.print("Motor at LOCK angle (");
  Serial.print(LOCK_ANGLE);
  Serial.print("): ");
  Serial.println(lockDeg);
  Serial.print("Is at LOCK position: ");
  Serial.println(isLockPos ? "YES" : "NO");
  
  // Test UNLOCK position
  Serial.println("\nTesting UNLOCK position...");
  myservo.write(UNLOCK_ANGLE);
  delay(2000);
  
  int unlockDeg = getCurrentDeg();
  bool isUnlockPos = isAtUnlock(unlockDeg);
  
  Serial.print("Motor at UNLOCK angle (");
  Serial.print(UNLOCK_ANGLE);
  Serial.print("): ");
  Serial.println(unlockDeg);
  Serial.print("Is at UNLOCK position: ");
  Serial.println(isUnlockPos ? "YES" : "NO");
  
  bool testPassed = isLockPos && isUnlockPos;
  
  Serial.println("\n--- Test Results ---");
  if (testPassed) {
    Serial.println("✓ TEST PASSED - Position detection working correctly");
  } else {
    Serial.println("✗ TEST FAILED - Position detection incorrect");
    Serial.print("LOCK detection: ");
    Serial.println(isLockPos ? "PASS" : "FAIL");
    Serial.print("UNLOCK detection: ");
    Serial.println(isUnlockPos ? "PASS" : "FAIL");
  }
  
  return testPassed;
}

/*
 * INTEGRATION TEST 7: FSM Command Response (Direct FSM test)
 * Action: Send command directly to FSM and verify state change and motor response
 * Test Output: State transition and motor movement
 * Note: This is a direct FSM test, not an HTTP test
 */
bool testFSMCommandResponse() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 5: FSM Command Response");
  Serial.println("========================================");
  
  // Start from UNLOCK
  fsmState.currentState = UNLOCK;
  fsmState.curCmd = NONE;
  myservo.write(UNLOCK_ANGLE);
  delay(2000);
  
  Serial.println("Sending LOCK command...");
  
  int initialDeg = getCurrentDeg();
  State initialState = fsmState.currentState;
  
  // Send command
  int currentDeg = getCurrentDeg();
  fsmTransition(currentDeg, millis(), NONE, LOCK_CMD);
  
  State afterCommandState = fsmState.currentState;
  
  Serial.print("State before command: ");
  Serial.println(stateToString(initialState));
  Serial.print("State after command: ");
  Serial.println(stateToString(afterCommandState));
  
  // Check if command was acknowledged (state changed to BUSY_MOVE)
  bool commandAcknowledged = (afterCommandState == BUSY_MOVE);
  
  // Wait a bit and check if motor started moving
  delay(500);
  int newDeg = getCurrentDeg();
  bool motorMoving = (abs(newDeg - initialDeg) > 2); // Position changed by more than 2 degrees
  
  Serial.print("Command acknowledged: ");
  Serial.println(commandAcknowledged ? "YES" : "NO");
  Serial.print("Motor started moving: ");
  Serial.println(motorMoving ? "YES" : "NO");
  
  bool testPassed = commandAcknowledged && motorMoving;
  
  Serial.println("\n--- Test Results ---");
  if (testPassed) {
    Serial.println("✓ TEST PASSED - Command received and motor responded");
  } else {
    Serial.println("✗ TEST FAILED");
    Serial.print("Command acknowledged: ");
    Serial.println(commandAcknowledged ? "YES" : "NO");
    Serial.print("Motor moving: ");
    Serial.println(motorMoving ? "YES" : "NO");
  }
  
  return testPassed;
}

/*
 * INTEGRATION TEST 8: Watchdog Timeout (Direct FSM test)
 * Action: Test timeout detection in FSM
 * Test Output: BAD state reached
 */
bool testWatchdogTimeout() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 6: Watchdog Timeout");
  Serial.println("========================================");
  
  // Start from BUSY_MOVE state with old startTime
  fsmState.currentState = BUSY_MOVE;
  fsmState.startTime = millis() - TOL - 1000; // Set startTime to 6 seconds ago (exceeds 5s timeout)
  fsmState.curCmd = LOCK_CMD; // Set a command for BUSY_MOVE state
  
  Serial.println("Simulating timeout condition...");
  Serial.print("Current time: ");
  Serial.println(millis());
  Serial.print("Start time: ");
  Serial.println(fsmState.startTime);
  Serial.print("Time elapsed: ");
  Serial.println(millis() - fsmState.startTime);
  
  // Run FSM transition - should detect timeout
  int currentDeg = getCurrentDeg();
  fsmTransition(currentDeg, millis(), NONE, NONE);
  
  bool reachedBadState = (fsmState.currentState == BAD);
  
  Serial.print("Final State: ");
  Serial.println(stateToString(fsmState.currentState));
  Serial.print("Reached BAD state: ");
  Serial.println(reachedBadState ? "YES" : "NO");
  
  bool testPassed = reachedBadState;
  
  Serial.println("\n--- Test Results ---");
  if (testPassed) {
    Serial.println("✓ TEST PASSED - Timeout detected correctly");
  } else {
    Serial.println("✗ TEST FAILED - Timeout not detected");
  }
  
  return testPassed;
}

/*
 * Run all integration tests
 * Returns true if all tests pass, false otherwise
 */
bool runIntegrationTests() {
  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("STARTING INTEGRATION TESTS");
  Serial.println("========================================");
  Serial.println("WARNING: These tests require actual hardware!");
  Serial.println("Make sure servo motor and feedback are connected.");
  Serial.println("Press any key in Serial Monitor to start...");
  
  while (!Serial.available()) {
    delay(100);
  }
  Serial.read(); // Clear the buffer
  
  bool allPassed = true;
  
  // HTTP End-to-End Tests
  allPassed &= testHTTPLockToUnlock();
  delay(2000);
  
  allPassed &= testHTTPUnlockToLock();
  delay(2000);
  
  allPassed &= testHTTPAuthentication();
  delay(1000);
  
  allPassed &= testHTTPStatusEndpoint();
  delay(1000);
  
  // Additional Tests (can be HTTP-based or direct FSM)
  // allPassed &= testMotorWithInterference();
  // delay(2000);
  
  // allPassed &= testFSMPositionOutput();
  // delay(1000);
  
  // allPassed &= testFSMCommandResponse();
  // delay(1000);
  
  // allPassed &= testWatchdogTimeout();
  
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST SUMMARY");
  Serial.println("========================================");
  if (allPassed) {
    Serial.println("✓ ALL TESTS PASSED");
  } else {
    Serial.println("✗ SOME TESTS FAILED");
  }
  Serial.println("========================================");
  
  return allPassed;
}

#endif // DOORLOCK_INTEGRATION_TESTS_H

