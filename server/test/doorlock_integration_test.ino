/*
 * INTEGRATION TESTS FOR DOORLOCK SYSTEM
 * 
 * These tests require actual hardware and test real-time behavior
 * Enable with #define INTEGRATION_TESTING
 */

#include <Servo.h>

// Include doorlock FSM (in a real setup, this would be a shared header)
// For now, we'll duplicate necessary definitions

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
int tolerance = 5;

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

// Constants
const unsigned long TOL = 5000;
const int LOCK_ANGLE = 120;
const int UNLOCK_ANGLE = 50;
const int ANGLE_TOLERANCE = 3;

// Test state tracking
struct TestResult {
  bool passed;
  String message;
  State reachedStates[10];
  int stateCount;
};

TestResult currentTest;

// Include actual FSM logic from doorlock.ino
// (In production, this would be #include "doorlock_fsm.h")

void calibrate(Servo servo, int analogPin, int minPos, int maxPos) {
  servo.write(minPos);
  minDegrees = minPos;
  delay(2000);
  minFeedback = analogRead(analogPin);

  servo.write(maxPos);
  maxDegrees = maxPos;
  delay(2000);
  maxFeedback = analogRead(analogPin);
}

int getCurrentDeg() {
  int feedback = analogRead(feedbackPin);
  return map(feedback, minFeedback, maxFeedback, minDegrees, maxDegrees);
}

bool isAtUnlock(int deg) { 
  return deg <= (fsmState.unlockDeg + ANGLE_TOLERANCE); 
}

bool isAtLock(int deg) { 
  return deg >= (fsmState.lockDeg - ANGLE_TOLERANCE); 
}

void recordState(State s) {
  if (currentTest.stateCount < 10) {
    currentTest.reachedStates[currentTest.stateCount++] = s;
  }
}

void fsmTransition(int deg, unsigned long millis, Command button, Command cmd) {
  State nextState = fsmState.currentState;
  recordState(fsmState.currentState);

  switch (fsmState.currentState) {
    case CALIBRATE_LOCK:
      fsmState.lockDeg = deg;
      nextState = CALIBRATE_UNLOCK;
      break;

    case CALIBRATE_UNLOCK:
      fsmState.unlockDeg = deg;
      nextState = UNLOCK;
      break;

    case UNLOCK:
      if (isAtUnlock(deg) && cmd == LOCK_CMD) {
        nextState = BUSY_MOVE;
        fsmState.startTime = millis;
        myservo.write(fsmState.lockDeg);
      } else if (isAtLock(deg)) {
        nextState = LOCK;
      } else if (!isAtLock(deg) && !isAtUnlock(deg)) {
        nextState = BUSY_WAIT;
      }
      break;

    case LOCK:
      if (isAtLock(deg) && cmd == UNLOCK_CMD) {
        nextState = BUSY_MOVE;
        fsmState.startTime = millis;
        myservo.write(fsmState.unlockDeg);
      } else if (isAtUnlock(deg)) {
        nextState = UNLOCK;
      } else if (!isAtLock(deg) && !isAtUnlock(deg)) {
        nextState = BUSY_WAIT;
      }
      break;

    case BUSY_WAIT:
      if (isAtUnlock(deg)) {
        nextState = UNLOCK;
      } else if (isAtLock(deg)) {
        nextState = LOCK;
      }
      break;

    case BUSY_MOVE:
      if (millis - fsmState.startTime > TOL) {
        nextState = BAD;
      } else if (isAtUnlock(deg)) {
        nextState = UNLOCK;
      } else if (isAtLock(deg)) {
        nextState = LOCK;
      }
      break;

    case BAD:
      break;
  }

  fsmState.currentState = nextState;
}

void resetFSM() {
  fsmState.currentState = UNLOCK;
  fsmState.lockDeg = LOCK_ANGLE;
  fsmState.unlockDeg = UNLOCK_ANGLE;
  fsmState.startTime = 0;
  currentTest.stateCount = 0;
}

