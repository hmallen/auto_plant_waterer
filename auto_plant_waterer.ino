/*
   AUTOMATIC PLANT WATERER
   - Soil moisture (Digital INPUT)
   - Water valve [solenoid] (Digital OUTPUT)

   TO DO:
   - INTERRUPTS????
   - Determine timing/mechanism for watering shut-off
   - Determine moisture threshold to trigger watering
   - Determine length of watering
   - Add more detailed timing information
   - Add light sensor
   -- Determine light threshold for watering mode activation
   -- Add calibration feature for various device positions relative to sunlight
*/

#include <DHT.h>
#include <SdFat.h>

#define moistureInput1 A0
#define moistureInput2 A1
#define moistureInput3 A2
#define moistureInput4 A3
#define lightInput A4
#define flowInput 2
#define DHT11Pin 3
#define waterOutput 4
#define logJumper 7
#define waterJumper 8
#define chipSelect 10

//#define DEBUG

boolean wateringActive = false;
boolean dataLogging = false;

SdFat sd;
SdFile logFile;

DHT dht(DHT11Pin, DHT11);

//const float moistureThreshold = (**DETERMINE PROPER VALUE**);
const float moistureThreshold = 100.0;

//const int wateringDuration = (**DETERMINE PROPER VALUE**);
const unsigned long wateringDuration = 60000;  // Watering time in milliseconds

const unsigned long delayTime = 60000;  // Delay time between light and moisture checks in milliseconds

boolean soil_1_active = true;
boolean soil_2_active = true;
boolean soil_3_active = true;
boolean soil_4_active = true;

volatile uint16_t pulses = 0;
volatile uint8_t lastflowpinstate;
volatile uint32_t lastflowratetimer = 0;
volatile float flowrate;

// Interrupt is called once a millisecond, looks for any pulses from the sensor
SIGNAL(TIMER0_COMPA_vect) {
  uint8_t x = digitalRead(flowInput);

  if (x == lastflowpinstate) {
    lastflowratetimer++;
    return; // nothing changed!
  }

  if (x == HIGH) pulses++;  //low to high transition

  lastflowpinstate = x;
  flowrate = 1000.0;
  flowrate /= lastflowratetimer;  // in hertz
  lastflowratetimer = 0;
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
  }

  else TIMSK0 &= ~_BV(OCIE0A);  // do not call the interrupt function COMPA anymore
}

void setup() {
  pinMode(waterOutput, OUTPUT); digitalWrite(waterOutput, LOW);
  pinMode(logJumper, INPUT);
  pinMode(waterJumper, INPUT);
  pinMode(flowInput, INPUT);

  if (digitalRead(logJumper) == 1) dataLogging = true;
  if (digitalRead(waterJumper) == 1) wateringActive = true;

  Serial.begin(57600);

  useInterrupt(true);

#ifdef DEBUG
  Serial.println();
  Serial.print("Initiating SD card...");
#endif

  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) {
#ifdef DEBUG
    Serial.println("failed.");
#endif
    sd.initErrorHalt();
  }
#ifdef DEBUG
  Serial.println("complete.");
#endif

  if (dataLogging == true) writeSD("Light, Temperature (F), Humidity, Soil Moisture", true);

#ifdef DEBUG
  Serial.print("Detecting number of soil sensors...");
#endif
  sensorDetect();
#ifdef DEBUG
  Serial.println("complete.");

  Serial.print("Sensor 1: ");
  Serial.println(soil_1_active);
  Serial.print("Sensor 2: ");
  Serial.println(soil_2_active);
  Serial.print("Sensor 3: ");
  Serial.println(soil_3_active);
  Serial.print("Sensor 4: ");
  Serial.println(soil_4_active);

  Serial.print("Warming up DHT11 sensor...");
#endif
  for (int x = 0; x < 5; x++) {
    float tempF = getData("temperature", 0);
    float humidity = getData("humidity", 0);
    //Serial.print(tempF);
    //Serial.print(", ");
    //Serial.println(humidity);
    delay(250);
  }
