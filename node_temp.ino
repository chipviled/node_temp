/**
 * DHT21 with http server and OLED 0.96
 * v 0.9
 */

#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <time.h>
#include <Wire.h>
#include <SSD1306.h>       // include <SSD1306Wire.h>
#include <RBD_Timer.h>
#include <RBD_Button.h>

// Uncommint for debug
// #define DEBUG

#define DEBUG_PRINTER Serial
#ifdef DEBUG
  #define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
  #define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
  #define DEBUG_PRINT(...) {}
  #define DEBUG_PRINTLN(...) {}
#endif


#define DHTTYPE DHT21
#define DHTCOUNT 3

#define TIME_MSG_LEN  11   // time sync to PC is HEADER and unix time_t as ten ascii digits
#define TIME_HEADER  255   // Header tag for serial time sync message

#define D0   16
#define D1   5    // oled
#define D2   4    // oled
#define D3   0    // dht
#define D4   2 
#define D5   14   // dht
#define D6   12   // dht
#define D7   13   // button
#define D8   15   // button
#define D9   3    // - RX0 (Serial console)
#define D10  1    // - TX0 (Serial console)

const char* ssid = "SSID";
const char* password = "PASSWORD32";
const char* token = "TOKEN32";

//IPAddress ip(192, 168, 11, 11);        //static IP
IPAddress ip(192, 168, 11, 12);
IPAddress gateway(192, 168, 11, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

DHT dht1(D3, DHTTYPE);
DHT dht2(D5, DHTTYPE);
DHT dht3(D6, DHTTYPE);

SSD1306 display(0x3c, D1, D2);
// SH1106 display(0x3c, D1, D2);

RBD::Timer timer1;
//RBD::Timer timer2;

RBD::Button button1(D7);
RBD::Button button2(D8);


float cacheTemp [DHTCOUNT] = {};
float cacheHumi [DHTCOUNT] = {};
float cacheHeat [DHTCOUNT] = {};
time_t timeForCache [DHTCOUNT] = {};
time_t timeoutCacheSec = 15;
time_t timeoutMaxCacheSec = 300; // 5 * 60

int displayStatus = 0;
bool initializedWifi = false;
byte flicker = 0;


void setup() {
  
  Serial.begin(9600);
  Serial.setTimeout(2000);
  // Wait for serial to initialize.
  while (!Serial) { delay(50); }
  
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  Serial.println("");
  Serial.println("Connected to WiFi ...");
  
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);

  timer1.setTimeout(1000);
  timer1.restart();
//  timer2.setTimeout(100);
//  timer2.restart();

  button1.setDebounceTimeout(50);
  button2.setDebounceTimeout(50);

  dht1.begin(60);
  dht2.begin(70);
  dht3.begin(80);
}


void configureWifi() {
  // Call one per second.
  
  // If all good.
  if( WiFi.status() == WL_CONNECTED and initializedWifi == true) {
    display.drawString(0, 0, "wifi ok");
    return;
  }

  // If connected.
  if( WiFi.status() == WL_CONNECTED and initializedWifi != true) {
    initializedWifi = true;
    
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    server.on("/", handleRoot);
    server.begin();
    
    Serial.println("HTTP server started");
    return;
  }

  // If lost connect.
  if( WiFi.status() != WL_CONNECTED and initializedWifi == true) {
    initializedWifi = false;
    // Wait in loop.
    return;
  }
  
  // Wait connect.
  if( WiFi.status() != WL_CONNECTED and initializedWifi != true) {
    Serial.print(".");
    //display.drawString(0, 0, "wifi");
    if (flicker % 2 == 0){
      display.drawString(0, 0, "wifi lost");
    }
    return;
  }

  // All else (never been).
  return;
}


void getDhtData(DHT &dht, int i) {
  time_t nowTime = time(0);
  
  if(timeForCache[i] + timeoutMaxCacheSec < nowTime) {
    cacheTemp[i] = cacheHumi[i] = cacheHeat[i] = nanf("");
    timeForCache[i] = 0;
  }  
  
  if(timeForCache[i] + timeoutCacheSec < nowTime) {
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    cacheTemp[i] = dht.readTemperature();
    cacheHumi[i] = dht.readHumidity();
    // Compute heat index in Celsius (isFahreheit = false)
    cacheHeat[i] = dht.computeHeatIndex(cacheTemp[i], cacheHumi[i], false);
    timeForCache[i] = nowTime;
    if (isnan(cacheTemp[i]) || isnan(cacheHumi[i])) {
      timeForCache[i] = 0;
    }
    DEBUG_PRINTLN("DEB: Update now = " + String(nowTime));
  } else {
    DEBUG_PRINTLN("DEB: NOT update (cache) now = " + String(nowTime));
  }
  DEBUG_PRINTLN("DEB: " + String(i) + "  " + String(cacheTemp[i]) + "  " + String(cacheHumi[i]));
}


