#include <Wire.h>

/* --------- CONFIGURE THESE ---------- */

// Selector pins that control your inverter/diode address trick
const uint8_t SEL_PIN_A = 6;  // set HIGH when using the first of the twins
const uint8_t SEL_PIN_B = 7;  // set HIGH when using the second of the twins

// Selector for the ICs. When selecting SEL_PIN_A, use SEL_A. When using SEL_NONE, sets both HIGH.
enum SelMode : uint8_t { SEL_NONE,
                         SEL_A,
                         SEL_B };


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
const uint8_t TRIP_CNT_ADDR = 0x39;
const uint8_t ODOMETER_ADDR = 0x3A;
const uint8_t SPEEDOMETER_LOWER_ADDR = 0x38;
const uint8_t SPEEDOMETER_UPPER_ADDR = 0x3B;


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

/* --------- SELECT LINE CONTROL ---------- */

void setSelector(SelMode m) {
  // Ensure only ONE is asserted at a time
  // NOTE: Have confirmed LOW : HIGH logic is ok!
  digitalWrite(SEL_PIN_A, (m == SEL_A) ? LOW : HIGH);
  digitalWrite(SEL_PIN_B, (m == SEL_B) ? LOW : HIGH);
  // Small settle time for the inverter/diode network to stabilize the ADR pin
  delayMicroseconds(100);
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
  /*
  Updates the battery/eco display based on the current bat_eco_digit struct values
  */

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
  
  setSelector(SEL_B);
  writeDigits(BAT_ECO_DISP_ADDR, numval, upper_val, lower_val, 1<<(int)bat_eco_digit.eco);
}

void set_odo_meter(uint32_t odoval) {
  /*
  :param odoval: value from 0 to 99999
  */

  // Set 
  // 4 digits, last digit is separate display
  // seperate digit is the LSB
  uint8_t tens = odoval % 10;
  uint8_t hundreds = (odoval / 10) % 10;
  uint8_t thousands = (odoval / 100) % 10;
  uint8_t ten_thousands = (odoval / 1000) % 10;
  uint8_t msb = (odoval / 10000) % 10;
  //MSB

  // LSB
  bat_eco_digit.digit = tens;
  


  setSelector(SEL_A);
  writeDigits(ODOMETER_ADDR, 
    number_to_saa1064_digit(msb), 
    number_to_saa1064_digit(ten_thousands), 
    number_to_saa1064_digit(thousands), 
    number_to_saa1064_digit(hundreds) // First digit left side 
  );
  update_eco_bat_disp();

}

void set_trip_counter(uint16_t tripval) {
  /*
  :param tripval: value from 0 to 9999
  */
  uint8_t d1 = number_to_saa1064_digit(tripval % 10); // Last digit
  uint8_t d2 = number_to_saa1064_digit((tripval / 10) % 10);
  uint8_t d3 = number_to_saa1064_digit((tripval / 100) % 10);
  uint8_t d4 = number_to_saa1064_digit((tripval / 1000) % 10);

  setSelector(SEL_NONE);
  writeDigits(TRIP_CNT_ADDR, d4, d3, d2, d1);
}

void set_speed(uint8_t speedval) {
  /*
  Sets the speedometer LED display to the given speed value.
  :param speedval: value from 0 to 100
  */

  // 0-100KM with 2km steps mapped to 51 LEDs
  // Use a 64-bit literal for the shifts
  uint64_t led_full_range_val = 1ULL << (speedval / 2);
  uint8_t val0_16 = (uint8_t)(led_full_range_val & 0xFF);
  uint8_t val16_32 = (uint8_t)((led_full_range_val >> 8) & 0xFF);
  uint8_t val32_48 = (uint8_t)((led_full_range_val >> 16) & 0xFF);
  uint8_t val48_64 = (uint8_t)((led_full_range_val >> 24) & 0xFF);

  // Upper IC 64..100 km/h
  uint8_t val64_80 = (uint8_t)((led_full_range_val >> 32) & 0xFF);
  uint8_t val80_96 = (uint8_t)((led_full_range_val >> 40) & 0xFF);
  uint8_t val96_100 = (uint8_t)((led_full_range_val >> 48) & 0x7);  // Only get the last 3 bits
  
  // Write to lower IC
  // d1 of IC = 0-16, d2=32-48, d3=16-32, d4=48-64
  setSelector(SEL_NONE);
  writeDigits(SPEEDOMETER_LOWER_ADDR, val0_16, val32_48, val16_32, val48_64);

  // Write to upper IC
  // d1=64-80, d2=96-100, d3=80-96, d4=unused
  // Selector already set to None
  writeDigits(SPEEDOMETER_UPPER_ADDR, val64_80, val96_100, val80_96, 0);  
}


void setup() {
  pinMode(SEL_PIN_A, OUTPUT);
  pinMode(SEL_PIN_B, OUTPUT);
  setSelector(SEL_NONE);

  Wire.begin();
  Serial.begin(9600);

  // Initialize all devices
  writeCtrl(BAT_ECO_DISP_ADDR, CTRL_DYN_BOTH_ENABLE);
  writeCtrl(TRIP_CNT_ADDR, CTRL_DYN_BOTH_ENABLE);
  writeCtrl(ODOMETER_ADDR, CTRL_DYN_BOTH_ENABLE);
  writeCtrl(SPEEDOMETER_LOWER_ADDR, CTRL_DYN_BOTH_ENABLE);
  writeCtrl(SPEEDOMETER_UPPER_ADDR, CTRL_DYN_BOTH_ENABLE);

  // Set initial values
  set_speed(50);
  set_odo_meter(12345);
  set_trip_counter(6789);
  bat_eco_digit.bat_indicator = 75;
  bat_eco_digit.eco = MODE_NORMAL;
  update_eco_bat_disp();
}

void loop() {


    //S - Speed
    //E - ECO
    //T - Trip
    //B - Battery
    //O - Odometer

    // Read serial and set battery level to value.
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      String cmd = input.substring(0,1);
      uint32_t val = input.substring(1).toInt();

      if(cmd == "S") {
        if (val >= 0 && val <= 100) {
          set_speed((uint8_t)val);
        }
      } else if (cmd == "E") {
        if (val >= 0 && val <= 2) {
          bat_eco_digit.eco = (EcoMode)val;
          update_eco_bat_disp();
        }
      } else if (cmd == "T") {
        if (val >= 0 && val <= 9999) {
          set_trip_counter((uint16_t)val);
        }
      } else if (cmd == "B") {
        if (val >= 0 && val <= 100) {
          bat_eco_digit.bat_indicator = (uint8_t)val;
          update_eco_bat_disp();
        }
      } else if (cmd == "O") {
        if (val >= 0 && val <= 99999) {
          set_odo_meter((uint32_t)val);
        }
      }
    }
}
