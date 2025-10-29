#include <Wire.h>

/* --------- CONFIGURE THESE ---------- */

// Selector pins that control your inverter/diode address trick
const uint8_t SEL_PIN_A = 6;  // set HIGH when using the first of the twins
const uint8_t SEL_PIN_B = 7;  // set HIGH when using the second of the twins

// How fast to step the LED chase (milliseconds)
const uint16_t STEP_MS = 30;

// List every "logical device" you want to test, in the order to test them.
// - For normal chips: use SEL_NONE
// - For twins (same I2C address): add two entries with SEL_A and SEL_B
enum SelMode : uint8_t { SEL_NONE, SEL_A, SEL_B };

struct Dev {
  uint8_t addr;   // 7-bit I2C address (0x38..0x3B)
  SelMode sel;    // which selector to assert
};

// EXAMPLE: adjust to your actual setup
Dev DEVICES[] = {
  {0x38, SEL_NONE},
  {0x39, SEL_NONE},
  //{0x3A, SEL_NONE},
  {0x3B, SEL_NONE},
  // If two ICs share 0x3A, uncomment these and remove the plain 0x3A above:
  {0x3A, SEL_A},   // first physical IC at 0x3A when pin 6 HIGH
  {0x3A, SEL_B},   // second physical IC at 0x3A when pin 7 HIGH
};

const uint8_t NUM_DEV = sizeof(DEVICES) / sizeof(DEVICES[0]);

/* --------- SAA1064 HELPERS ---------- */

// Control byte presets (C7..C0)
const uint8_t CTRL_DYN_BLANK       = 0b01110001; // dynamic, both groups blanked
const uint8_t CTRL_DYN_BOTH_ENABLE = 0b01110111; // dynamic, both groups enabled, seg-test OFF (data-driven)

void writeCtrl(uint8_t addr, uint8_t ctrl) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);       // control register
  Wire.write(ctrl);
  Wire.endTransmission();
}

void writeDigits(uint8_t addr, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
  Wire.beginTransmission(addr);
  Wire.write(0x01);       // start at digit 1 register
  Wire.write(d1);
  Wire.write(d2);
  Wire.write(d3);
  Wire.write(d4);
  Wire.endTransmission();
}

// Convenience: clear all 4 digit bytes
void clearAll(uint8_t addr) {
  writeDigits(addr, 0x00, 0x00, 0x00, 0x00);
}

/* --------- SELECT LINE CONTROL ---------- */

void setSelector(SelMode m) {
  // Ensure only ONE is asserted at a time
  digitalWrite(SEL_PIN_A, (m == SEL_A) ? LOW : HIGH);
  digitalWrite(SEL_PIN_B, (m == SEL_B) ? LOW : HIGH);
  // Small settle time for the inverter/diode network to stabilize the ADR pin
  delayMicroseconds(100);
}

/* --------- TEST LOGIC ---------- */

void blankAllExcept(int keepIdx) {
  for (int i = 0; i < NUM_DEV; ++i) {
    if (i == keepIdx) continue;
    setSelector(DEVICES[i].sel);
    writeCtrl(DEVICES[i].addr, CTRL_DYN_BLANK);
    clearAll(DEVICES[i].addr);
  }
  // After blanking others, release their selectors
  setSelector(SEL_NONE);
}

void initActive(int idx) {
  setSelector(DEVICES[idx].sel);
  writeCtrl(DEVICES[idx].addr, CTRL_DYN_BOTH_ENABLE);
  clearAll(DEVICES[idx].addr);
}

// Chase 32 outputs: bit 0..7 on D1, then D2, then D3, then D4
void runChase(uint8_t addr) {
  uint8_t d[4] = {0,0,0,0};
  // One pass lights each sink once
  for (int reg = 0; reg < 4; ++reg) {
    for (int bit = 0; bit < 8; ++bit) {
      // turn previous off
      d[0]=d[1]=d[2]=d[3]=0;
      // set current one
      d[reg] = (uint8_t)(1u << bit);
      writeDigits(addr, d[0], d[1], d[2], d[3]);
      delay(STEP_MS);
    }
  }
  // leave off
  clearAll(addr);
}

void setup() {
  pinMode(SEL_PIN_A, OUTPUT);
  pinMode(SEL_PIN_B, OUTPUT);
  setSelector(SEL_NONE);

  Wire.begin();

  // Start with everything blanked
  for (int i = 0; i < NUM_DEV; ++i) {
    setSelector(DEVICES[i].sel);
    writeCtrl(DEVICES[i].addr, CTRL_DYN_BLANK);
    clearAll(DEVICES[i].addr);
  }
  setSelector(SEL_NONE);
}

void loop() {
  for (int i = 0; i < NUM_DEV; ++i) {
    // Give this device sole ownership
    blankAllExcept(i);
    initActive(i);

    // IDENT banner: briefly turn all on so you can see which block is under test
    writeDigits(DEVICES[i].addr, 0xFF, 0xFF, 0xFF, 0xFF);
    delay(150);
    clearAll(DEVICES[i].addr);
    delay(80);

    // Run the 32-LED chase for this device
    runChase(DEVICES[i].addr);

    // Release selector after test
    setSelector(SEL_NONE);

    // Small gap between devices
    delay(200);
  }

  // Optional pause before repeating the full suite
  delay(500);
}
