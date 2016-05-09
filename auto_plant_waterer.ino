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

#define chipSelect 10
#define moistureInput1 A0
#define moistureInput2 A1
#define moistureInput3 A2
#define moistureInput4 A3
#define lightInput A4
#define waterOutput 2
#define logJumper 3
#define dataJumper 4
#define DHT11Pin 5

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

void setup() {
  pinMode(waterOutput, OUTPUT); digitalWrite(waterOutput, LOW);
  pinMode(logJumper, INPUT);
  pinMode(dataJumper, INPUT);

  if (digitalRead(logJumper) == 1) dataLogging = true;
  if (digitalRead(dataJumper) == 1) wateringActive = true;

  Serial.begin(57600);

  Serial.println();
  Serial.print("Initiating SD card...");
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) {
    Serial.println("failed.");
    sd.initErrorHalt();
  }
  Serial.println("complete.");

  if (dataLogging == true) writeSD("Light, Temperature (F), Humidity, Soil Moisture");

  Serial.print("Detecting number of soil sensors...");
  sensorDetect();
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
  for (int x = 0; x < 5; x++) {
    float tempF = getData("temperature", 0);
    float humidity = getData("humidity", 0);
    //Serial.print(tempF);
    //Serial.print(", ");
    //Serial.println(humidity);
    delay(250);
  }
  Serial.println("complete.");

  Serial.println("Beginning environmental condition monitoring.");
  Serial.println();
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

  String dataString = String(lightVal) + ", " + String(tempF) + ", " + String(humidity) + ", " + String(moistureVal1) + ", " + String(moistureVal2) + ", " + String(moistureVal3) + ", " + String(moistureVal4);
  Serial.println(dataString);

  if (dataLogging == true) writeSD(dataString);

  float moistureValAvg = moistureAvgCalc(moistureVal1, moistureVal2, moistureVal3, moistureVal4);
  Serial.print("Sensor Average:   ");
  Serial.println(moistureValAvg);
  Serial.print("Sensor Threshold: ");
  Serial.println(moistureThreshold);

  if (moistureValAvg < moistureThreshold) {
    boolean falsePositive = false;

    Serial.println();
    Serial.print("Watering triggered. Checking for false positive...");

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
      Serial.println("passed. Beginning watering cycle.");
      wateringCycle(wateringDuration);
    }
    else if (falsePositive == true && wateringActive == true) Serial.println("failed. Aborting watering cycle.");
    Serial.println();
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

void writeSD(String dataString) {
  if (!logFile.open("data_log.txt", O_RDWR | O_CREAT | O_AT_END)) sd.errorHalt("Failed to open log file.");

  //Serial.print("Writing data to log file...");
  logFile.println(dataString);
  //Serial.println("complete.");
  logFile.close();
}

void wateringCycle(unsigned long cycleTime) {
  digitalWrite(waterOutput, HIGH);
  // INCLUDE PUMP ACTIVATION, IF NECESSARY, OR AS A SOLE ALTERNATIVE
  // MODIFY FOR DYNAMIC WATERING DURATION
  // EX. MONITOR MOISTURE CONTENT WHILE WATERING
  delay(cycleTime);
  digitalWrite(waterOutput, LOW);

  writeSD("Watering cycle complete.");
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
  // Soil sensor 1
  int moistureVal1_initial = analogRead(moistureInput1);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal = analogRead(moistureInput1);
    delay(100);
  }
  int moistureVal1_final = analogRead(moistureInput1);
  if (abs(moistureVal1_initial - moistureVal1_final) > 10) soil_1_active = false;

  // Soil sensor 2
  int moistureVal2_initial = analogRead(moistureInput2);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal2 = analogRead(moistureInput2);
    delay(100);
  }
  int moistureVal2_final = analogRead(moistureInput2);
  if (abs(moistureVal2_initial - moistureVal2_final) > 10) soil_2_active = false;

  // Soil sensor 3
  int moistureVal3_initial = analogRead(moistureInput3);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal3 = analogRead(moistureInput3);
    delay(100);
  }
  int moistureVal3_final = analogRead(moistureInput3);
  if (abs(moistureVal3_initial - moistureVal3_final) > 10) soil_3_active = false;

  // Soil sensor 4
  int moistureVal4_initial = analogRead(moistureInput4);
  delay(100);
  for (int x = 0; x < 3; x++) {
    int moistureVal4 = analogRead(moistureInput4);
    delay(100);
  }
  int moistureVal4_final = analogRead(moistureInput4);
  if (abs(moistureVal4_initial - moistureVal4_final) > 10) soil_4_active = false;
}

