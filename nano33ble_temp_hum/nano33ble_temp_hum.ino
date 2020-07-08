/*  Project PAPR July 2020
 *  
 *  Duncan Wilson https://github.com/djdunc/papr
 *  
 *  Code based on examples ArduinoBLE, Arduino_HTS221 and ClosedCube_HDC1080 libaries
 *  
 *  Hardware: Arduino Nano 33 Sense and a Closed Cube HDC1080 temp / hum I2C sensor
 *  
 *  HDC1080 connected via A4 and A5
 *  
 *  Sketch is used to create a peripheral that sends temp and humidity readings over BLE
 *  
 */
   

#include <ArduinoBLE.h>
#include <Arduino_HTS221.h>
#include <ClosedCube_HDC1080.h>

// temperature and humidity I2C sensor https://github.com/closedcube/ClosedCube_HDC1080_Arduino
ClosedCube_HDC1080 hdc1080;

 // BLE Environmental Sensing Service https://www.bluetooth.com/specifications/gatt/services/ 
BLEService HDC1080Service("181A");

// BLE Temp in C Characteristic - see https://www.bluetooth.com/specifications/gatt/characteristics/ for UUID's
// standard 16-bit characteristic UUID 
BLEFloatCharacteristic temperatureChar("2A6E", BLERead | BLENotify); // remote clients will be able to get notifications if this characteristic changes
BLEFloatCharacteristic humidityChar("2A6F", BLERead | BLENotify); // remote clients will be able to get notifications if this characteristic changes

float temperature;
float humidity;
long previousMillis = 0;  // last time the battery level was checked, in ms

void setup() {
  delay(5000);            // seen some issues with serial not connecting immediately on nano 33 ble hence giving it time to connect 
  Serial.begin(9600);     // initialize serial communication
  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED pin to indicate when a central is connected
  digitalWrite(LED_PWR, LOW); // turning off the power led to save energy
  
  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1){
      blinky(1);    
    }
  }

  // initialise on board sensors
  if (!HTS.begin()) {
    Serial.println("Failed to initialize humidity temperature sensor!");
    while (1){
      blinky(1);
    }
  }

  // Default settings for HDC1080 sensors: - Heater off - 14 bit Temperature and Humidity Measurement Resolutions
  hdc1080.begin(0x40);

/*
  Serial.print("Manufacturer ID=0x");
  Serial.println(hdc1080.readManufacturerId(), HEX); // 0x5449 ID of Texas Instruments
  Serial.print("Device ID=0x");
  Serial.println(hdc1080.readDeviceId(), HEX); // 0x1050 ID of the device 
  printSerialNumber();
*/

  /* Set a local name for the BLE device
     This name will appear in advertising packets and can be used by remote devices to identify this BLE device
     The name can be changed but maybe be truncated based on space left in advertisement packet
  */
  BLE.setLocalName("HDC1080 Monitor");
  BLE.setAdvertisedService(HDC1080Service); // add the service UUID
  HDC1080Service.addCharacteristic(temperatureChar); // add the characteristic
  HDC1080Service.addCharacteristic(humidityChar); // add the characteristic
  BLE.addService(HDC1080Service); // Add the service
  temperatureChar.writeValue(temperature); // set initial value for this characteristic
  humidityChar.writeValue(humidity);
  /* Start advertising BLE.  
  It will start continuously transmitting BLE advertising packets and will be visible to remote BLE central devices until it receives a new connection */

  // start advertising
  BLE.advertise();
  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
  // wait for a BLE central
  BLEDevice central = BLE.central();

  // if a central is connected to the peripheral:
  if (central) {
    blinky(5);
//    Serial.print("Connected to central: "); // print the central's BT address:
//    Serial.println(central.address());      // turn on the LED to indicate the connection:

    // check the status while the central is connected:
    while (central.connected()) {
      updateReadings();
      delay(1000);                   // take a reading every second while something is listening
    }
      // when the central disconnects
      blinky(3);
//    Serial.print("Disconnected from central: ");
//    Serial.println(central.address());
  }
  delay(1000);
}

void updateReadings() {
  
  // take readings from the HDC1080
  temperature = hdc1080.readTemperature();
  humidity = hdc1080.readHumidity();
  temperatureChar.writeValue(temperature);
  humidityChar.writeValue(humidity);
/*
  Serial.print("T=");
  Serial.print(temperature);
  Serial.print("C, RH=");
  Serial.print(humidity);
  Serial.println("%");
*/
  
/* some code to check the readings being sent over BLE

    float tempVal[4]; // hold the characteristic's bytes
    temperatureChar.readValue(tempVal, 4);
    Serial.print(tempVal[0]);
    Serial.print(" C ");

    float humVal[4]; // hold the characteristic's bytes
    humidityChar.readValue(humVal, 4);
    Serial.print(humVal[0]);
    Serial.println(" % ");
*/

/*  code below reads sensor values from onboard sensors - not being used on live system
    
  // read all the sensor values
  temperature = HTS.readTemperature();
  humidity    = HTS.readHumidity();
  
  // print each of the sensor values
  Serial.print("T=");
  Serial.print(temperature);
  Serial.print("C, RH=");
  Serial.print(humidity);
  Serial.println("%");

  // print an empty line
  Serial.println();
 */

}

/*
void printSerialNumber() {
  Serial.print("Device Serial Number=");
  HDC1080_SerialNumber sernum = hdc1080.readSerialNumber();
  char format[12];
  sprintf(format, "%02X-%04X-%04X", sernum.serialFirst, sernum.serialMid, sernum.serialLast);
  Serial.println(format);
}
*/

void blinky(int n){
  for(int i = 0; i < n; i++){
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);      
  }
}
