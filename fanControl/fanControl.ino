/*
    PAPR code 4.0
    By Albert Corredera
    July 2020

    Not tested with real pressure sensors
    Reading data generated by another Arduino

*/

#include <avr/wdt.h>
#include <Wire.h>


//PINS used:
int lowBattWarning = 8; //low battery warning LED (orange)
int fan = 11;    // select the pin for the Fan
int lowBatteryLED = 12; //LED to show low battery levels
int buzzer = 13; //pin for the buzzer
int pinBattery = A0; // analogue input pin for battery level


//Variables:
int batteryVoltage = 0;
int batteryLevel; //varialbe to store the battery level
int batterycounting = 0; //variable to store the counting for the battery check during the while function
int powerUp = 0; //variable to check if it has been just switched on
bool stop;// = true;
int counting = 0;
int overallCounter = 0;

//gear system:
int gear = 0; //to determine the speed of the fan
int gearChanged = 0; //counting to check how many times we change through speeds
int gearCounting = 0; //how often we change the gears

//pressure variables
int masterPressureRange = 0; //to store the ideal pressure and have it as a reference
int pressureRange = 0; // the ideal pressure variable to play with
int pressureRangeMax = 0; // the ideal pressure maximum range
int pressureRangeMin = 0; // the ideal pressure minimum range
int sensors = 0; // the reading of the sensors combined.... would like to compare it against pressureRange


//Faults:

int fault = 0;    //to tell if there is a fault and which one:
/*
   Faults:
   Fault 101: Low Battery
   Fault 102: Faulty sensors start up
   Fault 103: very low pressure sensors
   Fault 104: very high pressure sensors
   Fault 105:

   Fault 1001: Watch Dog timer timed out... Don't think we could see this one... Anyway, it should reset the Arduino

*/

//Sensors variables:
float fullSpeedSensor_1 = 0.0F; // variable to store the value coming from the pressure sensor when fully powered
float restSensor_1 = 0.0F;  // variable to store the value coming from the pressure sensor when first swtiched ON (at rest)
float sensor_1 = 0.0F;  // variable to store the value coming from the pressure sensor
String dataSensor_1 = ""; //variable where to store incoming data
char c_1; // variable where we store incoming data

float fullSpeedSensor_2 = 0.0F; // variable to store the value coming from the pressure sensor when fully powered
float restSensor_2 = 0.0F;  // variable to store the value coming from the pressure sensor when first swtiched ON (at rest)
float sensor_2 = 0.0F;  // variable to store the value coming from the pressure sensor
String dataSensor_2 = ""; //variable where to store incoming data
char c_2; // variable where we store incoming data

float fullSpeedSensor_3 = 0.0F; // variable to store the value coming from the pressure sensor when fully powered
float restSensor_3 = 0.0F;  // variable to store the value coming from the pressure sensor when first swtiched ON (at rest)
float sensor_3 = 0.0F;  // variable to store the value coming from the pressure sensor
String dataSensor_3 = ""; //variable where to store incoming data
char c_3; // variable where we store incoming data


void setup() {
  pinMode(fan, OUTPUT); //Fan as an Output
  pinMode(buzzer, OUTPUT); //Buzzer
  analogWrite (fan, 0); //fan PWM speed 0-255
  digitalWrite (buzzer, LOW);
  digitalWrite (lowBattWarning, LOW);
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.begin(9600);  // start serial for output
  watchdogSetup(); // to set up watchdog parameters
}



void watchdogSetup(void) {
  cli(); //disable interrupts
  wdt_reset(); // reset WDT
  /*
     WDTCSR config:
     WDIE = 1: Interrupt enable
     WDE = 1: Reset enable
     WDP3 = 1: For 8sec time-out
     WDP2 = 0: For 8sec time-out
     WDP1 = 0: For 8sec time-out
     WDP0 = 1: For 8sec time-out
  */
  // to enter WD Config mode:
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  // Set Watchdog settings:
  WDTCSR = (1 << WDIE) | (1 << WDE) | (1 << WDP3)  | (0 << WDP2) | (0 << WDP1)  | (1 << WDP0);
  sei(); //enable interrupts
}



