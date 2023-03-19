// ---------------------------------------------------------------- 
//                                                                  
// MTConnect Adapter for ESP32
//
// (c) Rolf Wuthrich, 
//     2022-2023 Concordia University
//
// author:  Rolf Wuthrich
// email:   rolf.wuthrich@concordia.ca
//
// This software is copyright under the BSD license
//
// --------------------------------------------------------------- 

// Demonstrates how to setup an MTConnect Adapter which
// reads the temperature from a DS18B20 Temperature Sensor
//
// The adapter sends SHDR format to an MTConnect Agent which connected
// to this adapter
//
// The adapter assumes the following configuration 
// in the MTConnect device model:
//
//   <DataItem category="SAMPLE" id="Temp" type="TEMPERATURE" units="CELCIUS"/>
//
//
// Required libraries :
// (available via Sketch > Include Library > Manage Libraries)
//
// - OneWire library by Paul Stoffregen
//   https://github.com/PaulStoffregen/OneWire
//   https://www.pjrc.com/teensy/td_libs_OneWire.html
//
// - DallasTemperature library by Miles Burton
//   https://github.com/milesburton/Arduino-Temperature-Control-Library
//
// - HX711 Arduino Library by Bogdan Necula
//   https://github.com/bogde/HX711
//   https://reference.arduino.cc/reference/en/libraries/hx711-arduino-library/


#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <OneWire.h>
#include <DallasTemperature.h>
#include "HX711.h"
#include "secrets.h"


// -----------------------------------------------------
// Configuration for WiFi access
const char *ssid     = SECRET_SSID;     // WIFI ssid
const char *password = SECRET_PASS;     // WIFI password


// -----------------------------------------------------
// Configuration for MTConnect Adapter

// Hostname
String ADAPTER_HOSTNAME = "MTConnectAdapter";

// Port number
const uint16_t port = 7878;

// PONG (answer to '*PING' request from the MTConnect Agent)
#define PONG "* PONG 60000"


// -----------------------------------------------------
// Configuration for DS18B20 sensor

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;     // D4 [GPIO4]

// Setup a oneWire instance
OneWire oneWire(oneWireBus);

// Pass oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Global temperature variable
float tempOld;


// -----------------------------------------------------
// Configuration for HX711

HX711 hx711;

// GPIOs where the HX711 is connected to
const int LOADCELL_DOUT_PIN = 15;     // D1 5 [GPIO15]
const int LOADCELL_SCK_PIN  = 2;      // D2   [GPIO2]

// Global scale variable
float scaleOld;


// ---------------------------------------------------
// WiFi Event callback functions

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to WiFi Acces Point");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect...");
  WiFi.begin(ssid, password);
}


// ------------------------------------------------
// Global variables

WiFiServer server(port, 1);   // max_clients = 1
WiFiClient client;
bool connected = false;


// -----------------------------------------------------------
// Adapter functions

void sendTempSHDR(float temp)
{
  Serial.print("|Temp|");
  Serial.println(temp);
  String shdr = "|Temp|" + String(temp) + "\n";
  client.println(shdr);
}

void sendScaleSHDR(float scale)
{
  Serial.print("|Mass|");
  Serial.println(scale);
  String shdr = "|Mass|" + String(scale) + "\n";
  client.println(shdr);
}


void setup() {
  
  // Start the Serial Monitor
  Serial.begin(115200);

  // WiFi events
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // Configure hostename
  WiFi.setHostname(ADAPTER_HOSTNAME.c_str());

  // Conencting to WiFi
  Serial.println("Connecting to WiFi ...");
  WiFi.begin(ssid, password);

  // Start TCP server
  server.begin();
  Serial.print("\nStarting MTConnect Adapter on port ");
  Serial.println(port);
  Serial.println();
  Serial.println("Waiting for connection from MTConnect agent");
  
  // Start the DS18B20 sensor
  sensors.begin();
  tempOld = -99999.0;

  // Start HX711
  hx711.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  hx711.set_scale(-471.497);             // this value is obtained by calibrating the scale with known weights
  hx711.tare();
  scaleOld = -99999.0;
}

void loop() {

  if (!connected) {
    client = server.available();
    if (client) {
      if (client.connected()) {
        Serial.print("Connection recieved from ");
        Serial.println(client.remoteIP());
        connected = true;
      } else {    
        // the connection was not a TCP connection  
        client.stop();  // close the connection:
      }
    }
  } 
  else {
    if (client.connected()) {
      
      // collect sensor data
      sensors.requestTemperatures(); 
      float temp = sensors.getTempCByIndex(0);
      float scale = hx711.get_units(20);
      
      // Check if * PING request came
      String currentLine = "";
      while (client.available()) { 
        char c = client.read(); 
        if (c == '\n') {
          if (currentLine.startsWith("* PING")) {
            client.println(PONG);
          }
          Serial.println(currentLine);
          currentLine = "";
        }
        currentLine += c;
      }

      // sends SHDR data
      if (temp!=tempOld) {
        sendTempSHDR(temp);
        }
      tempOld = temp;

      if (scale!=scaleOld) {
        sendScaleSHDR(scale);
        }
      scaleOld = scale;

      
    }
    else {
      Serial.println("Client has disconnected ");
      client.stop();
      connected = false;
    }
  }

}
