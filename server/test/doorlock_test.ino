/*
 * THIS FILE WILL BE USED FOR UNIT TESTING THE DOORLOCK FSM
 * 
 * Unit tests for FSM state transitions in the doorlock system.
 * Tests are designed to verify all state transitions work correctly
 * without requiring actual hardware.
 */

// Enable testing mode
#define TESTING

// Include necessary definitions from doorlock.ino
// Note: In actual implementation, these should be in a shared header file

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

// Constants
const unsigned long TOL = 5000;  // 5 second timeout for moves
const int LOCK_ANGLE = 120;
const int UNLOCK_ANGLE = 50;
const int ANGLE_TOLERANCE = 3;

// Global FSM state for testing
FSMState testFsmState;

/*
 * A struct to keep all state inputs in one place
 */
typedef struct {
  int currentDeg;        // Current servo position in degrees
  Command cmd;           // Command (LOCK_CMD, UNLOCK_CMD, or NONE)
  unsigned long clock;   // Current time in milliseconds
} state_inputs;

/*
 * Helper function for printing states
 */
char* s2str(State s) {
  switch(s) {
    case CALIBRATE_LOCK:
      return "(1) CALIBRATE_LOCK";
    case CALIBRATE_UNLOCK:
      return "(2) CALIBRATE_UNLOCK";
    case UNLOCK:
      return "(3) UNLOCK";
    case LOCK:
      return "(4) LOCK";
    case BUSY_WAIT:
      return "(5) BUSY_WAIT";
    case BUSY_MOVE:
      return "(6) BUSY_MOVE";
    case BAD:
      return "(7) BAD";
    default:
      return "???";
  }
}

/*
 * Helper function for printing commands
 */
char* c2str(Command c) {
  switch(c) {
    case NONE:
      return "NONE";
    case LOCK_CMD:
      return "LOCK_CMD";
    case UNLOCK_CMD:
      return "UNLOCK_CMD";
    default:
      return "???";
  }
}

/*
 * Mock helper functions for testing
 * These replace hardware-dependent functions
 */

// Check if at unlock position (open-ended tolerance: only check upper bound)
bool isAtUnlock(int deg, int unlockDeg) {
  return deg <= (unlockDeg + ANGLE_TOLERANCE);
}

// Check if at lock position (open-ended tolerance: only check lower bound)
bool isAtLock(int deg, int lockDeg) {
  return deg >= (lockDeg - ANGLE_TOLERANCE);
}

/*
 * Test version of fsmTransition that returns the new state
 * This is a copy of the logic from doorlock.ino but returns FSMState
 * In production, fsmTransition should be refactored to return FSMState
 */
FSMState updateFSM(FSMState state, int deg, Command cmd, unsigned long clock) {
  State nextState = state.currentState;

  switch (state.currentState) {
    case CALIBRATE_LOCK:
      state.lockDeg = deg;
      nextState = CALIBRATE_UNLOCK;
      break;

    case CALIBRATE_UNLOCK:
      state.unlockDeg = deg;
      nextState = UNLOCK;
      break;

    case UNLOCK:
      if (isAtUnlock(deg, state.unlockDeg) && cmd == LOCK_CMD) {
        nextState = BUSY_MOVE;
        state.startTime = clock;
      } else if (isAtLock(deg, state.lockDeg)) {
        nextState = LOCK;
      } else if (!isAtLock(deg, state.lockDeg) && !isAtUnlock(deg, state.unlockDeg)) {
        nextState = BUSY_WAIT;
      }
      break;

    case LOCK:
      if (isAtLock(deg, state.lockDeg) && cmd == UNLOCK_CMD) {
        nextState = BUSY_MOVE;
        state.startTime = clock;
      } else if (isAtUnlock(deg, state.unlockDeg)) {
        nextState = UNLOCK;
      } else if (!isAtLock(deg, state.lockDeg) && !isAtUnlock(deg, state.unlockDeg)) {
        nextState = BUSY_WAIT;
      }
      break;

    case BUSY_WAIT:
      if (isAtUnlock(deg, state.unlockDeg)) {
        nextState = UNLOCK;
      } else if (isAtLock(deg, state.lockDeg)) {
        nextState = LOCK;
      }
      // Otherwise stay in BUSY_WAIT
      break;

    case BUSY_MOVE:
      if (clock - state.startTime > TOL) {
        nextState = BAD;
      } else if (isAtUnlock(deg, state.unlockDeg)) {
        nextState = UNLOCK;
      } else if (isAtLock(deg, state.lockDeg)) {
        nextState = LOCK;
      }
      break;

    case BAD:
      // Stay in BAD state - requires manual reset
      break;
  }

  state.currentState = nextState;
  return state;
}