#ifdef DEBUG
  Serial.println("complete.");

  Serial.println("Beginning environmental condition monitoring.");
  Serial.println();
#endif
}

void loop() {
  float lightVal = getData("light", 0);
  float tempF = getData("temperature", 0);
  float humidity = getData("humidity", 0);
  float moistureVal1 = 0.0;
  float moistureVal2 = 0.0;
  float moistureVal3 = 0.0;
  float moistureVal4 = 0.0;
  if (soil_1_active == true) moistureVal1 = getData("moisture", 1);
  if (soil_2_active == true) moistureVal2 = getData("moisture", 2);
  if (soil_3_active == true) moistureVal3 = getData("moisture", 3);
  if (soil_4_active == true) moistureVal4 = getData("moisture", 4);

  unsigned long dataTime = millis();
  String dataString = String(dataTime) + ", " + String(lightVal) + ", " + String(tempF) + ", " + String(humidity) + ", " + String(soil_1_active) + ", " + String(moistureVal1) + ", " + String(soil_2_active) + ", " + String(moistureVal2) + ", " + String(soil_3_active) + ", " + String(moistureVal3) + ", " + String(soil_4_active) + ", " + String(moistureVal4);

  if (dataLogging == true) writeSD(dataString, true);

  float moistureValAvg = moistureAvgCalc(moistureVal1, moistureVal2, moistureVal3, moistureVal4);

#ifdef DEBUG
  Serial.println(dataString);
  Serial.print("Sensor Average:   ");
  Serial.println(moistureValAvg);
  Serial.print("Sensor Threshold: ");
  Serial.println(moistureThreshold);
  Serial.println();
#endif

  if (moistureValAvg < moistureThreshold) {
    boolean falsePositive = false;

#ifdef DEBUG
    Serial.println();
    Serial.print("Watering triggered. Checking for false positive...");
#endif

    for (int x = 0; x < 5; x++) {
      if (soil_1_active == true) moistureVal1 = getData("moisture", 1);
      if (soil_2_active == true) moistureVal2 = getData("moisture", 2);
      if (soil_3_active == true) moistureVal3 = getData("moisture", 3);
      if (soil_4_active == true) moistureVal4 = getData("moisture", 4);

      moistureValAvg = moistureAvgCalc(moistureVal1, moistureVal2, moistureVal3, moistureVal4);

      if (moistureValAvg >= moistureThreshold) falsePositive = true;
      delay(1000);
    }

    if (falsePositive == false && wateringActive == true) {
#ifdef DEBUG
      Serial.println("passed. Beginning watering cycle.");
#endif
      String wateringInfo = wateringCycle(wateringDuration);
      String wateringString = String(dataTime) + ": " + "Watering cycle complete - " + wateringInfo;
      writeSD(wateringString, false);
#ifdef DEBUG
      Serial.print("water_log.txt --> ");
      Serial.println(wateringString);
      Serial.println();
#endif
    }
    else if (falsePositive == true && wateringActive == true) {
#ifdef DEBUG
      Serial.println("failed. Aborting watering cycle.");
#endif
      String wateringString = String(dataTime) + ": " + "Watering aborted due to false positive.";
      writeSD(wateringString, false);
#ifdef DEBUG
      Serial.print("water_log.txt --> ");
      Serial.println(wateringString);
      Serial.println();
#endif
    }
  }

  delay(delayTime);
}