String stateToString(State s) {
  switch(s) {
    case CALIBRATE_LOCK: return "CALIBRATE_LOCK";
    case CALIBRATE_UNLOCK: return "CALIBRATE_UNLOCK";
    case UNLOCK: return "UNLOCK";
    case LOCK: return "LOCK";
    case BUSY_WAIT: return "BUSY_WAIT";
    case BUSY_MOVE: return "BUSY_MOVE";
    case BAD: return "BAD";
    default: return "UNKNOWN";
  }
}

bool containsState(State target, State states[], int count) {
  for (int i = 0; i < count; i++) {
    if (states[i] == target) return true;
  }
  return false;
}

/*
 * INTEGRATION TEST 1: Motor Behavior 1
 * Action: Motor fully rotates from LOCK to UNLOCK
 * Expected States: LOCK, BUSY_MOVE, UNLOCK
 */
bool testMotorLockToUnlock() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 1: LOCK -> UNLOCK");
  Serial.println("========================================");
  
  resetFSM();
  currentTest.passed = false;
  currentTest.message = "";
  
  // Start from LOCK state
  fsmState.currentState = LOCK;
  myservo.write(LOCK_ANGLE);
  delay(2000); // Wait for motor to reach lock position
  
  Serial.println("Starting from LOCK state");
  Serial.println("Sending UNLOCK command...");
  
  unsigned long startTime = millis();
  unsigned long timeout = 10000; // 10 second timeout for test
  
  // Send unlock command (only once, at the start)
  int currentDeg = getCurrentDeg();
  fsmTransition(currentDeg, millis(), NONE, UNLOCK_CMD);
  
  // Now monitor the movement until it reaches UNLOCK
  while (millis() - startTime < timeout) {
    currentDeg = getCurrentDeg();
    fsmTransition(currentDeg, millis(), NONE, NONE);  // No command during movement
    
    Serial.print("State: ");
    Serial.print(stateToString(fsmState.currentState));
    Serial.print(" | Position: ");
    Serial.println(currentDeg);
    
    // Check if we reached UNLOCK state
    if (fsmState.currentState == UNLOCK) {
      currentTest.passed = true;
      currentTest.message = "Successfully reached UNLOCK state";
      break;
    }
    
    delay(100);
  }
  
  // Verify expected states were reached
  bool hasLock = containsState(LOCK, currentTest.reachedStates, currentTest.stateCount);
  bool hasBusyMove = containsState(BUSY_MOVE, currentTest.reachedStates, currentTest.stateCount);
  bool hasUnlock = containsState(UNLOCK, currentTest.reachedStates, currentTest.stateCount);
  
  Serial.println("\n--- Test Results ---");
  Serial.print("Reached LOCK: ");
  Serial.println(hasLock ? "YES" : "NO");
  Serial.print("Reached BUSY_MOVE: ");
  Serial.println(hasBusyMove ? "YES" : "NO");
  Serial.print("Reached UNLOCK: ");
  Serial.println(hasUnlock ? "YES" : "NO");
  Serial.print("Final State: ");
  Serial.println(stateToString(fsmState.currentState));
  
  currentTest.passed = currentTest.passed && hasLock && hasBusyMove && hasUnlock;
  
  if (currentTest.passed) {
    Serial.println("✓ TEST PASSED");
  } else {
    Serial.println("✗ TEST FAILED");
    Serial.print("Reason: ");
    Serial.println(currentTest.message);
  }
  
  return currentTest.passed;
}

/*
 * INTEGRATION TEST 2: Motor Behavior 2
 * Action: Motor fully rotates from UNLOCK to LOCK
 * Expected States: UNLOCK, BUSY_MOVE, LOCK
 */
