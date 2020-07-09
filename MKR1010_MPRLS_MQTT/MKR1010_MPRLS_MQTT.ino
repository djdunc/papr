/*  Project PAPR July 2020
 *  
 *  Duncan Wilson https://github.com/djdunc/papr
 *  
 *  Hardware: Arduino MKR WIFI 1010, Adafruit MPRLS and a Closed Cube HDC1080 temp / hum I2C sensor
 *  
 *  MPRLS and HDC1080 connected via pin 11 and 12 (SCL / SDA)
 *  
 *  Sketch is used to sends readings over wifi to an MQTT server
 *  
 *  Get date and time - uses the ezTime library at https://github.com/ropg/ezTime   
 *  - note: edited the eztime.h file to comment out #define EZTIME_CACHE_EEPROM since MKR1010 has no EEPROM
 *  and then show data from a DHT22 on a web page served by the MKR1010 and
 *  push data to an MQTT server - uses library from https://pubsubclient.knolleary.net
 *  
 */
   
#include <ClosedCube_HDC1080.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <ezTime.h>
#include <PubSubClient.h>
#include <Adafruit_MPRLS.h>

#include "arduino_secrets.h" 
/*  access point credentials stored in seperate file "arduino_secrets.h" with the pattern:
    #define SECRET_SSID "my SSID name"
    #define SECRET_PASS "my SSID password" 
*/

// Wifi 
char ssid[] = SECRET_SSID;        // your network SSID (name)
char password[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;
WiFiServer wserver(80);
WiFiClient wclient;

// set up HDC10180 temp hum sensor
ClosedCube_HDC1080 hdc1080;
float temperature = 0;
float humidity = 0;

// You dont *need* a reset and EOC pin for most uses, so we set to -1 and don't connect
#define RESET_PIN  -1  // set to any GPIO pin # to hard-reset on begin()
#define EOC_PIN    -1  // set to any GPIO pin to read end-of-conversion by pin
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);
float pressure_hPa = 0;
float pressure_PSI = 0;

// MQTT
const char* mqtt_server = "bats.cetools.org";   // bats is one of our live mqtt servers in CE
PubSubClient mqttClient(wclient);
long lastMsg = 0;
char msg[50];
char topic[50];
int value = 0;


// Date and time
Timezone GB;

void setup() {

  // declare the built in LED and turn it off
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);  // Turn the LED off 

  Serial.begin(9600);
  delay(100);

  // Default settings for HDC1080 sensors: - Heater off - 14 bit Temperature and Humidity Measurement Resolutions
  hdc1080.begin(0x40);

  // setup MPRLS sensor
  if (! mpr.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Found MPRLS sensor");  

  // set up wifi and get current date time
  startWifi();
  startWebserver();
  syncDate();

  // start MQTT server
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);

}

void loop() {
  
  getTempHumData();
  getPressureData();
  serveWebPage();
  delay(1000);

}

void getTempHumData() {

  temperature = hdc1080.readTemperature();
  humidity = hdc1080.readHumidity();
  Serial.print("T=");
  Serial.print(temperature);
  Serial.print("C, RH=");
  Serial.print(humidity);
  Serial.print("%");
  
  snprintf (topic, 50, "papr/hdc1080/temperature/");
  sendMQTT(topic, temperature);
  snprintf (topic, 50, "papr/hdc1080/humidity/");
  sendMQTT(topic, humidity);
  
 }

 void getPressureData(){
  pressure_hPa = mpr.readPressure();
  pressure_PSI = pressure_hPa / 68.947572932;
  Serial.print(", P=");
  Serial.print(pressure_hPa);
  Serial.print(" hPa, P=");
  Serial.print(pressure_PSI);
  Serial.println(" PSI");
  
  snprintf (topic, 50, "papr/MPRLS/pressure/");
  sendMQTT(topic, pressure_hPa);
  snprintf (topic, 50, "papr/MPRLS/pressurePSI/");
  sendMQTT(topic, pressure_PSI);
 }


