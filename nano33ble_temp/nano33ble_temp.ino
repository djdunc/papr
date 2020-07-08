

#include <ArduinoBLE.h>
#include <Arduino_HTS221.h>
#include <ClosedCube_Si7051.h>

ClosedCube_Si7051 si7051;

 // BLE Battery Service
BLEService tempCService("181A");

// BLE Temp in C Characteristic - see https://www.bluetooth.com/specifications/gatt/characteristics/ fr UUID's
BLEFloatCharacteristic tempCChar("2A6E",  // standard 16-bit characteristic UUID https://www.bluetooth.com/xml-viewer/?src=https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.temperature.xml
    BLERead | BLENotify); // remote clients will be able to get notifications if this characteristic changes

float temperature;
float humidity;
long previousMillis = 0;  // last time the battery level was checked, in ms

//TMP36 Pin Variables
int sensorPin = 0; //the analog pin the TMP36's Vout (sense) pin is connected to
                        //the resolution is 10 mV / degree centigrade with a
                        //500 mV offset to allow for negative temperatures

void setup() {
  Serial.begin(9600);    // initialize serial communication
  Serial.println("Setting up...");
  
  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED pin to indicate when a central is connected

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");

    while (1);
  }

  if (!HTS.begin()) {
    Serial.println("Failed to initialize humidity temperature sensor!");
    while (1);
  }

  si7051.begin(0x40); // default I2C address is 0x40 and 14-bit measurement resolution

  Serial.print("firmware revision: ");
  switch (si7051.readFirmwareVersion())
  {
  case 0xFF:
    Serial.println("version 1.0");
    break;
  case 0x20:
    Serial.println("version 2.0");
    break;
  default:
    Serial.println("unknow");
      break;
  }

  //show initial readings
  updateReadings();  

  /* Set a local name for the BLE device
     This name will appear in advertising packets
     and can be used by remote devices to identify this BLE device
     The name can be changed but maybe be truncated based on space left in advertisement packet
  */
  BLE.setLocalName("TempCMonitor");
  BLE.setAdvertisedService(tempCService); // add the service UUID
  tempCService.addCharacteristic(tempCChar); // add the characteristic
  BLE.addService(tempCService); // Add the service
  tempCChar.writeValue(temperature); // set initial value for this characteristic

  /* Start advertising BLE.  It will start continuously transmitting BLE
     advertising packets and will be visible to remote BLE central devices
     until it receives a new connection */

  // start advertising
  BLE.advertise();

  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
  // wait for a BLE central
  BLEDevice central = BLE.central();

  // if a central is connected to the peripheral:
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's BT address:
    Serial.println(central.address());
    // turn on the LED to indicate the connection:
    digitalWrite(LED_BUILTIN, HIGH);

    // check the battery level every 200ms
    // while the central is connected:
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
  
  temperature = si7051.readTemperature();
  Serial.println("-------------------");
  Serial.print("si7051 T=");
  Serial.print(temperature);
  Serial.println("C");
  tempCChar.writeValue(temperature);

  // read all the sensor values
  temperature = HTS.readTemperature();
  humidity    = HTS.readHumidity();
  
  // print each of the sensor values
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity    = ");
  Serial.print(humidity);
  Serial.println(" %");

  // print an empty line
  Serial.println();
}