bool testMotorUnlockToLock() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 2: UNLOCK -> LOCK");
  Serial.println("========================================");
  
  resetFSM();
  currentTest.passed = false;
  currentTest.message = "";
  
  // Start from UNLOCK state
  fsmState.currentState = UNLOCK;
  myservo.write(UNLOCK_ANGLE);
  delay(2000); // Wait for motor to reach unlock position
  
  Serial.println("Starting from UNLOCK state");
  Serial.println("Sending LOCK command...");
  
  unsigned long startTime = millis();
  unsigned long timeout = 10000; // 10 second timeout for test
  
  // Send lock command (only once, at the start)
  int currentDeg = getCurrentDeg();
  fsmTransition(currentDeg, millis(), NONE, LOCK_CMD);
  
  // Now monitor the movement until it reaches LOCK
  while (millis() - startTime < timeout) {
    currentDeg = getCurrentDeg();
    fsmTransition(currentDeg, millis(), NONE, NONE);  // No command during movement
    
    Serial.print("State: ");
    Serial.print(stateToString(fsmState.currentState));
    Serial.print(" | Position: ");
    Serial.println(currentDeg);
    
    // Check if we reached LOCK state
    if (fsmState.currentState == LOCK) {
      currentTest.passed = true;
      currentTest.message = "Successfully reached LOCK state";
      break;
    }
    
    delay(100);
  }
  
  // Verify expected states were reached
  bool hasUnlock = containsState(UNLOCK, currentTest.reachedStates, currentTest.stateCount);
  bool hasBusyMove = containsState(BUSY_MOVE, currentTest.reachedStates, currentTest.stateCount);
  bool hasLock = containsState(LOCK, currentTest.reachedStates, currentTest.stateCount);
  
  Serial.println("\n--- Test Results ---");
  Serial.print("Reached UNLOCK: ");
  Serial.println(hasUnlock ? "YES" : "NO");
  Serial.print("Reached BUSY_MOVE: ");
  Serial.println(hasBusyMove ? "YES" : "NO");
  Serial.print("Reached LOCK: ");
  Serial.println(hasLock ? "YES" : "NO");
  Serial.print("Final State: ");
  Serial.println(stateToString(fsmState.currentState));
  
  currentTest.passed = currentTest.passed && hasUnlock && hasBusyMove && hasLock;
  
  if (currentTest.passed) {
    Serial.println("✓ TEST PASSED");
  } else {
    Serial.println("✗ TEST FAILED");
    Serial.print("Reason: ");
    Serial.println(currentTest.message);
  }
  
  return currentTest.passed;
}

/*
 * INTEGRATION TEST 3: Motor Behavior 3
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
  
  resetFSM();
  currentTest.passed = false;
  currentTest.message = "";
  
  // Start from UNLOCK state
  fsmState.currentState = UNLOCK;
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
  bool hasBusyWait = containsState(BUSY_WAIT, currentTest.reachedStates, currentTest.stateCount);
  bool finalStateValid = (fsmState.currentState == LOCK || 
                          fsmState.currentState == UNLOCK || 
                          fsmState.currentState == BUSY_WAIT);
  
  Serial.println("\n--- Test Results ---");
  Serial.print("Entered BUSY_WAIT: ");
  Serial.println(hasBusyWait ? "YES" : "NO");
  Serial.print("Final State: ");
  Serial.println(stateToString(fsmState.currentState));
  Serial.print("Final State Valid: ");
  Serial.println(finalStateValid ? "YES" : "NO");
  
  currentTest.passed = hasBusyWait && finalStateValid;
  
  if (currentTest.passed) {
    Serial.println("✓ TEST PASSED");
  } else {
    Serial.println("✗ TEST FAILED");
    Serial.println("Did you manually rotate the lock during movement?");
  }
  
  return currentTest.passed;
}

/*
 * INTEGRATION TEST 4: Arduino Behavior 1
 * Action: Arduino outputs correct LOCK/UNLOCK position
 * Test Output: Position of motor (LOCK/UNLOCK)
 */