// MQTT send function - makes sure connection exists, takes readings and publishes them to buffer and loop sends to server 
void sendMQTT(char* topic, float val) {

  if (!mqttClient.connected()) {
    reconnect();
  }
  snprintf (msg, 50, "%.1f", val);
//  Serial.print("Publish message: ");
//  Serial.print(msg);
//  Serial.print(" to topic: ");
//  Serial.println(topic);
  mqttClient.publish(topic, msg);   // publish to "(topic, payload)"

  mqttClient.loop();

}

// MQTT callback function - when an inbound message is received this function processes it
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED on 
  } else {
    digitalWrite(LED_BUILTIN, LOW);  // Turn the LED off 
  }

}


// MQTT reconnect function - establishes connection to the MQTT server, publishes a connection message and subscribes to topics to listen for
void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "MKR1010Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("plant/minty/outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe("plant/minty/inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void startWifi() {

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
    
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // check to see if connected and wait until you are
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

// sets date on device
void syncDate() {
  // get real date and time
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  GB.setLocation("Europe/London");
  Serial.println("London time: " + GB.dateTime());

}



// starts a webserver if you are wanting to display a webpage locally
void startWebserver() {
  // when connected and IP address obtained start HTTP server
  wserver.begin();
  Serial.println("HTTP server started");
}


void serveWebPage(){
  
  WiFiClient wclient = wserver.available();   // listen for incoming clients

  if (wclient) {

  String currentLine = "";              // make a String to hold incoming data from the client
  
  while (wclient.connected()) {         // loop while the client's connected
  
   if (wclient.available()) {          // if there's bytes to read from the client,
      char c = wclient.read();          // read a byte, then
      Serial.write(c);                  // print it out the serial monitor
      
      if (c == '\n') {                  // if the byte is a newline character

        // if the current line is blank, you got two newline characters in a row.
        // that's the end of the client HTTP request, so send a response:
        if (currentLine.length() == 0) {
          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
          // and a content-type so the client knows what's coming, then a blank line:
          wclient.println("HTTP/1.1 200 OK");
          wclient.println("Content-type:text/html");
          wclient.println("Connection: close");  // the connection will be closed after completion of the response
          wclient.println("Refresh: 60");  // refresh the page automatically every 5 sec
          wclient.println();

          // the content of the HTTP response follows the header:
          wclient.println(SendHTML(temperature, humidity, pressure_hPa, pressure_PSI));

          // The HTTP response ends with another blank line:
          wclient.println();
          // break out of the while loop:
          break;
        } else {    // if you got a newline, then clear currentLine:
          currentLine = "";
        }
      } else if (c != '\r') {  // if you got anything else but a carriage return character,
        currentLine += c;      // add it to the end of the currentLine
      }
      
      // Check to see if the client request was "GET /H" or "GET /L":
      if (currentLine.endsWith("GET /H")) {
        digitalWrite(LED_BUILTIN, HIGH);               // GET /H turns the LED on
      }
      if (currentLine.endsWith("GET /L")) {
        digitalWrite(LED_BUILTIN, LOW);                // GET /L turns the LED off
      }

    }
  }
  // close the connection:
  wclient.stop();
  Serial.println("client disconnected");    
  
  }
  
  delay(50);
  
}

// This is a string to hold the text of a webpage to be served from webserver
String SendHTML(float Temperaturestat, float Humiditystat, float pHPA, float pPSI) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>MKR1010 Report</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>MKR1010 Report</h1>\n";

  ptr += "<p>Temperature: ";
  ptr += (int)Temperaturestat;
  ptr += " C</p>";
  ptr += "<p>Humidity: ";
  ptr += (int)Humiditystat;
  ptr += "%</p>";

  ptr += "<p>Pressure hPa: ";
  ptr += (int)pHPA;
  ptr += " C</p>";
  ptr += "<p>Pressure PSI: ";
  ptr += (int)pPSI;
  ptr += "%</p>";

  ptr += "<p>Sampled on: ";
  ptr += GB.dateTime("l,");
  ptr += "<br>";
  ptr += GB.dateTime("d-M-y H:i:s T");
  ptr += "</p>";
  ptr += "<p>Click <a href=\"/H\">here</a> turn the LED on";
  ptr += "<br>";
  ptr += "Click <a href=\"/L\">here</a> turn the LED off";
  ptr += "</p>";
  
  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
