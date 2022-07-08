// Copyright (C) 2021 Stephan Martin <https://www.designer2k2.at>
// This file is part of Mother Clock <https://github.com/designer2k2/Mutteruhr>.
//
// Mother Clock is free software and hardware: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Mother Clock is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Mother myRTC.  If not, see <http://www.gnu.org/licenses/>.

/*
   Mother Clock
   A pulse of alternating polarity every minute.

   Hardware:
     Custom PCB with mother clock circuit and 5V supply.
     Arduino Nano Every, no register emulation.
     DS3231 module with AT24C32 memory
     Optional Oled 128x64 SSD1306

   https://github.com/designer2k2/Mutteruhr

   https://www.designer2k2.at
   Stephan Martin 2021
*/


/*
   Pinout description:
   Clock drive pins: D9 D10
   LED drive pin: D21 (A7)
   Input Button A: D7
   Input Button A: D8
   Switch 1: D4
   Switch 2: D5
   Switch 3: D6
   Analog 24V Input: A0 with 47k/10k divider
   DS3231 32k: D3
   DW3231 SQW: D2
   I2C SDA: A4
   I2C SCL: A5
*/


// Debug serial port:
#define DEBUG 1

#if DEBUG == 1
#define debugp(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugfl() Serial.flush()
#else
#define debugp(x)
#define debugln(x)
#define debugfl()
#endif


// Additional librarys
#include <DS3231.h>     // https://github.com/NorthernWidget/DS3231 (find in Library manager)
#include <Wire.h>
#include <avr/sleep.h>
#include <AT24Cxx.h>    //  https://github.com/cvmanjoo/AT24Cxx

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     4
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define i2c_address 0x57
// Initilaize using AT24CXX(i2c_address, size of eeprom in KB).
AT24Cxx eep(i2c_address, 32);

DS3231 myRTC;

// Parameters:
const long pulsetime = 200;            // Pulse time in milliseconds (200-800?)
const float minimumsupplyvolt = 20.0; // Minimum voltage to activate pulse

// Global variables:
bool dip1stateonpoweron = false;  // State of the DIP1 state during power up
volatile bool btogglepin = false;  //Stores the last step polarity
volatile int isrcounter = 0;  // counts up in second interval

volatile bool buttonA = false;  // the state of button A

void setup() {
  // Start the I2C interface
  Wire.begin();

  // initialize serial communications at 57600bps:
  Serial.begin(57600);
  debugln(F("MotherClock init"));

  //Set pinmodes:

  //Clockdrive:
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);

  //Buttons / Switches / Clock Inputs:
  pinMode(2, INPUT_PULLUP);   // DS3231 SQW (1HZ)
  pinMode(3, INPUT_PULLUP);   // DW3231 32k
  pinMode(4, INPUT_PULLUP);   // DIP 1
  pinMode(5, INPUT_PULLUP);   // DIP 2
  pinMode(6, INPUT_PULLUP);   // DIP 3
  pinMode(7, INPUT_PULLUP);   // SWITCH A
  pinMode(8, INPUT_PULLUP);   // SWITCH B

  // LED output:
  pinMode(21, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Analog input reference to VDD 5V:
  analogReference(VDD);

  // Display:
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    debugln(F("SSD1306 allocation failed"));
  }
  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.println(F("Motherclock ON"));
  display.display();

  // DS3231 set that SQW outputs 1Hz only when power is on:
  myRTC.enableOscillator(true, false, 0);
  myRTC.enable32kHz(false);

  // Read in DIP 1 to decide the operation mode:
  dip1stateonpoweron = digitalRead(4);


  if (dip1stateonpoweron) {
    debugln("Running in interrupt mode");
    // Interrup from the DS3231 module 1Hz Signal:
    attachInterrupt(digitalPinToInterrupt(2), isr1hz, FALLING);

    // Interrupt on Button A to drive the clock manually:
    attachInterrupt(digitalPinToInterrupt(7), isrbuttonA, FALLING);
  } else {
    debugln("Running without interrupts");
  }

  // Preload the seconds from the DS3231:
  isrcounter = myRTC.getSecond();

  // Read the EEPROM to the Serial:
  debugp(F("warm "));
  debugln((int)eep.read(111));
  debugp(F("cold "));
  debugln((int)eep.read(110));
  byte buf[4];
  buf[1] = eep.read(101);
  buf[2] = eep.read(102);
  buf[3] = eep.read(103);
  buf[4] = eep.read(104);
  unsigned long var = (unsigned long)(buf[4] << 24) | (buf[3] << 16) | (buf[2] << 8) | buf[1];
  debugp(F("hours "));
  debugln(var);

  // Read the RTC Time to the Serial:
  bool century = false;
  bool h12Flag;
  bool pmFlag;
  debugp((int)myRTC.getYear());
  debugp('/');
  debugp((int)myRTC.getMonth(century));
  debugp('/');
  debugp((int)myRTC.getDate());
  debugp(' ');
  debugp((int)myRTC.getHour(h12Flag, pmFlag));
  debugp(':');
  debugp((int)myRTC.getMinute());
  debugp(':');
  debugln((int)myRTC.getSecond());
  debugfl();
}

