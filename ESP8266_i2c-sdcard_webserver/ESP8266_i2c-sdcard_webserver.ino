#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
uint32_t i2c_bus_Clock = 100000; // default is 100000 we will go with that for stability. 
uint32_t i2c_bus_FileDownload = 400000; // used for browser download only, but can be set lower if there are I2C issues or noise
/*
i2c_Standard_Mode = 100000; // sd-card to browser about 3.5k/sec
i2c_Fast_Mode = 400000; // sd-card to browser about 9k/sec
i2c_Fast_Mode_Plus = 1000000;  // sd-card to browser about 12.5k/sec, might be unstable if incorrect I2C pull-up resistors are used.
i2c_High_speed_mode_Hs = 1700000; // sd-card to browser about 15k/sec, might be unstable if incorrect I2C pull-up resistors are used.
*/ 

// Replace with your network credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Create a web server object that listens on port 80
ESP8266WebServer server(80);
#include "SDCardFunc.h"

void handleRoot() {
  server.send(200, "text/html", "<h1>Hello, world!</h1><a href=\"./listSDCard\">List SD Card</a>");
}

void handleWebRequests() {
  if (SDCARDBUSY) return; // Check if the SD-Card is busy, if it is it will just return blank page
  if (loadFromI2CSD(server.uri())) { // if this fails, the below 404 page will be displayed
    return;
  }
  // Pre-allocate buffer for error message
  String msg;
  msg.reserve(512);

  static const char ERROR_HEAD[] PROGMEM =
    "File or Page Not Found\n\n<br>URI: ";
  static const char ERROR_METHOD[] PROGMEM =
    "\n<br>Method: ";
  static const char ERROR_ARGS[] PROGMEM =
    "\n<br>Arguments found: ";
  static const char ERROR_ARG_PREFIX[] PROGMEM =
    "\n<br>Argument ";
  static const char ERROR_ARG_NAME[] PROGMEM =
    " - NAME:";
  static const char ERROR_ARG_VALUE[] PROGMEM =
    "\n VALUE:";
  static const char ERROR_TAIL[] PROGMEM =
    "\n<br>404 - File Not Found\n\n";

  msg += FPSTR(ERROR_HEAD);
  msg += server.uri();
  msg += FPSTR(ERROR_METHOD);
  msg += (server.method() == HTTP_GET) ? F("GET") : F("POST");
  msg += FPSTR(ERROR_ARGS);
  msg += server.args();

  const uint8_t argCount = server.args();
  for (uint8_t i = 0; i < argCount; i++) {
    msg += FPSTR(ERROR_ARG_PREFIX);
    msg += (i + 1);
    msg += FPSTR(ERROR_ARG_NAME);
    msg += server.argName(i);
    msg += FPSTR(ERROR_ARG_VALUE);
    msg += server.arg(i);
    msg += F("\n<br>");
  }
  msg += FPSTR(ERROR_TAIL);
  server.send(404, F("text/html"), msg);
}

void setup() {
  // Start serial communication for debugging
  delay(6000); // 6 second delay on start up to establish serial connection
  Serial.begin(115200);
  Serial.println("--- Start Up ---");
  // Connect to WiFi
  WiFi.begin(ssid, password);
  uint8_t counter = 0;
  uint8_t progress = (counter * 100) / 20;
  Serial.print(F("Attempting to connect to WiFi"));
  Serial.print(F("\r\nConnection progress: "));
  char cntprog[4] = ("");
  while (counter < 20) {
    if (WiFi.status() == WL_CONNECTED) { 
      Serial.print("\r\nConnected! IP address: ");
      Serial.println(WiFi.localIP());
      break; 
    }
    CustDelay(500);
    counter++;
    progress = (counter * 100) / 20;
    Serial.print(String(progress));
    Serial.print(F("%  "));
    snprintf(cntprog, sizeof(cntprog), "%i", progress);
  } 
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\r\nWiFi connection failed! So sad, :(");
  }

  // Start I2C
  Wire.begin();
  Wire.setClock(i2c_bus_Clock);

  // Check for I2C Card
  Wire.beginTransmission(I2C_SDCARD);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Detected_i2cSDCard = true;
      Serial.println("Found I2C SD-Card at address: " + String(I2C_SDCARD));
      queryCardType();
      getvolsize();
      RunSDCard_Demo(); // Runs though most of the functions available
      }


  

  // Define routes
  server.onNotFound(handleWebRequests);  // If no route found, let's check the SD-Card for file per URI
  
  server.on("/deleteFile", HTTP_POST, handleDeleteFile);

  server.on("/", handleRoot);

  server.on("/listSDCard", []() {
      String argDIR = "/";
      if (server.arg("DIR") == "") {
        argDIR = "/";
      } else {
        argDIR = server.arg("DIR");
      }
      int page = 1;
      int perPage = 20;
      if (server.hasArg("page")) {
        page = server.arg("page").toInt();
        if (page < 1) page = 1;
      }
      if (server.hasArg("perPage")) {
        perPage = server.arg("perPage").toInt();
        if (perPage < 1) perPage = 20;
      }
      server.send(200, "text/html", listDirectory_HTML(argDIR.c_str(), page, perPage));
    });

    


  
   // Start the web server if WiFi connected
   if (WiFi.status() == WL_CONNECTED) {
    server.begin();
    Serial.println("HTTP server started");
  } else {
    Serial.println("HTTP server NOT started, no WiFi");
  }
   
}

void loop() {
  server.handleClient();
  // put your main code here, to run repeatedly:

}
