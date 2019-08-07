#include "SSD1306_minimal.h"
#include "EEPROM.h"
#include <avr/interrupt.h>
SSD1306_Mini oled; // Declare the OLED object

#define CYCLE 5
#define MS_TO_S 0.001
#define KMH_TO_MS 0.277777778
#define RADIAN_S 0.10472
#define TYRE_DIAMETER_M 0.6604 //(in meters) 26inch standart tyre
volatile bool pinChanged = false;
byte revolutions;
long totalKM = 0;
unsigned int rpm;
unsigned long passedtime;
unsigned long totalCycleTime = 0;
int speed = 0;

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
int reading = LOW;
bool updateCycleTime = false;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
byte debounceDelay = 10;    // the debounce time; increase if the output flickers

struct bike_data {
  char current_speed[10];
  char total_kilometers[15];
} bike;

void updateSpeed() {
  speed = TYRE_DIAMETER_M * rpm * RADIAN_S;
  sprintf(bike.current_speed, "KM/H: %03d", speed);
  oled.cursorTo(0, 0); // x:0, y:0
  oled.printString(bike.current_speed);
}

void updateTotalKM() {
  //convert km/h to m/s
  int kmhToMs = (speed * KMH_TO_MS);
  unsigned long timeValue = ((millis() - totalCycleTime) * MS_TO_S);
  int elapsedKilometers = kmhToMs * timeValue;
  int totalElapsedKilometers = (int)(elapsedKilometers * MS_TO_S);

  sprintf(bike.total_kilometers, "Total: %07d", totalKM);
  oled.cursorTo(0, 22); // x:0, y:23
  oled.printString(bike.total_kilometers);

  //we have elapsed kilometer
  if (totalElapsedKilometers > 0) {
    updateCycleTime = true;
    totalKM = totalKM + totalElapsedKilometers;
    EEPROMWritelong(0, totalKM);
  }
}

void setup() {
  oled.init(0x3C); // Initializes the display to the specified address
  oled.clear(); // Clears the display
  GIMSK = 0b00100000;    // turns on pin change interrupts
  PCMSK = 0b00010000;    // turn on interrupts on pins PB0, PB1, PB4
  //EEPROMWritelong(0, 0);
  totalKM = EEPROMReadlong(0);
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

void handlePinChange() {
  if (pinChanged) {
    if ((PINB & 0b00010000) != 0b00000000) {
      reading = HIGH;
      revolutions++;
    } else {
      reading = LOW;
    }
    pinChanged = false;
  }
}

unsigned long test = 0;
unsigned long totalMilageUpdate = 0;

void loop() {
  //handlePinChange();

  if (millis() - test > 100) {
    revolutions = CYCLE;
    if (reading == LOW) {
      reading = HIGH;
    } else {
      reading = LOW;
    }
    test = millis();
  }

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 1500) {
    rpm = 0;
    passedtime = 0;
    updateCycleTime = true;
    revolutions = 0;
    updateSpeed();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
    }

    if (buttonState == HIGH) {
      if (revolutions >= CYCLE) {
        cli();
        rpm = 60000 / (millis() - passedtime) * CYCLE;
        if (updateCycleTime) {
          totalCycleTime = millis();
          updateCycleTime = false;
        }
        passedtime = millis();
        updateSpeed();
        revolutions = 0;
        sei();
      }
    }
  }

  lastButtonState = reading;

  if (millis() - totalMilageUpdate > 999) {
    totalMilageUpdate = millis();
    updateTotalKM();
  }
}

ISR(PCINT0_vect)
{
  pinChanged = true;
}
