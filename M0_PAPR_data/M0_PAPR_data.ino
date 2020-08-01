/*  Project PAPR July 2020

    Duncan Wilson https://github.com/djdunc/papr

    Hardware: Adafruit Feather M0, 3x Adafruit MPRLS, 5 Closed Cube HDC1080 temp / hum I2C sensor and TCA9548A multiplexer

    MPRLS and HDC1080 connected via SCL / SDA

    Sketch to post data to RPi server and collect metrics from battery / fans

    Sketch builds on libraries including Adafruit_MQTT, Wifi101, ClosedCube_HDC1080, Adafruit_MPRLS

    Note: 
    - for ClosedCube library on multiplexer, had to edit the .cpp in docs/arduino/libraries folder to make delay 20 not 9
    - make sure all SAMD BSP's are upto date in board manager - both adafruit and arduino ones

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

float   p1H = 0;              // current pressure readings - note p1 is high pressure in hood
float   p2L = 0;              // note p2 is low pressure before fan (detects filter / leakage issues) 
float   p3 = 0;               // note p3 is ambient air pressure
float   p1H_start = 0;        // store starting values so we can watch for drift
float   p2L_start = 0;
float   p3_start = 0;
float   p1H_diff = 0;         // current reading minus start reading
float   p2L_diff = 0;         
float   p3_diff = 0;
int     pChanged = 0;         // pressure changed - true or false - if true resets start pressure         

/*************************                            *****************************/
/************************* values to tweak operation  *****************************/
/*************************                            *****************************/

float   p1H_target = 40;      // target pressure hPa (campared to ambient) 
float   p2L_target = -20;     // pressure in hPa below ambient (if above this create an alarm)
float   p3_target = 2;        // amount of drift allowed in hPa (if observed beyond consecutively, resets baseline)
int     fanSpeed = 200;       // this is the live PWM value to control the fan (0 to 255)
int     fanTargetSpeed = 200; // this is the PWM value to control the fan at "ideal" speed
int     loopDelay = 1000;     // delay between each each cycle of readings


#define DEBUG                 // comment out to remove all serial prints and save memory / time


/******************************** Moving Average  ****************************************/
const int queue_size = 10;

float queue1[queue_size];
float queue2[queue_size];
float queue3[queue_size];

int queue_index_1 = 0;
int queue_index_2 = 0;
int queue_index_3 = 0;

float sum1 = 0.0f;
float sum2 = 0.0f;
float sum3 = 0.0f;


void setup() {
  Serial.begin(9600);
  delay(2000);

  for(int i = 0; i < queue_size; i++){
    queue1[i] = 0.0f;
    queue2[i] = 0.0f;
    queue3[i] = 0.0f;
  }

  #ifdef DEBUG
  Serial.println("\n\n\=====================================================");
  Serial.println("PAPR Arduino Sketch for fan control and sensor data gathering. July 2020.\n\n");
  #endif

  // Define wifi pins for the WINC1500 - needed for SAMD boards
  WiFi.setPins(WINC_CS, WINC_IRQ, WINC_RST, WINC_EN);

  // Declare the built in LED and turn it off
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);  //  Turn the RED LED near the USB off

  // Declare PWM controller for fan
  pinMode(fanSpeed, OUTPUT);


  /************************* loop to startup HDC10180 sensor *****************************/
  for (int n = 0; n < (sizeof(hdcaddress) / sizeof(int)); n++) {

    #ifdef DEBUG
    Serial.print("Looking for HDC at multiplexer: ");
    Serial.print(hdcaddress[n]);
    #endif

    /* Initialise the sensor at the address of the address array*/
    tcaselect(hdcaddress[n]);
    // Default settings for HDC1080 sensors: - Heater off - 14 bit Temperature and Humidity Measurement Resolutions
    hdc1080.begin(0x40);

  }

  /************************* check and startup MPRLS sensor *****************************/
  for (int n = 0; n < (sizeof(mpladdress) / sizeof(int)); n++) {
    #ifdef DEBUG
    Serial.print("/nLooking for MPLRS at multiplexer: ");
    Serial.print(mpladdress[n]);
    #endif
    /* Initialise the 1st sensor */
    tcaselect(mpladdress[n]);
    // setup MPRLS sensor
    if (! mpr.begin()) {
      #ifdef DEBUG
      Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
      #endif
    }
  }

  /************************* Initialise WIFI  *****************************/
  #ifdef DEBUG
  Serial.print(F("\nInit the WiFi module..."));
  #endif
  // check for the presence of the breakout
  if (WiFi.status() == WL_NO_SHIELD) {
    #ifdef DEBUG
    Serial.println("WINC1500 not present");
    #endif
    // don't continue:
    while (true);
  }
  #ifdef DEBUG
  Serial.println("ATWINC OK!");
  #endif

  WiFi.lowPowerMode(); // not 100% sure this is helping? check power profile with and without?
  
}

void loop() {

  // Setup connection to MQTT server
  MQTT_connect(); 

  // if this is the first time in loop then setup the pressure start readings
  if(!p1H_start || pChanged){
    #ifdef DEBUG
    Serial.println("\n\nFirst reading or change in pressure detected so reseting baseline. ");
    #endif
    firstReadPressureSensors();      
  }

  /************************* read sensors *****************************/
  #ifdef DEBUG
  Serial.println("\n\nNew reading: ");
  #endif
  readBatterySensor();
  readTempHumSensors();
  readPressureSensors();

  /************************* update fan speed *****************************/
  updateFanSpeed();


  /************************* pause programme until time to measure *****************************/
  delay(loopDelay);

}

