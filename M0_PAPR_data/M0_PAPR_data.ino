/*  Project PAPR July 2020

    Duncan Wilson https://github.com/djdunc/papr

    Hardware: Adafruit Feather M0, 3x Adafruit MPRLS, 5 Closed Cube HDC1080 temp / hum I2C sensor and TCA9548A multiplexer

    MPRLS and HDC1080 connected via SCL / SDA

    Sketch to post data to RPi server and collect metrics from battery / fans

    Sketch builds on libraries including Adafruit_MQTT, Wifi101, ClosedCube_HDC1080, Adafruit_MPRLS

*/

#include <SPI.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <WiFi101.h>
#include <ClosedCube_HDC1080.h>
#include <Adafruit_MPRLS.h>
#include <Wire.h>


/************************* WiFI Setup *****************************/
#include "arduino_secrets.h"
/*  access point credentials stored in seperate file "arduino_secrets.h" with the pattern:
    #define SECRET_SSID "my SSID name"
    #define SECRET_PASS "my SSID password"
*/
#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2     // or, tie EN to VCC

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

/************************* MQTT Setup *****************************/
#define MQTT_SERVER      "192.168.10.1"
#define MQTT_SERVERPORT  1883
#define MQTT_USERNAME    ""
#define MQTT_KEY         ""

/************ Global State (you don't need to change this!) ******************/

//Set up the wifi client
WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

/****************************** Setup feeds for MQTT ***************************************/
#include "mqtt_feeds.h"

/************************* HDC10180 temp hum Setup *****************************/
ClosedCube_HDC1080 hdc1080;

/************************* MPRLS pressure Setup *****************************/
// You dont *need* a reset and EOC pin for most uses, so we set to -1 and don't connect
#define RESET_PIN  -1  // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1  // set to any GPIO pin to read end-of-conversion by pin
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

/************************* Configure address of multiplexer *****************************/
#define TCAADDR 0x70

/************************* Function to connect to Multiplexer *****************************/
void tcaselect(uint8_t i) {
  if (i > 7) return;          // only eight I2C devices on multiplexer

  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

/************************* define sensor addresses *****************************/
int hdcaddress[] = {0, 4, 5, 6, 7};   //i2c addresses on multiplexer
int mpladdress[] = {1, 2, 3};         //i2c addresses on multiplexer
int pinBattery = A0;                  // analogue input pin for battery level

/************************* define variables holding sensor values *****************************/
float   temperature = 0;
float   humidity = 0;
float   pressure_hPa = 0;
float   pressure_PSI = 0;
double  batteryLevel = 0;  
double  batteryResolution = 0.00322265625; // used to convert voltage on pin A0 3.3V / 1024

int     fanBaselineSpeed = 200;   // this is the PWM value to control the fan at "ideal" speed
int     fanSpeed = 0;             // this is the PWM value to control the fan = 0 to 255
float   p1H = 0;
float   p2L = 0;
float   p3 = 0;
float   p1H_start = 0;
float   p2L_start = 0;
float   p3_start = 0;
float   p1H_target = 10;
float   p2L_target = -10; 
//      p3_target doesnt exist since it is ambient reading
float   p1H_diff = 0;
float   p2L_diff = 0; 
int     pwmFanContol = 9;
int     timeBetweenReadings = 1000;



void setup() {
  Serial.begin(9600);
  delay(2000);

  Serial.println("\n\n\=====================================================");
  Serial.println("PAPR Arduino Sketch for fan control and sensor data gathering. July 2020.");
  Serial.println("");

  // Define wifi pins for the WINC1500 - needed for SAMD boards
  WiFi.setPins(WINC_CS, WINC_IRQ, WINC_RST, WINC_EN);

  // Declare the built in LED and turn it off
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);  //  Turn the RED LED near the USB off

  // Declare PWM controller for fan
  pinMode(pwmFanContol, OUTPUT);


  /************************* loop to startup HDC10180 sensor *****************************/
  for (int n = 0; n < (sizeof(hdcaddress) / sizeof(int)); n++) {
    Serial.print("Looking for HDC at multiplexer: ");
    Serial.print(hdcaddress[n]);

    /* Initialise the sensor at the address of the address array*/
    tcaselect(hdcaddress[n]);
    // Default settings for HDC1080 sensors: - Heater off - 14 bit Temperature and Humidity Measurement Resolutions
    hdc1080.begin(0x40);

    Serial.println(" - found HDC sensor");

  }

  /************************* check and startup MPRLS sensor *****************************/
  for (int n = 0; n < (sizeof(mpladdress) / sizeof(int)); n++) {
    Serial.print("/nLooking for MPLRS at multiplexer: ");
    Serial.print(mpladdress[n]);
    /* Initialise the 1st sensor */
    tcaselect(mpladdress[n]);
    // setup MPRLS sensor
    if (! mpr.begin()) {
      Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    }
    Serial.println(" - found MPRLS sensor");
  }

  // Initialise the Client
  Serial.print(F("\nInit the WiFi module..."));
  // check for the presence of the breakout
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WINC1500 not present");
    // don't continue:
    while (true);
  }
  Serial.println("ATWINC OK!");

  WiFi.lowPowerMode(); // not 100% sure this is helping? check power profile with and without?

  fanSpeed = fanBaselineSpeed;


  
}

void loop() {

  // Setup connection to MQTT server
  MQTT_connect(); 

  // if this is the first time in loop then setup the pressure start readings
  if(!p1H_start){
    firstReadPressureSensors();  





    /// TEMP UNTIL SENSOR 3 WHICH IS AMBIENT IS ADDED TO HARDWARE
    p3_start = p2L_start;
    Serial.println("\nSensor first readings:");
    Serial.println(p1H_start);
    Serial.println(p2L_start);
    Serial.println(p3_start);


    
  }

  /************************* read sensors *****************************/
  Serial.println("\n\nNew reading: ");
  readBatterySensor();
  readTempHumSensors();
  readPressureSensors();

  /************************* update fan speed *****************************/
  updateFanSpeed();


  /************************* pause programme until time to measure *****************************/
  delay(timeBetweenReadings);

}

