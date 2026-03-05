#include <Servo.h>

// ===== LED mapping (your wiring) =====
// Road 1: D2=RED, D3=YELLOW, D4=GREEN
// Road 2: D5=RED, D6=YELLOW, D7=GREEN
const int A_RED = 2,  A_YELLOW = 3,  A_GREEN = 4;
const int B_RED = 5,  B_YELLOW = 6,  B_GREEN = 7;

// ===== Buttons =====
const int BTN_A = 8;   // Road 1 demand
const int BTN_B = 9;   // Road 2 demand

// ===== Servo =====
Servo gate;
const int SERVO_PIN = 10;

// === FLIPPED ANGLES (fixed direction) ===
const int GATE_DOWN = 110;   // barrier down
const int GATE_UP   = 20;    // barrier up

// ===== Timings (ms) =====
const unsigned long GREEN_BASE   = 5000;
const unsigned long GREEN_BOOST  = 3000;
const unsigned long YELLOW_TIME  = 2000;
const unsigned long ALL_RED_TIME = 1000;

// ===== Logging counters =====
unsigned long cycleCount = 0;
unsigned long reqA = 0, reqB = 0;
unsigned long extA = 0, extB = 0;

// Demand memory
bool demandA = false;
bool demandB = false;

// ---- Button edge-detect + debounce state ----
bool lastA = false, lastB = false;                 // last stable pressed state
unsigned long lastChangeA = 0, lastChangeB = 0;    // last time reading changed
const unsigned long DEBOUNCE_MS = 30;

// INPUT_PULLUP: pressed = LOW
bool rawPressed(int pin) {
  return digitalRead(pin) == LOW;
}

// Updates stable pressed states; returns true only on "new press" event
bool pressedEvent(int pin, bool &lastStable, unsigned long &lastChange) {
  bool r = rawPressed(pin);

  // if reading differs from stable state, start debounce timer
  if (r != lastStable) {
    if (millis() - lastChange >= DEBOUNCE_MS) {
      // accept the change as stable
      lastStable = r;
      lastChange = millis();
      // if stable state became pressed -> event
      if (lastStable) return true;
    }
  } else {
    // reading matches stable; keep timer fresh
    lastChange = millis();
  }

  return false;
}

void latchDemand() {
  // count once per new press
  if (pressedEvent(BTN_A, lastA, lastChangeA)) { demandA = true; reqA++; }
  if (pressedEvent(BTN_B, lastB, lastChangeB)) { demandB = true; reqB++; }
}

void waitWithLatch(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    latchDemand();
    delay(5);
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Adaptive Intersection Log شروع");

  pinMode(A_RED, OUTPUT);
  pinMode(A_YELLOW, OUTPUT);
  pinMode(A_GREEN, OUTPUT);

  pinMode(B_RED, OUTPUT);
  pinMode(B_YELLOW, OUTPUT);
  pinMode(B_GREEN, OUTPUT);

  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  gate.attach(SERVO_PIN);

  // Initialize button stable states
  lastA = rawPressed(BTN_A);
  lastB = rawPressed(BTN_B);
  lastChangeA = millis();
  lastChangeB = millis();

  // Start safe: all red + gate down
  digitalWrite(A_RED, HIGH);
  digitalWrite(B_RED, HIGH);
  digitalWrite(A_YELLOW, LOW);
  digitalWrite(B_YELLOW, LOW);
  digitalWrite(A_GREEN, LOW);
  digitalWrite(B_GREEN, LOW);

  gate.write(GATE_DOWN);
  delay(800);
}

void loop() {
  cycleCount++;
  Serial.print("Cycle #");
  Serial.println(cycleCount);

  // ===== ROAD 1 GREEN =====
  unsigned long greenA = GREEN_BASE + (demandA ? GREEN_BOOST : 0);
  if (demandA) extA++;

  Serial.print("Road1 green(ms): ");
  Serial.println(greenA);

  digitalWrite(A_GREEN, HIGH);
  digitalWrite(A_YELLOW, LOW);
  digitalWrite(A_RED, LOW);

  digitalWrite(B_GREEN, LOW);
  digitalWrite(B_YELLOW, LOW);
  digitalWrite(B_RED, HIGH);

  gate.write(GATE_UP);
  waitWithLatch(greenA);

  // Road 1 Yellow
  digitalWrite(A_GREEN, LOW);
  digitalWrite(A_YELLOW, HIGH);
  gate.write(GATE_DOWN);
  waitWithLatch(YELLOW_TIME);

  // All red buffer
  digitalWrite(A_YELLOW, LOW);
  digitalWrite(A_RED, HIGH);
  waitWithLatch(ALL_RED_TIME);

  demandA = false; // served

  // ===== ROAD 2 GREEN =====
  unsigned long greenB = GREEN_BASE + (demandB ? GREEN_BOOST : 0);
  if (demandB) extB++;

  Serial.print("Road2 green(ms): ");
  Serial.println(greenB);

  digitalWrite(B_RED, LOW);
  digitalWrite(B_GREEN, HIGH);
  digitalWrite(B_YELLOW, LOW);

  gate.write(GATE_DOWN); // stays down for road 2
  waitWithLatch(greenB);

  // Road 2 Yellow
  digitalWrite(B_GREEN, LOW);
  digitalWrite(B_YELLOW, HIGH);
  waitWithLatch(YELLOW_TIME);

  // All red buffer
  digitalWrite(B_YELLOW, LOW);
  digitalWrite(B_RED, HIGH);
  waitWithLatch(ALL_RED_TIME);

  demandB = false; // served

  // ===== Summary =====
  Serial.print("Requests A/B: ");
  Serial.print(reqA);
  Serial.print(" / ");
  Serial.println(reqB);

  Serial.print("Extensions A/B: ");
  Serial.print(extA);
  Serial.print(" / ");
  Serial.println(extB);

  Serial.println("----");
}