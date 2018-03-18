#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "vars.h"

// defines
#define TEMP_DECIMALS 4                     // 4 decimals of output
#define TEMPERATURE_PRECISION 12            // 12 bits precision
#define DELAY_POST_DATA 120000L            // delay between updates, in milliseconds
#define DELAY_PRINT 15000L
#define DELAY_READ 5000L

// ***** Temperature sensors *****
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress addresses[20];
float temperatures[20];
uint8_t sensorCount;
      
// **** WiFi *****
ESP8266WiFiMulti WiFiMulti;
unsigned long lastConnect = millis();
unsigned long lastPrint = millis();
unsigned long lastRead = millis();

void setup() {
  // initialize serial
  Serial.begin(115200);

  // Start up the sensors
  Serial.println("Initializing sensors");
  sensors.begin();
  Serial.println("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // init wifi
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, pass);
}

void loop() {
  if ((WiFiMulti.run() != WL_CONNECTED)) {
    // not connected - wait
    Serial.println("Not connected...");
    delay(1000);
    yield();
    
  } else {
    // read sensors if applicable
    if ((((unsigned long)millis()) - lastPrint) > DELAY_READ) {
      lastRead = millis();
      readTemperatures();
    }
    yield();
  
    // print temperatures
    if ((((unsigned long)millis()) - lastPrint) > DELAY_PRINT) {
      lastPrint = millis();
      printTemperatures();
    }
    yield();
  
    // post temperatures
    if ((((unsigned long)millis()) - lastConnect) >= DELAY_POST_DATA) {
      lastConnect = millis();
      sendTemperatures();
    }
    yield();
  }
}

void readTemperatures() {
  // get sensors
  sensors.begin();
  uint8_t newSensorCount = sensors.getDeviceCount();
  if (newSensorCount != sensorCount) {
    Serial.print("Sensor count has changed - was ");
    Serial.print(sensorCount);
    Serial.print(" now ");
    Serial.println(newSensorCount);
    sensorCount = newSensorCount;
  }

  // set resolution and request
  sensors.setResolution(TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  
  // get temperatures and addresses
  for (uint8_t i=0; i<sensorCount; i++) {
    sensors.getAddress(addresses[i], i);
    temperatures[i] = sensors.getTempCByIndex(i);
  }
}

void printTemperatures() {
  if (sensorCount <= 0) {
    Serial.println("No sensors to print data from...");
    return;
  }
  for (uint8_t i=0; i<sensorCount; i++) {
    String straddr = deviceAddressToString(addresses[i]);
    Serial.print(straddr);
    Serial.print(": ");
    Serial.println(temperatures[i], TEMP_DECIMALS);
  }
}

void sendTemperatures() {
  // prepare content
  String content;
  for (uint8_t i=0; i<sensorCount; i++) {
    if (i == 0) {
      content = String("[{\"sensorId\": \"");
    } else {
      content += String(", {\"sensorId\": \"");
    }
    content += deviceAddressToString(addresses[i]) + String("\", \"sensorValue\": ") + String(temperatures[i], TEMP_DECIMALS) + String("}");
  }
  content += String("]");
  Serial.print("Content: ");
  Serial.println(content);

  // send
  HTTPClient http;
  http.begin(server);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Content-Length", String(content.length() + 4));
  int httpCode = http.POST(content);
  String payload = http.getString();                  //Get the response payload

  Serial.println(httpCode);   //Print HTTP return code
  Serial.println(payload);    //Print request response payload

  http.end();
  
  Serial.println("Sent to server...");
}

String deviceAddressToString(DeviceAddress deviceAddress){
    static char return_me[18];
    static char *hex = "0123456789ABCDEF";
    uint8_t i, j;

    for (i=0, j=0; i<8; i++) 
    {
         return_me[j++] = hex[deviceAddress[i] / 16];
         return_me[j++] = hex[deviceAddress[i] & 15];
    }
    return_me[j] = '\0';
    
    return String(return_me);
}
