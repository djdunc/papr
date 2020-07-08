

#include <ArduinoBLE.h>
#include <Arduino_HTS221.h>
#include <ClosedCube_HDC1080.h>

ClosedCube_HDC1080 hdc1080;

 // BLE Environmental Sensing Service
BLEService HDC1080Service("181A");

// BLE Temp in C Characteristic - see https://www.bluetooth.com/specifications/gatt/characteristics/ fr UUID's
// standard 16-bit characteristic UUID https://www.bluetooth.com/xml-viewer/?src=https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.temperature.xml
BLEFloatCharacteristic temperatureChar("2A6E", BLERead | BLENotify); // remote clients will be able to get notifications if this characteristic changes
BLEFloatCharacteristic humidityChar("2A6F", BLERead | BLENotify); // remote clients will be able to get notifications if this characteristic changes

float temperature;
float humidity;
long previousMillis = 0;  // last time the battery level was checked, in ms

void setup() {
  delay(5000); // seen some issues with serial not connecting immediately on nano 33 ble hence giving it time to connect 
  Serial.begin(9600);    // initialize serial communication
  Serial.println("Setting up...");
  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED pin to indicate when a central is connected

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");

    while (1);
  }

  // initialise on board sensors
  if (!HTS.begin()) {
    Serial.println("Failed to initialize humidity temperature sensor!");
    while (1);
  }

  // Default settings for HDC1080 sensors: - Heater off - 14 bit Temperature and Humidity Measurement Resolutions
  hdc1080.begin(0x40);

  Serial.print("Manufacturer ID=0x");
  Serial.println(hdc1080.readManufacturerId(), HEX); // 0x5449 ID of Texas Instruments
  Serial.print("Device ID=0x");
  Serial.println(hdc1080.readDeviceId(), HEX); // 0x1050 ID of the device 
  printSerialNumber();


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
    Serial.print("Connected to central: "); // print the central's BT address:
    Serial.println(central.address());      // turn on the LED to indicate the connection:
    digitalWrite(LED_BUILTIN, HIGH);

    // check the status while the central is connected:
    while (central.connected()) {
      long currentMillis = millis();
      // if 1000ms have passed, check the sensor:
      if (currentMillis - previousMillis >= 1000) {
        previousMillis = currentMillis;
        updateReadings();
      }
    }
    // when the central disconnects, turn off the LED:
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}

void updateReadings() {
  
  temperature = hdc1080.readTemperature();
  humidity = hdc1080.readHumidity();
  Serial.print("T=");
  Serial.print(temperature);
  Serial.print("C, RH=");
  Serial.print(humidity);
  Serial.println("%");
  temperatureChar.writeValue(temperature);
  humidityChar.writeValue(humidity);

  
    float tempVal[4]; // hold the characteristic's bytes
    temperatureChar.readValue(tempVal, 4);
    Serial.print(tempVal[0]);
    Serial.print(" C ");

    float humVal[4]; // hold the characteristic's bytes
    humidityChar.readValue(humVal, 4);
    Serial.print(humVal[0]);
    Serial.println(" % ");


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
}

void printSerialNumber() {
  Serial.print("Device Serial Number=");
  HDC1080_SerialNumber sernum = hdc1080.readSerialNumber();
  char format[12];
  sprintf(format, "%02X-%04X-%04X", sernum.serialFirst, sernum.serialMid, sernum.serialLast);
  Serial.println(format);
}
