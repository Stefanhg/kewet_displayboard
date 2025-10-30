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
enum SelMode : uint8_t { SEL_NONE,
                         SEL_A,
                         SEL_B };

struct Dev {
  uint8_t addr;  // 7-bit I2C address (0x38..0x3B)
  SelMode sel;   // which selector to assert
};

// EXAMPLE: adjust to your actual setup
Dev DEVICES[] = {
  //{0x38, SEL_NONE},
  //{0x39, SEL_NONE},
  //{0x3A, SEL_NONE},
  //{0x3B, SEL_NONE},
  // If two ICs share 0x3A, uncomment these and remove the plain 0x3A above:
  //{0x3A, SEL_A},   // first physical IC at 0x3A when pin 6 HIGH
  { 0x3A, SEL_B },  // second physical IC at 0x3A when pin 7 HIGH
};

enum EcoMode : uint8_t { MODE_ECO,
                         MODE_NORMAL,
                         MODE_BOOST};

struct BatEcoDigitStruct {
  uint8_t bat_indicator;
  EcoMode eco;   // which selector to assert
  uint8_t digit;
};

BatEcoDigitStruct bat_eco_digit = {25, MODE_NORMAL, 8}; 
const uint8_t BAT_ECO_DISP_ADDR = 0x3A;


const uint8_t NUM_DEV = sizeof(DEVICES) / sizeof(DEVICES[0]);

/* --------- SAA1064 HELPERS ---------- */

// Control byte presets (C7..C0)
const uint8_t CTRL_DYN_BLANK = 0b01110001;        // dynamic, both groups blanked
const uint8_t CTRL_DYN_BOTH_ENABLE = 0b01110111;  // dynamic, both groups enabled, seg-test OFF (data-driven)

void writeCtrl(uint8_t addr, uint8_t ctrl) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);  // control register
  Wire.write(ctrl);
  Wire.endTransmission();
}

void writeDigits(uint8_t addr, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
  Wire.beginTransmission(addr);
  Wire.write(0x01);  // start at digit 1 register
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
  uint8_t d[4] = { 0, 0, 0, 0 };
  // One pass lights each sink once
  for (int reg = 0; reg < 4; ++reg) {
    for (int bit = 0; bit < 8; ++bit) {
      // turn previous off
      d[0] = d[1] = d[2] = d[3] = 0;
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
  Serial.begin(9600);

  // Start with everything blanked
  for (int i = 0; i < NUM_DEV; ++i) {
    setSelector(DEVICES[i].sel);
    writeCtrl(DEVICES[i].addr, CTRL_DYN_BLANK);
    clearAll(DEVICES[i].addr);
  }
  setSelector(SEL_NONE);

  //  blankAllExcept(0);
   writeDigits(DEVICES[0].addr, 0xFF, 0xFF, 0xFF, 0xFF);
  // setSelector(SEL_B);
  // writeCtrl(DEVICES[0].addr, CTRL_DYN_BLANK);
  // writeDigits(DEVICES[0].addr, 12, 12, 12, 1<<5);
}

uint8_t number_to_saa1064_digit(uint8_t number) {
  /*
  Converts 0-9 to corrosponding digit directly written to the IC and returns the covnerted value
  e.g. To set 7, you write 7, to set 3 you write 79 
  */
  switch (number) {
    case 0:
      return 63;
    case 1:
      return 6;
    case 2:
      return 91;
    case 3:
      return 79;
    case 4:
      return 102;
    case 5:
      return 109;
    case 6:
      return 125;
    case 7:
      return 7;
    case 8:
      return 127;
    case 9:
      return 111;
    default:
      return 0; // Blank for out of range
  }
} 

void update_eco_bat_disp() {
  
  setSelector(SEL_B);
  writeCtrl(BAT_ECO_DISP_ADDR, CTRL_DYN_BOTH_ENABLE);
  clearAll(BAT_ECO_DISP_ADDR);


  // 16 levels of battery
  // at 100%, ALL on
  // AT 0%, first LED is on
  #define PERC_TO_LEVEL 6.25 // 100/16

  // DISP1 controls last digit
  // DISP2, DISP3 controls battery indicator
  // DISP2 controls UPPER part, DISP3 lower
  // DISP4 bit 0,1,2 controls ECO
  uint8_t numval = number_to_saa1064_digit(bat_eco_digit.digit);
  uint16_t setval = 1<<(int)(bat_eco_digit.bat_indicator / PERC_TO_LEVEL);


  // Split setval into upper and lower parts
  uint8_t upper_val = 0;
  uint8_t lower_val = 0;

  if (setval > 0xFF) {
    upper_val = (uint8_t)(setval >> 8);
    lower_val = 0xFF;
  } else {
    upper_val = 0;
    lower_val = (uint8_t)setval;
  }

  // Turn on all LEDs below the current level
  for (int i = 0; i < 8; ++i) {
    if (lower_val & (1 << i)) {
      lower_val |= (1 << i) - 1;
    }
  }
  for (int i = 0; i < 8; ++i) {
    if (upper_val & (1 << i)) {
      upper_val |= (1 << i) - 1;
    }
  }
  
  
  Serial.println(setval);
  Serial.println(bat_eco_digit.bat_indicator);
  writeDigits(BAT_ECO_DISP_ADDR, numval, upper_val, lower_val, 1<<(int)bat_eco_digit.eco);

  setSelector(SEL_NONE);

}

void loop() {


    // Read serial and set battery level to value.
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      int val = input.toInt();
      if (val >= 0 && val <= 100) {
        bat_eco_digit.bat_indicator = (uint8_t)val;
        update_eco_bat_disp();
      }
    }


  // for (int i = 0; i < NUM_DEV; ++i) {
  //   // Give this device sole ownership
  //   blankAllExcept(i);
  //   initActive(i);


  //   // DISP2, DISP3 controls battery indicator
  //   // DISP2 controls UPPER part, DISP3 lower
  //   // DISP4 bit 0,1,2 controls ECO
  //   for(int b=0; b<=100; b+=1) {
  //     bat_eco_digit.bat_indicator = b; // Set battery level
  //     //for(int e=MODE_ECO; e<=MODE_BOOST; e++) {
  //       //bat_eco_digit.eco = (EcoMode)e;
  //       //for(int d=0; d<=9; d++) {
  //         //bat_eco_digit.digit = d;
  //     update_eco_bat_disp();
  //     delay(200);
  //      // }
  //     //}
  //   }
  //   //bat_eco_digit.bat_indicator = 80; // Set battery level to 25%
  //   //update_eco_bat_disp();
  //   // DISP4 bit 0,1,2 controls ECO

  //   // IDENT banner: briefly turn all on so you can see which block is under test
  //   //writeDigits(DEVICES[i].addr, 0xFF, 0xFF, 0xFF, 0xFF);
  //   // delay(150);
  //   // clearAll(DEVICES[i].addr);
  //   // delay(80);

  //   // Run the 32-LED chase for this device
  //   // runChase(DEVICES[i].addr);

  //   // Release selector after test
  //   setSelector(SEL_NONE);

  //   // Small gap between devices
  //   // delay(200);
  // }

  // Optional pause before repeating the full suite
  //delay(50000);
}