float getData(String dataType, byte sensorNumber) {
  if (dataType == "light") {
    float lightVal = analogRead(lightInput);
    delay(100);
    return lightVal;
  }

  else if (dataType == "moisture") {
    float moistureVal = 0.0;

    if (sensorNumber == 1) moistureVal = analogRead(moistureInput1);
    else if (sensorNumber == 2) moistureVal = analogRead(moistureInput2);
    else if (sensorNumber == 3) moistureVal = analogRead(moistureInput3);
    else if (sensorNumber == 4) moistureVal = analogRead(moistureInput4);

    moistureVal = 1023.0 - moistureVal;

    delay(100);

    return moistureVal;
  }

  else if (dataType == "temperature") {
    float tempF = dht.readTemperature(true);
    delay(400);
    return tempF;
  }

  else if (dataType == "humidity") {
    float humidity = dht.readHumidity();
    delay(400);
    return humidity;
  }
}

void writeSD(String dataString, boolean dataLog) {
  if (dataLog == true) {
    if (!logFile.open("data_log.txt", O_RDWR | O_CREAT | O_AT_END)) sd.errorHalt("Failed to open data log file.");
    logFile.println(dataString);
    logFile.close();
  }

  else {
    if (!logFile.open("watering_log.txt", O_RDWR | O_CREAT | O_AT_END)) sd.errorHalt("Failed to open watering log file.");
    //Serial.print("Writing data to log file...");
    logFile.println(dataString);
    //Serial.println("complete.");
    logFile.close();
  }
}

String wateringCycle(unsigned long cycleTime) {
  lastflowpinstate = digitalRead(flowInput);
  float waterVolume = 0.0;
  float waterRate = 0.0;
  pulses = 0;
  for (unsigned long startTime = millis(); (millis() - startTime) < cycleTime; ) {
    ;
  }
  waterVolume = pulses;
  waterVolume /= 450.0;

  float timingRatio = 60000.0 / float(cycleTime);
  waterRate = waterVolume * timingRatio;

  String waterVolumeRate = String(waterVolume) + " L @ " + String(waterRate) + " L/min";

  return waterVolumeRate;
}

float moistureAvgCalc(float moistureVal1, float moistureVal2, float moistureVal3, float moistureVal4) {
  float sensorCount = 0.0;
  float moistureTotal = 0.0;

  if (soil_1_active == true) {
    moistureTotal += moistureVal1;
    sensorCount = sensorCount + 1.0;
  }
  if (soil_2_active == true) {
    moistureTotal += moistureVal2;
    sensorCount = sensorCount + 1.0;
  }
  if (soil_3_active == true) {
    moistureTotal += moistureVal3;
    sensorCount = sensorCount + 1.0;
  }
  if (soil_4_active == true) {
    moistureTotal += moistureVal4;
    sensorCount = sensorCount + 1.0;
  }

  float moistureAvg = moistureTotal / sensorCount;

  return moistureAvg;
}

void sensorDetect() {
  const int detectThreshold = 10;
  // Soil sensor 1
  int moistureVal1_initial = analogRead(moistureInput1);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal = analogRead(moistureInput1);
    delay(100);
  }
  int moistureVal1_final = analogRead(moistureInput1);
  if (abs(moistureVal1_initial - moistureVal1_final) > detectThreshold) soil_1_active = false;

  // Soil sensor 2
  int moistureVal2_initial = analogRead(moistureInput2);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal2 = analogRead(moistureInput2);
    delay(100);
  }
  int moistureVal2_final = analogRead(moistureInput2);
  if (abs(moistureVal2_initial - moistureVal2_final) > detectThreshold) soil_2_active = false;

  // Soil sensor 3
  int moistureVal3_initial = analogRead(moistureInput3);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal3 = analogRead(moistureInput3);
    delay(100);
  }
  int moistureVal3_final = analogRead(moistureInput3);
  if (abs(moistureVal3_initial - moistureVal3_final) > detectThreshold) soil_3_active = false;

  // Soil sensor 4
  int moistureVal4_initial = analogRead(moistureInput4);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal4 = analogRead(moistureInput4);
    delay(100);
  }
  int moistureVal4_final = analogRead(moistureInput4);
  if (abs(moistureVal4_initial - moistureVal4_final) > detectThreshold) soil_4_active = false;
}