bool testArduinoPositionOutput() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 4: Position Output");
  Serial.println("========================================");
  
  resetFSM();
  currentTest.passed = false;
  
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
  
  currentTest.passed = isLockPos && isUnlockPos;
  
  Serial.println("\n--- Test Results ---");
  if (currentTest.passed) {
    Serial.println("✓ TEST PASSED - Position detection working correctly");
  } else {
    Serial.println("✗ TEST FAILED - Position detection incorrect");
    Serial.print("LOCK detection: ");
    Serial.println(isLockPos ? "PASS" : "FAIL");
    Serial.print("UNLOCK detection: ");
    Serial.println(isUnlockPos ? "PASS" : "FAIL");
  }
  
  return currentTest.passed;
}

/*
 * INTEGRATION TEST 5: Arduino Behavior 2
 * Action: Arduino rotates motor upon receiving command
 * Mocked: Command (simulated)
 * Test Output: Command acknowledgment
 */
bool testArduinoCommandResponse() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 5: Command Response");
  Serial.println("========================================");
  
  resetFSM();
  currentTest.passed = false;
  
  // Start from UNLOCK
  fsmState.currentState = UNLOCK;
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
  
  currentTest.passed = commandAcknowledged && motorMoving;
  
  Serial.println("\n--- Test Results ---");
  if (currentTest.passed) {
    Serial.println("✓ TEST PASSED - Command received and motor responded");
  } else {
    Serial.println("✗ TEST FAILED");
    Serial.print("Command acknowledged: ");
    Serial.println(commandAcknowledged ? "YES" : "NO");
    Serial.print("Motor moving: ");
    Serial.println(motorMoving ? "YES" : "NO");
  }
  
  return currentTest.passed;
}

/*
 * INTEGRATION TEST 6: Arduino Behavior 3
 * Action: Arduino resets when watch-dog timer reaches 0
 * Mocked: millis() and startTime to simulate timeout
 * Test Output: BAD state reached
 */
bool testArduinoWatchdogTimeout() {
  Serial.println("\n========================================");
  Serial.println("INTEGRATION TEST 6: Watchdog Timeout");
  Serial.println("========================================");
  
  resetFSM();
  currentTest.passed = false;
  
  // Start from BUSY_MOVE state with old startTime
  fsmState.currentState = BUSY_MOVE;
  fsmState.startTime = millis() - TOL - 1000; // Set startTime to 6 seconds ago (exceeds 5s timeout)
  
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
  
  currentTest.passed = reachedBadState;
  
  Serial.println("\n--- Test Results ---");
  if (currentTest.passed) {
    Serial.println("✓ TEST PASSED - Timeout detected correctly");
  } else {
    Serial.println("✗ TEST FAILED - Timeout not detected");
  }
  
  return currentTest.passed;
}

/*
 * Run all integration tests
 */
bool runAllIntegrationTests() {
  #ifndef INTEGRATION_TESTING
  Serial.println("Integration testing not enabled. Define INTEGRATION_TESTING to run tests.");
  return false;
  #else
  
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
  
  // Motor Behavior Tests
  allPassed &= testMotorLockToUnlock();
  delay(2000);
  
  allPassed &= testMotorUnlockToLock();
  delay(2000);
  
  allPassed &= testMotorWithInterference();
  delay(2000);
  
  // Arduino Behavior Tests
  allPassed &= testArduinoPositionOutput();
  delay(1000);
  
  allPassed &= testArduinoCommandResponse();
  delay(1000);
  
  allPassed &= testArduinoWatchdogTimeout();
  
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
  #endif
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  // Setup pins
  pinMode(lockPin, INPUT);
  pinMode(unlockPin, INPUT);
  
  myservo.attach(servoPin);
  
  // Calibrate servo
  Serial.println("Calibrating servo...");
  calibrate(myservo, feedbackPin, 50, 140);
  Serial.print("minFeedback: ");
  Serial.println(minFeedback);
  Serial.print("maxFeedback: ");
  Serial.println(maxFeedback);
  
  delay(1000);
  
  #ifdef INTEGRATION_TESTING
  runAllIntegrationTests();
  #else
  Serial.println("Integration testing disabled.");
  Serial.println("Define INTEGRATION_TESTING to enable tests.");
  #endif
}

void loop() {
  // Empty - tests run once in setup()
  delay(1000);
}

