#include "SSD1306_minimal.h"
#include "EEPROM.h"
#include <avr/interrupt.h>
SSD1306_Mini oled; // Declare the OLED object

#define CYCLE 20
#define TYRE_DIAMETER 0.6604 //(in meters)
volatile bool pinChanged = false;
byte revolutions;
unsigned int rpm;
unsigned long passedtime;

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
byte debounceDelay = 10;    // the debounce time; increase if the output flickers

struct bike_data {
  char current_speed[3];
  char total_kilometers[10];
} bike;

void updateSpeed(int speedd) {
  oled.cursorTo(0, 10); // x:0, y:23
  oled.printString("KMH/H:");
  sprintf(bike.current_speed, "%03d", speedd);
  oled.cursorTo(40, 10); // x:0, y:0
  oled.printString(bike.current_speed);
}

void updateTotalKM() {
  sprintf(bike.total_kilometers, "%07d", EEPROMReadlong(0));
  oled.cursorTo(0, 22); // x:0, y:23
  oled.printString("Total:");
  oled.cursorTo(45, 22); // x:0, y:23
  oled.printString(bike.total_kilometers);
}

void setup() {
  oled.init(0x3C); // Initializes the display to the specified address
  //oled.clear(); // Clears the display
  GIMSK = 0b00100000;    // turns on pin change interrupts
  PCMSK = 0b00010000;    // turn on interrupts on pins PB0, PB1, PB4
  sei();                 // enables interrupts
}

long EEPROMReadlong(long address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void EEPROMWritelong(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

int reading = LOW;

void loop() {
  if (pinChanged) {
    reading = digitalRead(4);
    if (reading == HIGH) {
      revolutions++;
    }
    pinChanged = false;
  }

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 1500) {
    updateSpeed(0);
    revolutions = 0;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
    }

    if (buttonState == HIGH) {
      if (revolutions == CYCLE) {
        cli();
        rpm = 60000 / (millis() - passedtime) * CYCLE;
        passedtime = millis();
        float speed = TYRE_DIAMETER * rpm * 0.10472;
        updateSpeed(speed);
        revolutions = 0;
        sei();
      }
    }
  }

  lastButtonState = reading;
  updateTotalKM();
}

ISR(PCINT0_vect)
{
  pinChanged = true;
}