/*
 * Reset function to initialize test state
 */
void reset() {
  testFsmState.currentState = UNLOCK;  // Start from UNLOCK for most tests
  testFsmState.lockDeg = LOCK_ANGLE;
  testFsmState.unlockDeg = UNLOCK_ANGLE;
  testFsmState.startTime = 0;
}

/*
 * Given a start state, inputs, tests that updateFSM returns the correct end state
 * Returns true if test passed, false otherwise
 */
bool testTransition(FSMState start,
                    FSMState end,
                    state_inputs inputs,
                    bool verbos) {
  FSMState res = updateFSM(start, inputs.currentDeg, inputs.cmd, inputs.clock);
  
  bool passedTest = (res.currentState == end.currentState &&
                     res.lockDeg == end.lockDeg &&
                     res.unlockDeg == end.unlockDeg &&
                     res.startTime == end.startTime);

  if (!verbos) {
    return passedTest;
  } else if (passedTest) {
    char sToPrint[200];
    sprintf(sToPrint, "Test from %s to %s PASSED", s2str(start.currentState), s2str(end.currentState));
    Serial.println(sToPrint);
    return true;
  } else {
    char sToPrint[200];
    sprintf(sToPrint, "Test from %s to %s FAILED", s2str(start.currentState), s2str(end.currentState));
    Serial.println(sToPrint);
    sprintf(sToPrint, "End state expected: %s | actual: %s", s2str(end.currentState), s2str(res.currentState));
    Serial.println(sToPrint);
    sprintf(sToPrint, "Inputs: currentDeg %d | cmd %s | clock %lu", inputs.currentDeg, c2str(inputs.cmd), inputs.clock);
    Serial.println(sToPrint);
    sprintf(sToPrint, "          %12s | %8s | %8s | %10s", "currentState", "lockDeg", "unlockDeg", "startTime");
    Serial.println(sToPrint);
    sprintf(sToPrint, "starting: %12s | %8d | %8d | %10lu", s2str(start.currentState), start.lockDeg, start.unlockDeg, start.startTime);
    Serial.println(sToPrint);
    sprintf(sToPrint, "expected: %12s | %8d | %8d | %10lu", s2str(end.currentState), end.lockDeg, end.unlockDeg, end.startTime);
    Serial.println(sToPrint);
    sprintf(sToPrint, "actual:   %12s | %8d | %8d | %10lu", s2str(res.currentState), res.lockDeg, res.unlockDeg, res.startTime);
    Serial.println(sToPrint);
    Serial.println("");
    return false;
  }
}

/*
 * TEST CASES - Generated for all required transitions
 */

// Test 1: UNLOCK to BUSY_WAIT (manual turn detected - intermediate position)
const FSMState test1_start = {UNLOCK, 120, 50, 0};
const FSMState test1_end = {BUSY_WAIT, 120, 50, 0};
const state_inputs test1_inputs = {75, NONE, 1000};  // 75 deg is between 50 and 120

// Test 2: UNLOCK to BUSY_MOVE (lock command received while at unlock position)
const FSMState test2_start = {UNLOCK, 120, 50, 0};
const FSMState test2_end = {BUSY_MOVE, 120, 50, 2000};
const state_inputs test2_inputs = {50, LOCK_CMD, 2000};  // At unlock (50), command to lock