String getTempData(int id) {
  float h;
  float t;
  float hic;
  
  if (id > 0 && id <= DHTCOUNT) {
    t = cacheTemp[id - 1];
    h = cacheHumi[id - 1];
    hic = cacheHeat[id - 1];
  } else {
    Serial.println("Incorrect id:" + String(id));
    return "{\"error\":true,\"message\":\"Failed to select DHT sensor!\"}";
  }

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return "{\"error\":true,\"message\":\"Failed to read from DHT sensor!\"}";
  }
    
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.println(" *C ");
  
  return "{\"id\":" + String(id) + ",\"h\":" + String(h, 2) + ",\"t\":" 
                + String(t, 2) + ",\"hic\":" + String(hic, 2) + "}";
  
}


void handleRoot() {
  Serial.println("Get request");  
  
  if (server.hasArg("token") == false or server.arg("token") != token){
    server.send(200, "application/json", "{\"error\":true,\"message\":\"You forget some params\"}");
    return;
  }
  
  if (server.hasArg("id") == false){
    server.send(200, "application/json", "{\"error\":true,\"message\":\"You forget set id\"}");
    return;
  }

  if (server.arg("id") != "1" and server.arg("id") != "2" and server.arg("id") != "3"){
    server.send(200, "application/json", "{\"error\":true,\"message\":\"Incorrect id\"}");
    return;
  }

  Serial.println("Get data for response");
  String data = getTempData(server.arg("id").toInt());
  server.send(200, "application/json", data); 
}

String displayTemperature(float t) {
  String res = "";
  if (isnan(t)) {return "err";}
  if (t > 99.9) {t = 99.9;}
  if (t < -99.9) {t = -99.9;}
  if (t < 0) {
    res = '-';
  } else {
    res = '+';
  }
  if (abs(t) < 10) {
    res += "  ";
  }
  return res + String(abs(t)) + "*C";
}


String displayHumidity(float h) {
  String res = "";
  if (isnan(h)) {return "err";}
  if (h > 99.9) {h = 99.9;}
  if (h < 0) {h = 0;}
  if (abs(h) < 10) {
    res = "  ";
  }
  return res + String(abs(h)) + " %";
}


void updateDisplay() {
  if (flicker % 2 == 0){
    display.drawString(88, 0, "00:00");
  } else {
    display.drawString(88, 0, "00 00");
  }
  
  display.drawString(0, 16, "d1:");
  display.drawString(32, 16, displayTemperature(cacheTemp[0]));
  display.drawString(88, 16, displayHumidity(cacheHumi[0]));

  display.drawString(0, 32, "d2:");
  display.drawString(32, 32, displayTemperature(cacheTemp[1]));
  display.drawString(88, 32, displayHumidity(cacheHumi[1]));

  display.drawString(0, 48, "d3:");
  display.drawString(32, 48, displayTemperature(cacheTemp[2]));
  display.drawString(88, 48, displayHumidity(cacheHumi[2]));
  
}


void loop() {
  if (initializedWifi) {
    server.handleClient();
  }
    
  if (timer1.onRestart()) {
    getDhtData(dht1, 0);
    getDhtData(dht2, 1);
    getDhtData(dht3, 2);
    
    display.clear();
    
    configureWifi();
    updateDisplay();
    
    display.display();

    if (flicker >= 255){
      flicker = 0;
    } else {
      flicker++; 
    }
  }

  if(button1.onPressed()) {
    Serial.println("B7");
    displayStatus++;
    if (displayStatus > 2) { displayStatus = 0; }
    if (displayStatus == 0) {
      display.displayOn();
      display.setContrast(100);
    } else if (displayStatus == 1) {
      display.displayOn();
      display.setContrast(20, 80);
    } else {
      display.setContrast(10, 5, 0);
      display.displayOff();
    }
  }
}
