/*  Project PAPR July 2020
 *  
 *  Duncan Wilson https://github.com/djdunc/papr
 *  
 *  Hardware: Arduino MKR WIFI 1010 and a Closed Cube HDC1080 temp / hum I2C sensor
 *  
 *  HDC1080 connected via pin 11 and 12 (SCL / SDA)
 *  
 *  Sketch is used to create a central BLE that collects data from an Arduino Nano peripheral
 *  
 *  
 */
   
#include <ArduinoBLE.h>
#include <ClosedCube_HDC1080.h>

// set up HDC10180 temp hum sensor
ClosedCube_HDC1080 hdc1080;

 // BLE Environmental Sensing Service
BLEService HDC1080Service("181A");

float temperature;
float humidity;

void setup() {
  Serial.begin(9600);
  delay(5000);

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  // Default settings for HDC1080 sensors: - Heater off - 14 bit Temperature and Humidity Measurement Resolutions
  hdc1080.begin(0x40);

  Serial.print("Manufacturer ID=0x");
  Serial.println(hdc1080.readManufacturerId(), HEX); // 0x5449 ID of Texas Instruments
  Serial.print("Device ID=0x");
  Serial.println(hdc1080.readDeviceId(), HEX); // 0x1050 ID of the device 

  // start scanning for peripheral
  Serial.println("BLE Central - GW for BLE devices");
  BLE.scan();
}

void loop() {
  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();
  
  if (peripheral) {
    // discovered a peripheral, print out address, local name, and advertised service
    Serial.print("Found ");
    Serial.print(peripheral.address());
    Serial.print(" '");
    Serial.print(peripheral.localName());
    Serial.print("' ");
    Serial.print(peripheral.advertisedServiceUuid());
    Serial.println();

    if (peripheral.localName() == "HDC1080 Monitor") {
      // stop scanning
      BLE.stopScan();

      getTempHumData(peripheral);

      // peripheral disconnected, start scanning again
      BLE.scan();
    }
  }
}

void getTempHumData(BLEDevice peripheral) {
  // connect to the peripheral
  Serial.println("Connecting ...");
  if (peripheral.connect()) {
    Serial.println("Connected");
  } else {
    Serial.println("Failed to connect!");
    return;
  }

  // discover peripheral attributes
  Serial.println("Discovering service 0x181a ...");
  if (peripheral.discoverService("181a")) {
    Serial.println("Service discovered");
  } else {
    Serial.println("Attribute discovery failed.");
    peripheral.disconnect();

 //   while (1);
   return;
  }

  // retrieve the simple key characteristic
  BLECharacteristic tChar = peripheral.characteristic("2a6e");
  BLECharacteristic hChar = peripheral.characteristic("2a6f");

  // subscribe to the simple key characteristic
  Serial.println("Subscribing to simple key characteristic ...");
  if (!tChar) {
    

    
    Serial.println("no simple key characteristic found!");
    peripheral.disconnect();
    return;
  } else if (!tChar.canSubscribe()) {
    Serial.println("simple key characteristic is not subscribable!");
    peripheral.disconnect();
    return;
  } else if (!tChar.subscribe()) {
    Serial.println("subscription failed!");
    peripheral.disconnect();
    return;
  } else {
    Serial.println("Subscribed");
  }

  while (peripheral.connected()) {
    // while the peripheral is connected


    
    // check if the value of the simple key characteristic has been updated
    if (tChar.valueUpdated()) {


      float tempVal[4]; // hold the characteristic's bytes
      tChar.readValue(tempVal, 4);
      Serial.print(tempVal[0]);
      Serial.print(" C ");
   
      float humVal[4]; // hold the characteristic's bytes
      hChar.readValue(humVal, 4);
      Serial.print(humVal[0]);
      Serial.println(" % ");


      temperature = hdc1080.readTemperature();
      humidity = hdc1080.readHumidity();
      Serial.print("T=");
      Serial.print(temperature);
      Serial.print("C, RH=");
      Serial.print(humidity);
      Serial.println("%");
      delay(5000);

    }
  }

  Serial.println("disconnected!");
}
