#include <Wire.h>
//https://reference.arduino.cc/reference/en/libraries/ms5xxx/
#include <MS5xxx.h>

#define BATT_SENSE_PIN PA2
#define STATUS_LED_PIN PA0
#define FIRE_1_PIN PB7 //name based on SILKSCREEN
#define FIRE_2_PIN PB6 //name based on SILKSCREEN
#define CONTINUITY_1_PIN PB9 //name based on SILKSCREEN
#define CONTINUITY_2_PIN PB8 //name based on SILKSCREEN
#define SCL_PIN PB10
#define SDA_PIN PB11

//TODO: Add internal filtering (median?) to getTemp and getPressure functions.

// Battery sense voltage divider ratio (default 3.00x)
const float BATT_SENSE_MULTIPLIER = 3.012; //199.4k & 99.1k
// Internal Vref voltage (default 1200mV)
const long INT_REF = 1196;


// Variables; flash status code
const unsigned int STATUS_FLASH_COOLDOWN = 1000;  // Pause duration between status flash sequences
unsigned int flashTime = 190;                     // Time (ms) to hold flashes
unsigned long lastFlashChange = 0;                // Last millis() timestamp that a flash change occured
byte statusFlashRemain = 0;                       // Number of status flashes remaining in current cycle
byte statusCode = 0;                              // Status LED will flash this many times

// Variables; pressure sensor
double cachedTemp = 0; // Last-read temperature (0.01C)
double cachedPres = 0; // Last-read pressure (Pa)
const int TEMP_OFFSET = 0; // Offset for temperature (1 = 0.01C)
const int PRES_OFFSET = 0; // Offset for pressure (1 = 1Pa)

// Global objects
MS5xxx ms5607Sensor(&Wire);



/*
 * Fires the e-match on channel 1
 */
void fireMatch1() {
  digitalWrite(FIRE_1_PIN, HIGH);
}//fireMatch1()

/*
 * Fires the e-match on channel 2
 */
void fireMatch2() {
  digitalWrite(FIRE_2_PIN, HIGH);
}//fireMatch2()

/*
 * Checks continuity on channel 1
 * (true means continuity)
 * 
 * Reading will always be False
 * if fireMatch1() has been run
 */
bool getContinuityMatch1() {
  return !digitalRead(CONTINUITY_1_PIN);
}//getContinuityMatch1()

/*
 * Checks continuity on channel 2
 * (true means continuity)
 * 
 * Reading will always be False
 * if fireMatch2() has been run
 */
bool getContinuityMatch2() {
  return !digitalRead(CONTINUITY_2_PIN);
}//getContinuityMatch2()

/*
 * Sets the status LED to flash the
 * given number of times.
 */
void setStatusFlash(byte code) {
  statusCode = code;
  lastFlashChange = millis();
  digitalWrite(STATUS_LED_PIN, LOW);
}//setStatusFlash()

/*
 * Handles the status LED flash sequence.
 * Non-blocking; should be run in loop().
 */
void handleStatusFlash() {
  if (statusCode != 0) {
    if (statusFlashRemain == 0) {
      if (millis() - lastFlashChange > STATUS_FLASH_COOLDOWN) {
        // Reset flash sequence
        statusFlashRemain = statusCode;
      }//if
    } else {
      // Play flash sequence
      if (millis() - lastFlashChange > flashTime) {
        lastFlashChange = millis();
        // Change flash state
        if (!digitalRead(STATUS_LED_PIN)) {
          digitalWrite(STATUS_LED_PIN, HIGH);
        } else {
          digitalWrite(STATUS_LED_PIN, LOW);
          statusFlashRemain--; // one flash complete
        }//if (status not on)
      }//if (flash time)
    }//if (flashses remaining)
  }//if (code>0
}//handleStatusFlash()


/*
 * Polls new data from the pressure sensor.
 * Caches values for later use.
 * 
 * Uses moderate resources (~30ms & 48MHz).
 */
void pollPressureSensor() {
  ms5607Sensor.ReadProm();
  ms5607Sensor.Readout();
  cachedTemp = ms5607Sensor.GetTemp();
  cachedPres = ms5607Sensor.GetPres();
}//pollPressureSensor

/*
 * Returns current temperature in
 * celcius.
 * 
*  No filtering is applied (yet)
 */
double getTemp() {
  return (cachedTemp+TEMP_OFFSET)/100.0d;
}//getTemp()

/* 
 * Returns current pressure in
 * pascals.
 *  
 * No filtering is applied (yet)
 */
double getPressure() {
  return cachedPres+PRES_OFFSET;
}//getPressure()






/*
 * NOTES:
 * You must increase ADC sampling time in built_opt.h to
 * give it time to charge when reading battery (100k+200k
 * resistor divider limits charge speed).
 * See https://github.com/stm32duino/Arduino_Core_STM32/issues/651
 * for options.
 * 
 * This file is going to get messy for a while until I clean
 * it up some more
 */



void setup() {
  // Start serial (Baud doesn't matter for STM32 USB)
  Serial.begin(9600);
  // Configure pinmodes
  pinMode(BATT_SENSE_PIN, INPUT_ANALOG);
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(FIRE_1_PIN, OUTPUT);
  pinMode(FIRE_2_PIN, OUTPUT);
  pinMode(CONTINUITY_1_PIN, INPUT);
  pinMode(CONTINUITY_2_PIN, INPUT);
  // Force ADC to max resolution
  analogReadResolution(12);
  // Configure I2C
  Wire.setSCL(SCL_PIN);
  Wire.setSDA(SDA_PIN);
  // Connect to MS5607 Pressure Sensor
  while (ms5607Sensor.connect() > 0) {
    Serial.println(F("Connecting to MS5607..."));
    delay(500);
  }//while
}//setup()


void loop() {
  // put your main code here, to run repeatedly:

  unsigned long strt = millis();

  handleStatusFlash();
  pollPressureSensor();

  if (Serial.available()) {
    byte in = Serial.read();
    while (Serial.available()) {Serial.read();}
    byte lvl = in - '0';
    setStatusFlash(lvl);
    if (lvl == 1) {fireMatch1();}
    if (lvl == 2) {fireMatch2();}
  }

  Serial.print(getTemp());
  Serial.print(F(", "));
  Serial.println(getPressure());

  //Serial.print("VRef=");
  //Serial.print(measureVref(500));
  //Serial.print("mV  VBatt=");
  //Serial.print(readBatteryVoltage());
  //Serial.println("mV");
  //delay(1000);

  Serial.println(millis()-strt);

}//loop()


/*
 * Reads the battery voltage and
 * returns it in millivolts.
 * 
 * TODO: Add "num samples" argument
 */
int readBatteryVoltage() {
  // Get Vref
  int vRef = measureVref(200);
  // Read ADC value
  int aVal = analogRead(BATT_SENSE_PIN);
  // Calculate voltage
  return int( vRef*aVal*BATT_SENSE_MULTIPLIER/4096 );
}//readBatteryVoltage()

/*
 * Reads internal voltage reference and back-calculates
 * the system Vdd (VDDA) using a calibrated Vref consant.
 * Returns the average across several samples (MAX ~32k
 * samples supported). Probably works with many STM32F1xx
 * chips.
 * 
 * Inspired by the STM32duino Internal_channels example
 */
int measureVref(int samples) {
  // Take samples
  int result;
  long accum = 0;
  for (int i = 0; i < samples; i++) {
    result = analogRead(AVREF);
    accum += ((INT_REF * 4096L) / result); // Back-calculate VDDA in mV
  }//for
  return accum/samples; //Average
}//measureVref(int)
