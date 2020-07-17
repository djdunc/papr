/*  Project PAPR July 2020
 *  
 *  Duncan Wilson https://github.com/djdunc/papr
 *  
 *  Hardware: Adafruit Feather M0, 3x Adafruit MPRLS, 5 Closed Cube HDC1080 temp / hum I2C sensor and TCA9548A multiplexer
 *  
 *  MPRLS and HDC1080 connected via SCL / SDA
 *  
 *  Sketch tests to see if 8 sensors are readable via multiplexer
 *  
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MPRLS.h>
#include <ClosedCube_HDC1080.h>

/************************* Configure address of multiplexer *****************************/
#define TCAADDR 0x70

/************************* MPRLS  pressure Setup *****************************/
// You dont *need* a reset and EOC pin for most uses, so we set to -1 and don't connect
#define RESET_PIN  -1  // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1  // set to any GPIO pin to read end-of-conversion by pin
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);
float pressure_hPa = 0;
float pressure_PSI = 0;

/************************* HDC10180 temp hum Setup *****************************/
ClosedCube_HDC1080 hdc1080;
float temperature = 0;
float humidity = 0;

/************************* Function to connect to Multiplexer *****************************/
void tcaselect(uint8_t i) {
  if (i > 7) return;          // only eight I2C devices on multiplexer
 
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();  
}

/************************* define sensor addresses *****************************/
int hdcaddress[] = {0,4,5,6,7};
int mpladdress[] = {1,2};       // will need to add in 3 when third sensor added


void setup(void) 
{
  Serial.begin(9600);
  delay(2000);

  Serial.println("\n\n\Test"); Serial.println("");


  /************************* startup HDC10180 sensor *****************************/
  for (int n = 0; n < (sizeof(hdcaddress)/sizeof(int)); n++) {
    Serial.print("/nLooking for HDC at multiplexer: ");
    Serial.print(hdcaddress[n]);

    /* Initialise the sensor at the address of the address array*/
    tcaselect(hdcaddress[n]);
    // Default settings for HDC1080 sensors: - Heater off - 14 bit Temperature and Humidity Measurement Resolutions
    hdc1080.begin(0x40);
    
    Serial.println(" - found HDC sensor");  
  
  } 
  
  /************************* check and startup MPRLS sensor *****************************/
  for (int n = 0; n < (sizeof(mpladdress)/sizeof(int)); n++) {
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

}

void loop(void) 
{

    Serial.println("\n\nNew reading");
    
    for (int n = 0; n < (sizeof(hdcaddress)/sizeof(int)); n++) {
      tcaselect(hdcaddress[n]);
      Serial.print("\nSensor:");
      Serial.print(hdcaddress[n]);
      Serial.print(" = ");

      getTempHumData();
    } 
    for (int n = 0; n < (sizeof(mpladdress)/sizeof(int)); n++) {
      tcaselect(mpladdress[n]);
      Serial.print("\nSensor:");
      Serial.print(mpladdress[n]);
      Serial.print(" = ");
     getPressureData();
    }
      
    delay(10000);
}



void getPressureData(){
  pressure_hPa = mpr.readPressure();
  pressure_PSI = pressure_hPa / 68.947572932;
  Serial.print("P=");
  Serial.print(pressure_hPa);
  Serial.print(" hPa, P=");
  Serial.print(pressure_PSI);
  Serial.print(" PSI");
}

void getTempHumData() {
  delay(20); // was seeing an error of temp being 125 whilst humidity ok - pause before reading - needs 20 (!)
  temperature = hdc1080.readTemperature();
  humidity = hdc1080.readHumidity();
  Serial.print("T=");
  Serial.print(temperature);
  Serial.print("C, RH=");
  Serial.print(humidity);
  Serial.print("%");
    
 }
 
