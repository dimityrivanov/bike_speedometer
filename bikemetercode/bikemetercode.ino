#include "SSD1306_minimal.h"
#include "EEPROM.h"
SSD1306_Mini oled; // Declare the OLED object

#define F_CPU 16000000UL
#define SECS_PER_MIN (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY (SECS_PER_HOUR * 24L)
#define ONE_KILOMETER 1
#define TOTAL_KM_EEPROM_ADDRESS 0

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define LCD_I2C_ADDRESS 0x3C
#define CYCLE 10
#define MS_TO_S 0.001
#define KMH_TO_MS 0.277777778
#define RADIAN_S 0.10472
#define TYRE_DIAMETER_M 0.6604 //(in meters) 26inch standart tyre

//time storing variables
unsigned long rpmTime;
unsigned long beginOneKmTime = 0;
unsigned long totalMilageUpdate = 0;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled


volatile bool pinChanged = false;
byte speed = 0;
byte revolutions;
long totalKM = 0;
unsigned int rpm;
int switchState;             // the current reading from the input pin
int lastSwitchState = LOW;   // the previous reading from the input pin
int reading = LOW;
bool updateOneKmTime = true;

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
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

  unsigned long timeValue = ((millis() - beginOneKmTime) * MS_TO_S);

  int elapsedKilometers = kmhToMs * timeValue;

  int totalElapsedKilometers = (int)(elapsedKilometers * MS_TO_S);

  unsigned long timeElapsedTime = millis() / 1000;
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
    updateOneKmTime = true;
    totalKM = totalKM + ONE_KILOMETER;
    EEPROMWriteTotalKM(totalKM);
  }
}

void setup() {
  cli();
  oled.init(LCD_I2C_ADDRESS); // Initializes the display to the specified address
  oled.clear(); // Clears the display
  GIMSK = 0b00100000;    // turns on pin change interrupts
  PCMSK = 0b00010000;    // turn on interrupts on pins PB0, PB1, PB4

  totalKM = EEPROMReadTotalKM();
  updateSpeed();
  updateTotalKM();
  sei();                 // enables interrupts
}

long EEPROMReadTotalKM() {
  long four = EEPROM.read(TOTAL_KM_EEPROM_ADDRESS);
  long three = EEPROM.read(TOTAL_KM_EEPROM_ADDRESS + 1);
  long two = EEPROM.read(TOTAL_KM_EEPROM_ADDRESS + 2);
  long one = EEPROM.read(TOTAL_KM_EEPROM_ADDRESS + 3);

  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void EEPROMWriteTotalKM(long value) {

  for (byte i = 0; i < 4; i++) {
    byte toStoreInEeprom = ((value >> (i * 8)) & 0xFF);
    EEPROM.write(TOTAL_KM_EEPROM_ADDRESS + i, toStoreInEeprom);
  }

  //    byte four = (value & 0xFF);
  //    byte three = ((value >> 8) & 0xFF);
  //    byte two = ((value >> 16) & 0xFF);
  //    byte one = ((value >> 24) & 0xFF);
  //
  //    EEPROM.write(TOTAL_KM_EEPROM_ADDRESS, four);
  //    EEPROM.write(TOTAL_KM_EEPROM_ADDRESS + 1, three);
  //    EEPROM.write(TOTAL_KM_EEPROM_ADDRESS + 2, two);
  //    EEPROM.write(TOTAL_KM_EEPROM_ADDRESS + 3, one);
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
  if (reading != lastSwitchState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  //if there is no cycling activity for more than 1.5S, assume that the bike is stopped
  if ((millis() - lastDebounceTime) > 1500) {
    rpm = 0;
    rpmTime = 0;
    updateOneKmTime = true;
    revolutions = 0;
    updateSpeed();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != switchState) {
      switchState = reading;
    }

    if (switchState == HIGH) {
      if (revolutions >= CYCLE) {
        rpm = 60000 / (millis() - rpmTime) * CYCLE;
        if (updateOneKmTime) {
          beginOneKmTime = millis();
          updateOneKmTime = false;
        }
        rpmTime = millis();
        updateSpeed();
        revolutions = 0;
      }
    }
  }

  lastSwitchState = reading;

  if (millis() - totalMilageUpdate > 999) {
    totalMilageUpdate = millis();
    updateTotalKM();
  }
}

ISR(PCINT0_vect) {
  pinChanged = true;
}