void updateFanSpeed(){

/*
  if (fanSpeed <= 50 || fanSpeed >= 255) {
    fanTargetSpeed = -fanTargetSpeed;
  }
  fanSpeed = fanSpeed + fanTargetSpeed;
*/ 

  p1H_diff = p1H - p3_start; // want the difference in btwn p1H and starting ambient
  p2L_diff = p2L - p3_start;
  p3_diff = p3 - p3_start; 
    
  if(p1H_diff > 10){
    fanSpeed = fanSpeed - (p1H_diff - 10);
    
  }else if(p1H_diff < 10){
    fanSpeed = fanSpeed + (10 - p1H_diff);
     
  }
  if(fanSpeed < 0){fanSpeed = 0;}
  if(fanSpeed > 255){fanSpeed = 255;}  
    
  #ifdef DEBUG
  Serial.print("\nFanspeed: ");
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
  #endif

  analogWrite(fanSpeed, fanSpeed); 
  paprFan.publish(float(fanSpeed));
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {

    #ifdef DEBUG
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    #endif
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

  #ifdef DEBUG
  Serial.print("Connecting to MQTT... ");
  #endif

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    #ifdef DEBUG
    Serial.println("Retrying MQTT connection in 5 seconds...");
    #endif
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  #ifdef DEBUG
  Serial.println("MQTT Connected!");
  #endif
}

void readPressureSensors(){

  /************************* read MPRLS sensor *****************************/
  for (int n = 0; n < (sizeof(mpladdress) / sizeof(int)); n++) {
    tcaselect(mpladdress[n]);
    getAveragePressureData(n+1);

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
        break;
    }
  }
  
  #ifdef DEBUG
  Serial.print("\nSensor p1H:");
  Serial.print(p1H);
  Serial.print("\nSensor p2L:");
  Serial.print(p2L);
  Serial.print("\nSensor p3:");
  Serial.print(p3);  
  #endif
  
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
  pressure_PSI = pressure_hPa / 68.947572932;

  #ifdef DEBUG
  Serial.print("\nP=");
  Serial.print(pressure_hPa);
  Serial.print(" hPa, P=");
  Serial.print(pressure_PSI);
  Serial.print(" PSI");
  #endif

}

void getAveragePressureData(int sensor) {
  
  float pressure_temp = mpr.readPressure();
  
  /**************** Calculate moving average ****************/
  
  switch(sensor) {
    
    case 1:
      calculateMovingAverage(pressure_temp, queue1, sum1, queue_index_1);
      break;

    case 2:
      calculateMovingAverage(pressure_temp, queue2, sum2, queue_index_2);
      break;

    case 3:
      calculateMovingAverage(pressure_temp, queue3, sum3, queue_index_3);
      break; 

     default:
      //error
      Serial.println("Moving average switch error"); 
  }
}

void calculateMovingAverage(float pressure, float* queue, float& sum, int& queue_index){

  // subtract the last reading:
  sum = sum - queue[queue_index];

  queue[queue_index] = pressure;
  
  // add the reading to the total:
  sum = sum + queue[queue_index];
  
  // advance to the next position in the array:
  queue_index++;

  // if we're at the end of the array...
  if (queue_index >= queue_size) {
    // ...wrap around to the beginning:
    queue_index = 0;
  }

  // calculate the average:
  pressure_hPa = sum / queue_size;
  pressure_PSI = pressure_hPa / 68.947572932;


  #ifdef DEBUG
    Serial.print("\nP=");
    Serial.print(pressure_hPa);
    Serial.print(" hPa, P=");
    Serial.print(pressure_PSI);
    Serial.print(" PSI");
  #endif
  
}

void firstReadPressureSensors(){

  /************************* read MPRLS sensor *****************************/
  for (int n = 0; n < (sizeof(mpladdress) / sizeof(int)); n++) {
    tcaselect(mpladdress[n]);
    #ifdef DEBUG
    Serial.print("\nSensor:");
    Serial.print(mpladdress[n]);
    Serial.print(": ");
    #endif
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
        break;
    }
  } 
}

void readTempHumSensors(){

  /************************* read HDC sensors *****************************/
  for (int n = 0; n < (sizeof(hdcaddress) / sizeof(int)); n++) {
    tcaselect(hdcaddress[n]);
    #ifdef DEBUG
    Serial.print("\nSensor:");
    Serial.print(hdcaddress[n]);
    Serial.print(" = ");
    #endif

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
        break;
    }
  }  
}

void getTempHumData() {

  delay(20); // was seeing an error of temp being 125 whilst humidity ok - pause before reading - needs 20 (!)
  // sensors seem to be fairly stable on first read so dont think it is useful to multi sample
  temperature = hdc1080.readTemperature();
  humidity = hdc1080.readHumidity();
  #ifdef DEBUG
  Serial.print("T=");
  Serial.print(temperature);
  Serial.print("C, RH=");
  Serial.print(humidity);
  Serial.print("%");
  #endif


}

void readBatterySensor(){

  /************************* read Battery Level *****************************/
  
 // battery voltage is 14.8, 12V Zener being used 1k voltage divider circuit - analog read at 14.8V is 430 
  batteryLevel = 12+ (2 * batteryResolution * analogRead(pinBattery));
  #ifdef DEBUG
  Serial.print("Battery: ");
  Serial.print(batteryLevel);
  #endif
  paprBat.publish(batteryLevel);
    
}