// Test 3: UNLOCK to LOCK (detected at lock position)
const FSMState test3_start = {UNLOCK, 120, 50, 0};
const FSMState test3_end = {LOCK, 120, 50, 0};
const state_inputs test3_inputs = {120, NONE, 1000};  // At lock position (120)

// Test 4: UNLOCK to UNLOCK (self transition - stay at unlock)
const FSMState test4_start = {UNLOCK, 120, 50, 0};
const FSMState test4_end = {UNLOCK, 120, 50, 0};
const state_inputs test4_inputs = {48, NONE, 1000};  // At unlock, no command, no change

// Test 5: BUSY_WAIT to LOCK (reached lock position)
const FSMState test5_start = {BUSY_WAIT, 120, 50, 0};
const FSMState test5_end = {LOCK, 120, 50, 0};
const state_inputs test5_inputs = {120, NONE, 1000};  // Reached lock position

// Test 6: BUSY_WAIT to UNLOCK (reached unlock position)
const FSMState test6_start = {BUSY_WAIT, 120, 50, 0};
const FSMState test6_end = {UNLOCK, 120, 50, 0};
const state_inputs test6_inputs = {50, NONE, 1000};  // Reached unlock position

// Test 7: BUSY_WAIT to BUSY_WAIT (self transition - still in intermediate)
const FSMState test7_start = {BUSY_WAIT, 120, 50, 0};
const FSMState test7_end = {BUSY_WAIT, 120, 50, 0};
const state_inputs test7_inputs = {80, NONE, 1000};  // Still intermediate

// Test 8: BUSY_MOVE to LOCK (reached lock position during move)
const FSMState test8_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test8_end = {LOCK, 120, 50, 1000};
const state_inputs test8_inputs = {120, NONE, 2000};  // Reached lock, within timeout

// Test 9: BUSY_MOVE to UNLOCK (reached unlock position during move)
const FSMState test9_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test9_end = {UNLOCK, 120, 50, 1000};
const state_inputs test9_inputs = {50, NONE, 2000};  // Reached unlock, within timeout

// Test 10: BUSY_MOVE to BAD (timeout exceeded)
const FSMState test10_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test10_end = {BAD, 120, 50, 1000};
const state_inputs test10_inputs = {75, NONE, 7000};  // 6000ms elapsed > 5000ms timeout

// Test 11: BUSY_MOVE to BUSY_MOVE (self transition - still moving, within timeout)
const FSMState test11_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test11_end = {BUSY_MOVE, 120, 50, 1000};
const state_inputs test11_inputs = {75, NONE, 3000};  // Still moving, 2000ms < timeout

// Test 12: LOCK to BUSY_MOVE (unlock command received while at lock position)
const FSMState test12_start = {LOCK, 120, 50, 0};
const FSMState test12_end = {BUSY_MOVE, 120, 50, 2000};
const state_inputs test12_inputs = {120, UNLOCK_CMD, 2000};  // At lock (120), command to unlock

// Test 13: LOCK to UNLOCK (detected at unlock position)
const FSMState test13_start = {LOCK, 120, 50, 0};
const FSMState test13_end = {UNLOCK, 120, 50, 0};
const state_inputs test13_inputs = {50, NONE, 1000};  // At unlock position

// Test 14: LOCK to BUSY_WAIT (manual turn detected - intermediate position)
const FSMState test14_start = {LOCK, 120, 50, 0};
const FSMState test14_end = {BUSY_WAIT, 120, 50, 0};
const state_inputs test14_inputs = {85, NONE, 1000};  // Intermediate position

// Test 15: LOCK to LOCK (self transition - stay at lock)
const FSMState test15_start = {LOCK, 120, 50, 0};
const FSMState test15_end = {LOCK, 120, 50, 0};
const state_inputs test15_inputs = {122, NONE, 1000};  // At lock, no command, no change