void loop() {
  
  wdt_reset(); // reset WDT
  
  if (powerUp == 0) { //to let the system stabilize on start up. We could add a sequence for the buzzer to indicate starting up...
    delay(2000);
  }

  wdt_reset(); // reset WDT

  //time to read all sensors:
  /*
    Wire.requestFrom(8, 7, stop);    // request 7 bytes from slave device #8

    while (Wire.available()) { // slave may send less than requested
      char c_1 = Wire.read(); //read a char from wire
      dataSensor_1 = dataSensor_1 + c_1; // write that char to string
    }

    sensor_1 = dataSensor_1.toFloat(); //converting the string to float number
    dataSensor_1 = ""; //to clear variable before next read. Not doing it lead to errors

    Wire.requestFrom(X, 7, stop);    // request 7 bytes from slave device #X

    while (Wire.available()) { // slave may send less than requested
      char c_2 = Wire.read();
      dataSensor_2 = dataSensor_1 + c_2;
    }

    sensor_2 = dataSensor_2.toFloat();
    dataSensor_2 = "";

    Wire.requestFrom(X, 7, stop);    // request 7 bytes from slave device #X

    while (Wire.available()) { // slave may send less than requested
      char c_3 = Wire.read();
      dataSensor_3 = dataSensor_1 + c_3;
    }

    sensor_3 = dataSensor_3.toFloat();
    dataSensor_3 = "";
  */

  batteryLevel = analogRead (pinBattery); // analogRead = 3.3V/1023 = 3.22mV resolution. If we are using a 5V input -> resolution must be changed! and ALL BATTERY_LEVEL UPDATED to the new resolution!!

  // checking those readings...

  //batteryVoltage = (batteryLevel * 6.44 / 1000) + 12; // Using a zener of 12V and a voltage divider (R+R) on pinBattery [so (3.22mV * 2 * batteryLevel)+12 ]. We measure last resistor to ground.

  if (batteryLevel < 373) {  // checking battery level is ok, if not report the fault. Low battery fault = {[(3.6V*4)-12]/2}/3.22mV*1000
    fault = 101;
  }

  if (powerUp == 1) { //to store data at full speed. First we run through powerUp == 0 (rest) so we skip this one first time.
    fullSpeedSensor_1 = sensor_1;
    fullSpeedSensor_2 = sensor_2;
    fullSpeedSensor_3 = sensor_3;
    powerUp = 2; //and move up the power up step
  }

  if (powerUp == 0 && fault == 0) { //to store data at start up and getting ready for full speed readings. [If there is a fault don't go in! = It means that battery is really low!]
    restSensor_1 = sensor_1;
    restSensor_2 = sensor_2;
    restSensor_3 = sensor_3;
    powerUp = 1; //and move up the power up step
    analogWrite (fan, 255);
    gear = 5;
    delay (2000);
  }

  wdt_reset(); // reset WDT

  if (powerUp == 2 /*|| powerUp = 3*/) { //here we could deal with the faults system, check every sensor and battery and create a fault system
    if (restSensor_1 < 10 || restSensor_1 > 1000) { // checking sensors are ok @rest, if not report the fault. We can do one of these for each sensor
      fault = 102;
    }
    /*if (fullSpeedSensor_1 < 10 || fullSpeedSensor_1 > 1000) { // checking sensors are ok @full speed, if not report the fault. We can do one of these for each sensor
      fault = XXX;
      }*/ //and so on

    powerUp = 3; //and move up the power up step
  }

  wdt_reset(); // reset WDT

  if (fault < 75 && powerUp == 3) {   // to give warnings or faults in working conditions and adjust speeds accordingly

    if (batteryLevel < 435) {  //low battery counting. Low battery warning = {[(3.7V*4)-12]/2}/3.22mV*1000
      digitalWrite (lowBattWarning, HIGH);
      batterycounting += 1;
      if (batterycounting > 100) {  // to start indicating low battery... maybe we should make a longer counting
        digitalWrite (buzzer, HIGH);
        digitalWrite (lowBatteryLED, HIGH);
        delay (1000);
        digitalWrite (buzzer, LOW);
        digitalWrite (lowBatteryLED, LOW);
        //fault += 1; ---> shall we increase the fault or wait until batteryLevel fails (see above)
        batterycounting = 0;
      }
    }

    wdt_reset(); // reset WDT

    // Are we going to keep measuring very low and very high pressures?? What are we going to do with them?
    /*
        if (sensor_1 < 10) {   // All sensors conditions goes here to make sure they are OK, if not fault goes up! We should think what to do with fan speeds. This one is for very low pressure limit
          fault += 1;
          analogWrite (fan, 255);
          delay (1000);
          if (fault > 25) {
            fault = 103 or leave it as fault+=1 (see above) and hope it solves the problem; //how many times are we going to allow the have a faulty sensor...25?
          }
        }

        if (sensor_1 > 500) {   // All sensors conditions goes here to make sure they are OK, if not fault goes up! We should think what to do with fan speeds. Very high pressure limit
          fault += 1;
          analogWrite (fan, 32);
          delay (1000);
          if (fault > 25) {
            fault = 104 or leave it as fault+=1 (see above) and hope it solves the problem; //how many times are we going to allow the have a faulty sensor...25?
          }
        }

        if (counting > 100 && fault <10){
        fault = 0;
        counting = 0;
        overallCounter += 1;
        }
    */


    if (fault < 20) {
      /* Ideally we would like to know in advance the readings that we expect from the sensors.
          If that is the case, we can move by a range of pressures. If we don't know, we should find an equation that suits our application and
          finds the range that we would like to work at.

          and...

          We don't want the fan constantly going up and down...
        masterPressureRange_1 = (fullSpeedSensor_1 + restSensor_1) / 3; //  the reference is 2/3rds of the mean pressure read ------------ not sure this will work or if it is useful

        // we might have to deal with these ones (see next 4 lines) somewhere else
        masterPressureRange = (sensor_1 - sensor_3); //---------------------------------------Check above and think which one is best (differential {sensorX-sensorY}? or 2/3? or another method?)
        pressureRange = masterPressureRange;
        pressureRangeMax = pressureRange + ((pressureRange * 20) / 100);
        pressureRangeMin = pressureRange - ((pressureRange * 20) / 100);
      */

      if (sensors > pressureRangeMax) {
        gear -= 1;
        if (gear < 1) {
          gear = 1;
        }
        gearChanged += 1;
      }

      if (sensors < pressureRangeMin) {
        gear += 1;
        if (gear > 5) {
          gear = 5;
        }
        gearChanged += 1;
      }

      gearCounting += 1;

      if (gearChanged > 10 && gearCounting < 20) {
        pressureRange = pressureRange + ((pressureRange * 10) / 100);
        pressureRangeMax = pressureRange + ((pressureRange * 20) / 100);
        pressureRangeMin = pressureRange - ((pressureRange * 20) / 100);
      }

      if (gearChanged < 5 && gearCounting > 30) {
        gearChanged = 0;
        gearCounting = 0;
      }

      if (gear == 0) {
        analogWrite (fan, 0); // fan speed = 0%
      }

      if (gear == 1) {
        analogWrite (fan, 155); // fan speed = 60%
      }

      if (gear == 2) {
        analogWrite (fan, 180); // fan speed = 70%
      }

      if (gear == 3) {
        analogWrite (fan, 205); // fan speed = 80%
      }

      if (gear == 4) {
        analogWrite (fan, 230); // fan speed = 90%
      }

      if (gear == 5) {
        analogWrite (fan, 255); // fan speed = 100%
      }
    }
  }

  wdt_reset(); // reset WDT

  /* Here we should send all data
     to access point
     just before dealing with faults

     send millis(); + all variables that are useful to us...

  */

  wdt_reset(); // reset WDT

  if (fault > 50) { // When there is an unrecoverable fault in the system we are going full speed!! And as loud and bright as possible..
    analogWrite (fan, 255);
    if (fault == 101) {
      digitalWrite (lowBatteryLED, HIGH);
    }
    digitalWrite (buzzer, HIGH);
    delay (1000);
    digitalWrite (buzzer, LOW);
    delay (1000);
  }

  counting += 1;
  wdt_reset(); // reset WDT

}


ISR(WDT_vect) // Watchdog timer interrupt.
{
  //Last wish interrupt from the watchdog
  // Include your code here - be careful not to use functions they may cause the interrupt to hang and
  // prevent a reset.
  fault = 1001;
  analogWrite (fan, 255);
  digitalWrite (buzzer, HIGH);
  digitalWrite (lowBatteryLED, HIGH);
  //send data to server
}