void loop() {
  debugln(F("w"));
  debugfl();

  if (dip1stateonpoweron) {
    // Button press woke it up:
    if (buttonA) {
      pulse();
      delay(100); // delay to give the clock time to react
      //And only if the button is released reset this
      if (digitalRead(7) == HIGH) {
        buttonA = false;
        digitalWrite(LED_BUILTIN, LOW);
      }
    } else {
      // The 1Hz trigger woke it up:
      updateDisplay();
      // If the minute is full, give a pulse and reset:
      if (isrcounter >= 60) {
        isrcounter = 0;
        debugln(F("m"));
        pulse();
        minutecheck();
      }
    }

    // If button A is released go to sleep for 1 second, with the 1 second pulse from the DS3231 RTC:
    if (!buttonA) {
      enterSleep();
    }
  } else {
    // running in a sleepless mode
    if (digitalRead(7) == LOW) {
      // Button pressed, generate pulse:
      digitalWrite(LED_BUILTIN, HIGH);
      pulse();
      delay(100); // delay to give the clock time to react
      //And only if the button is released reset this
      if (digitalRead(7) == HIGH) {
        digitalWrite(LED_BUILTIN, LOW);
      }
    } else {
      // If the minute is full, give a pulse and reset:
      if (isrcounter >= 60) {
        debugln(F("m"));
        pulse();
        minutecheck();
      } else {
        delay(1000);  //wait 1sec (not accurate, but will be synced)
        isr1hz(); //increase second counter
        updateDisplay();
      }
      if (isrcounter % 5 == 0) {
        if (isrcounter >= 60) {
          debugln(F("m2"));
          pulse();
          minutecheck();
          isrcounter = myRTC.getSecond(); // sync counter from RTC
        }
      }
    }

  }

}

// Display update:
void updateDisplay()
{
  // Display the counter and current temperature:
  bool century = false;
  bool h12Flag;
  bool pmFlag;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(isrcounter);
  display.print(myRTC.getYear(), DEC);
  display.print(".");
  display.print(myRTC.getMonth(century), DEC);
  display.print(".");
  display.print(myRTC.getDate(), DEC);
  display.print(" (");
  display.print(myRTC.getDoW(), DEC);
  display.print(") - ");
  display.print(myRTC.getHour(h12Flag, pmFlag), DEC);
  display.print(":");
  display.print(myRTC.getMinute(), DEC);
  display.print(":");
  display.println(myRTC.getSecond(), DEC);
  display.print("Temp: ");
  display.print(myRTC.getTemperature(), 2);
  display.print(" C");
  display.display();
}

// Sleep mode handler:
void enterSleep(void)
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
  // The program will continue from here.
  // First thing to do is disable sleep.
  sleep_disable();
}

// ISR, Button A push:
void isrbuttonA() {
  digitalWrite(LED_BUILTIN, HIGH);
  buttonA = true;
}

// ISR, this gets triggered every second by external DS3231 module:
void isr1hz() {
  isrcounter = isrcounter + 1;
}

// Check the supply voltage with the voltage divider:
bool powerok() {
  int analogin;
  float analogvolt;
  analogin = analogRead(0);
  analogvolt = (5.0 / 1024.0) * analogin;          // Vref is VDD 5.0V
  analogvolt = analogvolt * (47.0 + 10.0) / 10.0;  // Voltage divider value
  debugp(F("v: "));
  debugln(analogvolt);
  debugfl();
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("V: "));
  display.println(analogvolt);
  display.display();
  if (analogvolt >= minimumsupplyvolt) {
    return true;
  } else {
    return false;
  }
}

// This gives the pulse to the clock:
void pulse() {
  // First, ensure both are off:
  digitalWrite(9, false);
  digitalWrite(10, false);

  // Run only if the supply voltage is ok:
  if (!powerok()) {
    return;
  }

  // Based on old pulse decide what pin to use now:
  if (btogglepin) {
    digitalWrite(9, true);
  } else {
    digitalWrite(10, true);
  }

  // Wait the pulse time:
  delay(pulsetime);

  // all off again:
  digitalWrite(9, false);
  digitalWrite(10, false);

  // Toogle the polarity for the next run:
  btogglepin = !btogglepin;
}

// Some things to do every minute:
void minutecheck() {
  // Min Temp save to eeprom
  uint8_t actual_temp = (int) myRTC.getTemperature();
  if (eep.read(110) > actual_temp) {
    eep.write(110, actual_temp);
    debugp(F("cold "));
    debugln(actual_temp);
    debugfl();
  }
  // Max Temp:
  if (eep.read(111) < actual_temp) {
    eep.write(111, actual_temp);
    debugp(F("warm "));
    debugln(actual_temp);
    debugfl();
  }

  // hour counter increase if minute is 0
  if (myRTC.getMinute() == 0) {
    byte buf[4];
    buf[1] = eep.read(101);
    buf[2] = eep.read(102);
    buf[3] = eep.read(103);
    buf[4] = eep.read(104);
    unsigned long var = (unsigned long)(buf[4] << 24) | (buf[3] << 16) | (buf[2] << 8) | buf[1];
    var = var + 1;
    eep.update(101, (byte) var);
    eep.update(102, (byte) var >> 8);
    eep.update(103, (byte) var >> 16);
    eep.update(104, (byte) var >> 24);
    debugp(F("hours "));
    debugln(var);
    debugfl();
  }
}