// Test 16: BUSY_MOVE to BUSY_MOVE (at boundary - exactly at timeout threshold but not exceeded)
const FSMState test16_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test16_end = {BUSY_MOVE, 120, 50, 1000};
const state_inputs test16_inputs = {75, NONE, 5999};  // 4999ms elapsed, just under timeout

// Test 17: BUSY_MOVE to BAD (exactly at timeout)
const FSMState test17_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test17_end = {BAD, 120, 50, 1000};
const state_inputs test17_inputs = {75, NONE, 6001};  // 5001ms elapsed, just over timeout

// Test 18: UNLOCK to BUSY_WAIT (edge case - just outside unlock tolerance)
const FSMState test18_start = {UNLOCK, 120, 50, 0};
const FSMState test18_end = {BUSY_WAIT, 120, 50, 0};
const state_inputs test18_inputs = {54, NONE, 1000};  // 54 = 50 + 3 + 1, just outside tolerance

// Test 19: BUSY_MOVE to LOCK (edge case - just at lock tolerance boundary)
const FSMState test19_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test19_end = {LOCK, 120, 50, 1000};
const state_inputs test19_inputs = {117, NONE, 2000};  // 117 = 120 - 3, at tolerance boundary

// Test 20: BUSY_MOVE to UNLOCK (edge case - just at unlock tolerance boundary)
const FSMState test20_start = {BUSY_MOVE, 120, 50, 1000};
const FSMState test20_end = {UNLOCK, 120, 50, 1000};
const state_inputs test20_inputs = {53, NONE, 2000};  // 53 = 50 + 3, at tolerance boundary

// Array of all test cases
const FSMState testStatesIn[20] = {
  test1_start, test2_start, test3_start, test4_start, test5_start,
  test6_start, test7_start, test8_start, test9_start, test10_start,
  test11_start, test12_start, test13_start, test14_start, test15_start,
  test16_start, test17_start, test18_start, test19_start, test20_start
};

const FSMState testStatesOut[20] = {
  test1_end, test2_end, test3_end, test4_end, test5_end,
  test6_end, test7_end, test8_end, test9_end, test10_end,
  test11_end, test12_end, test13_end, test14_end, test15_end,
  test16_end, test17_end, test18_end, test19_end, test20_end
};

const state_inputs testInputs[20] = {
  test1_inputs, test2_inputs, test3_inputs, test4_inputs, test5_inputs,
  test6_inputs, test7_inputs, test8_inputs, test9_inputs, test10_inputs,
  test11_inputs, test12_inputs, test13_inputs, test14_inputs, test15_inputs,
  test16_inputs, test17_inputs, test18_inputs, test19_inputs, test20_inputs
};

const int numTests = 20;

/*
 * Runs through all the test cases defined above
 */
bool testAll() {
  #ifndef TESTING
  Serial.println("Testing not compiled. Need to #define TESTING!");
  return false;
  #else // TESTING defined!
  
  Serial.println("========================================");
  Serial.println("Starting Doorlock FSM Unit Tests");
  Serial.println("========================================");
  Serial.println();
  
  for (int i = 0; i < numTests; i++) {
    Serial.print("Running test ");
    Serial.print(i + 1);
    Serial.print(" of ");
    Serial.println(numTests);
    reset();
    
    if (!testTransition(testStatesIn[i], testStatesOut[i], testInputs[i], true)) {
      Serial.println("========================================");
      Serial.println("TEST SUITE FAILED");
      Serial.println("========================================");
      return false;
    }
    Serial.println();
  }
  
  Serial.println("========================================");
  Serial.println("All tests passed!");
  Serial.println("========================================");
  return true;
  #endif // #ifndef TESTING
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  #ifdef TESTING
  testAll();
  #else
  Serial.println("Define TESTING to run unit tests");
  #endif
}

void loop() {
  // Empty - tests run once in setup()
}