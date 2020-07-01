/**
 * Simple temperature sensor sketch for the LMZ 335 temperature sensor
 * https://coolcomponents.co.uk/products/lm335az-temperature-sensor
 * 
 * This is a two wire zener diode temperature sensor with a calibration of 10 mV/Kelvin
 * The sensor is wired as the lower element of a voltage divider, with the upper resistor having a value of about 5 kOhms.
 * 
 * 
 */

#define debug true
#define tempPin A1

//Supply voltage
float Vcc = 5.0f;
const float calibration = 10.0;


void setup(){
  
  if(debug) {
    Serial.begin(9600);
  }
  
}


void loop(){

  float tempVoltage = Vcc*analogRead(tempPin)/1024;
  
  //Apply calibration and convert to degrees Celsius
  float temp = (1000.0*tempVoltage/calibration) - 273.15; 
  
  if(debug){
    Serial.print("temp voltage: ");
    Serial.println(tempVoltage);
    Serial.print("temperature: ");
    Serial.println(temp);
  }

  delay(1000);

}
