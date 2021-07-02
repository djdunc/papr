// list out all the feeds required - hard coded for flexibility in changing the topic feeds

Adafruit_MQTT_Publish paprT0 = Adafruit_MQTT_Publish(&mqtt, "papr/temperature/1/");
Adafruit_MQTT_Publish paprH0 = Adafruit_MQTT_Publish(&mqtt, "papr/humidity/1/");

Adafruit_MQTT_Publish paprP1 = Adafruit_MQTT_Publish(&mqtt, "papr/pressure/1/");

Adafruit_MQTT_Publish paprP2 = Adafruit_MQTT_Publish(&mqtt, "papr/pressure/2/");

Adafruit_MQTT_Publish paprP3 = Adafruit_MQTT_Publish(&mqtt, "papr/pressure/3/");

Adafruit_MQTT_Publish paprT4 = Adafruit_MQTT_Publish(&mqtt, "papr/temperature/2/");
Adafruit_MQTT_Publish paprH4 = Adafruit_MQTT_Publish(&mqtt, "papr/humidity/2/");

Adafruit_MQTT_Publish paprT5 = Adafruit_MQTT_Publish(&mqtt, "papr/temperature/3/");
Adafruit_MQTT_Publish paprH5 = Adafruit_MQTT_Publish(&mqtt, "papr/humidity/3/");

Adafruit_MQTT_Publish paprT6 = Adafruit_MQTT_Publish(&mqtt, "papr/temperature/4/");
Adafruit_MQTT_Publish paprH6 = Adafruit_MQTT_Publish(&mqtt, "papr/humidity/4/");

Adafruit_MQTT_Publish paprT7 = Adafruit_MQTT_Publish(&mqtt, "papr/temperature/5/");
Adafruit_MQTT_Publish paprH7 = Adafruit_MQTT_Publish(&mqtt, "papr/humidity/5/");
