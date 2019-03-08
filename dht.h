/**
 * DHT21 with http server and OLED 0.96
 * v 0.6
 */

#include "DHT.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266WebServer.h"
#include <time.h>
#include <Wire.h>    // библиотека Wire для коммуникации по I2C. нужна только для Arduino 1.6.5 и старее
#include "SSD1306.h" // включает в себя также «SSD1306Wire.h»`

#define DHTTYPE DHT21
#define DHTCOUNT 3

#define DHTPIN_1 0     // D3
#define DHTPIN_2 14    // D5
#define DHTPIN_3 12    // D6

#define TIME_MSG_LEN  11   // time sync to PC is HEADER and unix time_t as ten ascii digits
#define TIME_HEADER  255   // Header tag for serial time sync message

const char* ssid = "SSID";
const char* password = "PASSWORD32";
const char* token = "TOKEN32";

//IPAddress ip(192, 168, 11, 11);        //статический IP
IPAddress ip(192, 168, 11, 12);
IPAddress gateway(192, 168, 11, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

DHT dht1(DHTPIN_1, DHTTYPE);
DHT dht2(DHTPIN_2, DHTTYPE);
DHT dht3(DHTPIN_3, DHTTYPE);

SSD1306 display(0x3c, D1, D2);
// SH1106 display(0x3c, D3, D5);

float cacheTemp [DHTCOUNT] = {};
float cacheHumi [DHTCOUNT] = {};
float cacheHeat [DHTCOUNT] = {};
time_t timeForCache [DHTCOUNT] = {};
time_t timeoutCacheSec = 15;
time_t timeoutMaxCacheSec = 5 * 60;


void setup() {
  int tempRel = 0;
  
  Serial.begin(9600);
  Serial.setTimeout(2000);
  // Wait for serial to initialize.
  while(!Serial) { delay(50); }
  
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  Serial.println("");
  Serial.println("Connected to WiFi ...");
  
  WiFi.begin(ssid, password);
  WiFi.config(ip, gateway, subnet);
  
  // Whait
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    display.clear();
    display.drawString(0, 0, "Wait WiFi ...");
    if (tempRel > 1){
      tempRel = 0;
      display.drawString(112, 0, "*");
    }
    tempRel++;
    display.display();
    delay(1000);
  }
  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
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
    //Serial.println("DEB: Update now = " + String(nowTime));
  } else {
    //Serial.println("DEB: NOT update (cache) now = " + String(nowTime));
  }
  //Serial.println("DEB: " + String(timeForCache[i]) + " " + String(cacheHumi[i]));
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


void loop() {
  server.handleClient();

  getDhtData(dht1, 0);
  getDhtData(dht2, 1);
  getDhtData(dht3, 2);
  
  display.clear();
  //display.drawString(0, 0, "-- : --");
  
  display.drawString(0, 16, "d1:");
  display.drawString(32, 16, displayTemperature(cacheTemp[0]));
  display.drawString(88, 16, displayHumidity(cacheHumi[0]));

  display.drawString(0, 32, "d2:");
  display.drawString(32, 32, displayTemperature(cacheTemp[1]));
  display.drawString(88, 32, displayHumidity(cacheHumi[1]));

  display.drawString(0, 48, "d3:");
  display.drawString(32, 48, displayTemperature(cacheTemp[2]));
  display.drawString(88, 48, displayHumidity(cacheHumi[2]));
  
  display.display();
  
  delay(1000);
}