void updateFanSpeed(){



  p1H_diff = p1H - p3_start; // want the difference in btwn p1H and starting ambient
  if(p1H_diff > 10){
    fanSpeed = fanSpeed - (p1H_diff - 10);
    
  }else if(p1H_diff < 10){
    fanSpeed = fanSpeed + (10 - p1H_diff);
     
  }
  if(fanSpeed < 0){fanSpeed = 0;}
  if(fanSpeed > 255){fanSpeed = 255;}  
     
  Serial.print("Fanspeed: ");
  Serial.println(fanSpeed);


  Serial.println("\n==========================: ");
  Serial.print("Fanspeed: ");
  Serial.println(fanSpeed);
  Serial.print("p1H_diff: ");
  Serial.println(p1H_diff);
  Serial.print("p1H_start: ");
  Serial.println(p1H_start);
  Serial.print("p1H_target: ");
  Serial.println(p1H_target);


  analogWrite(pwmFanContol, fanSpeed); 
  paprFan.publish(float(fanSpeed));
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    uint8_t timeout = 10;
    while (timeout && (WiFi.status() != WL_CONNECTED)) {
      timeout--;
      delay(1000);
    }
  }

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}

void readPressureSensors(){

  /************************* read MPRLS sensor *****************************/
  for (int n = 0; n < (sizeof(mpladdress) / sizeof(int)); n++) {
    tcaselect(mpladdress[n]);
    getPressureData();

    switch (mpladdress[n]) {
      case 1:
        p1H = pressure_hPa;
        paprP1.publish(pressure_hPa);
        break;
      case 2:
        p2L = pressure_hPa;
        paprP2.publish(pressure_hPa);
        break;
      case 3:
        p3 = pressure_hPa;
        paprP3.publish(pressure_hPa);
        break;
      default:
        Serial.println("No MPRLS switch case found");
        break;
    }
  }
  Serial.print("\nSensor p1H:");
  Serial.print(p1H);
  Serial.print("\nSensor p2L:");
  Serial.print(p2L);
  Serial.print("\nSensor p3:");
  Serial.print(p3);  
  
}

void getPressureData() {

  /* could try and smooth the readings from the pressure sensors - take 10 and use mean?
    pressure_hPa = 0;
    for(int n=0; n<10; n++){
    pressure_hPa += mpr.readPressure();
    delay(20);
    }
    pressure_hPa = pressure_hPa/10;
  */
  // for time being just using one direct reading and using that
  pressure_hPa = mpr.readPressure();
  /*
  pressure_PSI = pressure_hPa / 68.947572932;
  Serial.print(", P=");
  Serial.print(pressure_hPa);
  Serial.print(" hPa, P=");
  Serial.print(pressure_PSI);
  Serial.print(" PSI");
  */

}

void firstReadPressureSensors(){

  /************************* read MPRLS sensor *****************************/
  for (int n = 0; n < (sizeof(mpladdress) / sizeof(int)); n++) {
    tcaselect(mpladdress[n]);
//    Serial.print("\nSensor:");
//    Serial.print(mpladdress[n]);
//    Serial.print(" = ");
    getPressureData();

    switch (mpladdress[n]) {
      case 1:
        p1H_start = pressure_hPa;
        paprP1s.publish(pressure_hPa);
        break;
      case 2:
        p2L_start = pressure_hPa;
        paprP2s.publish(pressure_hPa);
        break;
      case 3:
        p3_start = pressure_hPa;
        paprP3s.publish(pressure_hPa);
        break;
      default:
        Serial.println("No MPRLS switch case found");
        break;
    }

  }

  
}

void readTempHumSensors(){

  /************************* read HDC sensors *****************************/
  for (int n = 0; n < (sizeof(hdcaddress) / sizeof(int)); n++) {
    tcaselect(hdcaddress[n]);
//    Serial.print("\nSensor:");
//    Serial.print(hdcaddress[n]);
//    Serial.print(" = ");

    getTempHumData();

    switch (hdcaddress[n]) {
      case 0:
        paprT0.publish(temperature);
        paprH0.publish(humidity);
        break;
      case 4:
        paprT4.publish(temperature);
        paprH4.publish(humidity);
        break;
      case 5:
        paprT5.publish(temperature);
        paprH5.publish(humidity);
        break;
      case 6:
        paprT6.publish(temperature);
        paprH6.publish(humidity);
        break;
      case 7:
        paprT7.publish(temperature);
        paprH7.publish(humidity);
        break;
      default:
        Serial.println("No HDC switch case found");
        break;
    }
  }  
}

void getTempHumData() {

  delay(20); // was seeing an error of temp being 125 whilst humidity ok - pause before reading - needs 20 (!)
  // sensors seem to be fairly stable on first read so dont think it is useful to multi sample
  temperature = hdc1080.readTemperature();
  humidity = hdc1080.readHumidity();
//  Serial.print("T=");
//  Serial.print(temperature);
//  Serial.print("C, RH=");
//  Serial.print(humidity);
//  Serial.print("%");


}

void readBatterySensor(){

  /************************* read Battery Level *****************************/
  
 // battery voltage is 14.8, 12V Zener being used 1k voltage divider circuit - analog read at 14.8V is 430 
  batteryLevel = 12+ (2 * batteryResolution * analogRead(pinBattery));
  Serial.print("Battery: ");
  Serial.print(batteryLevel);
  paprBat.publish(batteryLevel);
    
}
