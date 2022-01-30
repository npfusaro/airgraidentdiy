/**
 * This sketch connects an AirGradient DIY sensor to a WiFi network, and runs a
 * tiny HTTP server to serve air quality metrics to Prometheus.
 */

#include <AirGradient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <BME280I2C.h>

#include <Wire.h>
#include "SSD1306Wire.h"

AirGradient ag = AirGradient();
BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                  // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

/////
// Config ----------------------------------------------------------------------

// set sensors that you do not use to false
boolean hasPM = false;
boolean hasCO2 = false;
boolean hasSHT = false;
boolean hasBME280 = true;


// set to true to switch display from Celcius to Fahrenheit
boolean inF = true;

// Optional.
const char* deviceId = "";

// WiFi and IP connection info.
const char* ssid = "SSID HERE";
const char* password = "PASSWORD";
const int port = 9926;

#define staticip
#ifdef staticip
IPAddress static_ip(192, 168, 2, 001);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
#endif

// The frequency of measurement updates.
const int updateFrequency = 5000;

// For housekeeping.
long lastUpdate;
int counter = 0;

String CO2 = "0";
int PM25 = 0;
float Temperature = 0;
float Humidity = 0;
float Pressure = 0;


// Config End ------------------------------------------------------------------

SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_64_48);
ESP8266WebServer server(port);

void setup() {
  Serial.begin(9600);

  // Init Display.
  display.init();
  showTextRectangle("Init", String(ESP.getChipId(),HEX),false);

  // Enable sensors.
  if(hasPM){
      ag.PMS_Init();
  }
  if(hasCO2){
      ag.CO2_Init();
  }
  if(hasSHT){
      ag.TMP_RH_Init(0x44);
  }
  if(hasBME){
      bme.begin();
  }

  // Set static IP address if configured.
  #ifdef staticip
  WiFi.config(static_ip,gateway,subnet);
  #endif

  // Set WiFi mode to client (without this it may try to act as an AP).
  WiFi.mode(WIFI_STA);
  
  // Configure Hostname
  if ((deviceId != NULL) && (deviceId[0] == '\0')) {
    Serial.printf("No Device ID is Defined, Defaulting to board defaults");
  }
  else {
    wifi_station_set_hostname(deviceId);
    WiFi.setHostname(deviceId);
  }
  
  // Setup and wait for WiFi.
  WiFi.begin(ssid, password);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    showTextRectangle("Trying to", "connect...", false);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Hostname: ");
  Serial.println(WiFi.hostname());
  server.on("/", HandleRoot);
  server.on("/metrics", HandleRoot);
  server.onNotFound(HandleNotFound);

  server.begin();
  Serial.println("HTTP server started at ip " + WiFi.localIP().toString() + ":" + String(port));
  showTextRectangle("Listening To", WiFi.localIP().toString() + ":" + String(port),true);
}

void loop() {
  long now = millis();

  server.handleClient();
  if ((now - lastUpdate) > updateFrequency) {
    // Take a measurement at a fixed interval.
    getData();
    displayData();
    lastUpdate = millis();
  }
}

String GenerateMetrics() {
  String message = "";
  String idString = "{id=\"" + String(deviceId) + "\",mac=\"" + WiFi.macAddress().c_str() + "\"}";

  if(hasPM){
    message += "# HELP pm02 Particulate Matter PM2.5 value\n";
    message += "# TYPE pm02 gauge\n";
    message += "pm02";
    message += idString;
    message += String(PM25);
   message += "\n";
  }

  if(hasCO2){
    message += "# HELP rco2 CO2 value, in ppm\n";
    message += "# TYPE rco2 gauge\n";
    message += "rco2";
    message += idString;
    message += String(CO2);
    message += "\n";
  }

  if(hasSHT || hasBME280){
    message += "# HELP atmp Temperature, in degrees Celsius\n";
    message += "# TYPE atmp gauge\n";
    message += "atmp";
    message += idString;
    message += String(Temperature);
    message += "\n";
  }

  if(hasSHT || hasBME280){
    message += "# HELP rhum Relative Humidity, in percent\n";
    message += "# TYPE rhum gauge\n";
    message += "rhum";
    message += idString;
    message += String(Humidity);
    message += "\n";
  }

  if(hasBME280){
    message += "# HELP pres Barometric Pressure, in hPa\n";
    message += "# TYPE pres gauge\n";
    message += "pres";
    message += idString;
    message += String(Pressure);
    message += "\n";
  }

  return message;
}

void HandleRoot() {
  server.send(200, "text/plain", GenerateMetrics() );
}

void HandleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message);
}

// DISPLAY
void showTextRectangle(String ln1, String ln2, boolean small) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (small) {
    display.setFont(ArialMT_Plain_10);
  } else {
    display.setFont(ArialMT_Plain_16);
  }
  display.drawString(0, 0, ln1);
  display.drawString(0, 24, ln2);
  display.display();
}

void getData(){

  if(hasSHT){
    TMP_RH stat = ag.periodicFetchData();
    Temperature = stat.t;
    Humidity = stat.rh;
  }
  
  if(hasCO2){
    String tempCO2 = ag.getCO2(3);
    if(tempCO2 != "NULL" || tempCO2 != "17410"){
      CO2 = tempCO2;
    }
  }
  
  if(hasBME280){
    BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
    BME280::PresUnit presUnit(BME280::PresUnit_hPa);
    bme.read(Pressure, Temperature, Humidity, tempUnit, presUnit);
  }

  if(hasPM){
    PM25 = ag.getPM2_Raw();
  }
  
}

void displayData() {
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "TMP");
  display.drawString(0, 12, "RH");
  display.drawString(0, 24, "CO2");
  display.drawString(0, 36, "PM2.5");
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  if(inF){
    float tempValue = (float(Temperature) * 1.8) + 32; 
    display.drawString(64, 0, String(tempValue, 1) + " °F");
  }
  else{
    display.drawString(64, 0, String(Temperature, 1) + " °C");
  }
  display.drawString(64, 12, String(Humidity, 1) + " %");
  display.drawString(64, 24, CO2);
  display.drawString(64, 36, String(PM25));
  display.display();
}

void updateScreen(long now) {
  if ((now - lastUpdate) > updateFrequency) {
    // Take a measurement at a fixed interval.
    getData();
    //displayData();
    lastUpdate = millis();
  }
}
