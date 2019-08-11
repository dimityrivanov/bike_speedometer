#include "SSD1306_minimal.h"
#include "EEPROM.h"
SSD1306_Mini oled; // Declare the OLED object

#define F_CPU 16000000UL
#define SECS_PER_MIN (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY (SECS_PER_HOUR * 24L)
#define ONE_KILOMETER 1

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)

#define CYCLE 10
#define MS_TO_S 0.001
#define KMH_TO_MS 0.277777778
#define RADIAN_S 0.10472
#define TYRE_DIAMETER_M 0.6604 //(in meters) 26inch standart tyre
volatile bool pinChanged = false;
byte revolutions;
long totalKM = 0;
unsigned int rpm;
unsigned long rpmTime;
unsigned long beginCycleTime = 0;
int speed = 0;

int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
int reading = LOW;
bool updateCycleTime = true;
unsigned long totalMilageUpdate = 0;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
byte debounceDelay = 10;    // the debounce time; increase if the output flickers

struct bike_data {
  char current_speed[10];
  char total_kilometers[15];
  char time[15];
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

  unsigned long timeValue = ((millis() - beginCycleTime) * MS_TO_S);

  int elapsedKilometers = kmhToMs * timeValue;

  int totalElapsedKilometers = (int)(elapsedKilometers * MS_TO_S);

  unsigned long timeElapsedTime = ((millis() - beginCycleTime) / 1000);
  int hours = numberOfHours(timeElapsedTime);
  int minutes = numberOfMinutes(timeElapsedTime);
  int seconds = numberOfSeconds(timeElapsedTime);

  sprintf(bike.time, "Time: %02d:%02d:%02d", hours, minutes , seconds);
  oled.cursorTo(0, 10);
  oled.printString(bike.time);

  sprintf(bike.total_kilometers, "Total: %07d", totalKM);
  oled.cursorTo(0, 20); // x:0, y:23
  oled.printString(bike.total_kilometers);

  //we have elapsed kilometer
  if (totalElapsedKilometers >= 0) {
    updateCycleTime = true;
    totalKM = totalKM + ONE_KILOMETER;
    EEPROMWritelong(0, totalKM);
  }
}

void setup() {
  cli();
  oled.init(0x3C); // Initializes the display to the specified address
  oled.clear(); // Clears the display
  GIMSK = 0b00100000;    // turns on pin change interrupts
  PCMSK = 0b00010000;    // turn on interrupts on pins PB0, PB1, PB4
  //EEPROMWritelong(0, 0);
  totalKM = EEPROMReadlong(0);
  updateSpeed();
  updateTotalKM();
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

void testSpeed() {
  if (millis() - test > 500) {
    revolutions = CYCLE;
    if (reading == LOW) {
      reading = HIGH;
    } else {
      reading = LOW;
    }
    test = millis();
  }
}

void loop() {
  handlePinChange();

  //testSpeed();

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  //if there is no cycling activity for more than 1.5S, assume that the bike is stopped
  if ((millis() - lastDebounceTime) > 1500) {
    rpm = 0;
    rpmTime = 0;
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
        rpm = 60000 / (millis() - rpmTime) * CYCLE;
        if (updateCycleTime) {
          beginCycleTime = millis();
          updateCycleTime = false;
        }
        rpmTime = millis();
        updateSpeed();
        revolutions = 0;
      }
    }
  }

  lastButtonState = reading;

  if (millis() - totalMilageUpdate > 999) {
    totalMilageUpdate = millis();
    updateTotalKM();
  }
}

ISR(PCINT0_vect) {
  pinChanged = true;
}
