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
   - Add logging capabilities
   - Add light sensor
   -- Determine light threshold for watering mode activation
   -- Add calibration feature for various device positions relative to sunlight
*/

#include <DHT.h>
#include <SdFat.h>

#define chipSelect 4
#define DHT11Pin 2
#define lightInput A0
#define moistureInput A1
#define waterOutput 3

const boolean wateringActive = false;

SdFat sd;
SdFile logFile;

DHT dht(DHT11Pin, DHT11);

//const float moistureThreshold = (**DETERMINE PROPER VALUE**);
const float moistureThreshold = 100.0;

//const int wateringDuration = (**DETERMINE PROPER VALUE**);
const int wateringDurationSec = 60;
const unsigned long wateringDuration = wateringDuration * 1000;

//const float lightThreshold = (**DETERMINE PROPER VALUE**);
const float lightThreshold = 300.0;

const unsigned long delayTime = 60000;  // Delay time between light and moisture checks in milliseconds

void setup() {
  pinMode(waterOutput, OUTPUT); digitalWrite(waterOutput, LOW);

  Serial.begin(57600);

  Serial.print("Initiating SD card...");
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) {
    Serial.println("failed.");
    sd.initErrorHalt();
  }
  Serial.println("complete.");

  writeSD("Light, Temperature (F), Humidity, Soil Moisture");
  
  Serial.println("Beginning environmental condition monitoring.");
}

void loop() {
  float lightVal = getData("light");
  float tempF = getData("temperature");
  float humidity = getData("humidity");
  float moistureVal = getData("moisture");

  String dataString = String(lightVal) + ", " + String(tempF) + ", " + String(humidity) + ", " + String(moistureVal);
  writeSD(dataString);

  if (moistureVal < moistureThreshold) {
    boolean falsePositive = false;
    for (int x = 0; x < 5; x++) {
      moistureVal = getData("moisture");
      if (moistureVal >= moistureThreshold) falsePositive = true;
      delay(1000);
    }
    if (falsePositive == false && wateringActive == true) wateringCycle(wateringDuration);
  }

  delay(delayTime);
}

float getData(String dataType) {
  if (dataType == "light") {
    int lightVal = analogRead(lightInput);
    delay(100);
    return lightVal;
  }
  else if (dataType == "moisture") {
    int moistureVal = analogRead(moistureInput);
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

  Serial.print("Writing data to log file...");
  logFile.println(dataString);
  Serial.println("complete.");
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
