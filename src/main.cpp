#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include "LittleFS.h"
#include <ESP_Mail_Client.h>
#include <ArduinoOTA.h>
#include <settings.h>
#include <sendmail.h>

const int button = D4;
const int led = LED_BUILTIN;
int buttonState = 0;
int pressedPrint = 0;
int releasedPrint = 1;
String textMsg = "";
unsigned long previousMillis = 0;

SMTPSession smtp;
void smtpCallback(SMTP_Status status);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "ip";
const char *PARAM_INPUT_4 = "gateway";

// Variables to save values from HTML form
String ssid;
String pass;
String ip;
String gateway;

// File paths to save input values permanently
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *ipPath = "/ip.txt";
const char *gatewayPath = "/gateway.txt";

IPAddress localIP;
// IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
// IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);

// Timer variables
unsigned long previousMail = 0;
const long interval = 10000; // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledPin = 2;
// Stores LED state

String ledState;

boolean restart = false;

// Initialize LittleFS
void initFS()
{
  if (!LittleFS.begin())
  {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else
  {
    Serial.println("LittleFS mounted successfully");
  }
}

// Read File from LittleFS
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path, "r");
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

// Write file to LittleFS
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- frite failed");
  }
  file.close();
}

// Initialize WiFi
bool initWiFi()
{
  if (ssid == "" || ip == "")
  {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if (!WiFi.config(localIP, localGateway, subnet))
  {
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.hostname(newHostname.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.println("Connecting to WiFi...");
  delay(20000);
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect.");
    return false;
  }
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

// Replaces placeholder with LED state value
String processor(const String &var)
{
  if (var == "STATE")
  {
    if (!digitalRead(ledPin))
    {
      ledState = "ON";
    }
    else
    {
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}

void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);
  ArduinoOTA.setHostname("espBoilerMon");
  ArduinoOTA.onStart([]()
                     { Serial.println("Start OTA"); });

  ArduinoOTA.onEnd([]()
                   { Serial.println("End OTA"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\n", (progress / (total / 100))); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

  ArduinoOTA.begin();
  initFS();

  pinMode(led, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  MailClient.networkReconnect(true);

  // Load values saved in LittleFS
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  ip = readFile(LittleFS, ipPath);
  gateway = readFile(LittleFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  if (initWiFi())
  {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html", false, processor); });

    server.serveStatic("/", LittleFS, "/");

    // Route to set GPIO state to HIGH
    server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      digitalWrite(ledPin, LOW);
      request->send(LittleFS, "/index.html", "text/html", false, processor); });

    // Route to set GPIO state to LOW
    server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      digitalWrite(ledPin, HIGH);
      request->send(LittleFS, "/index.html", "text/html", false, processor); });
    server.begin();
  }
  else
  {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/wifimanager.html", "text/html"); });

    server.serveStatic("/", LittleFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(LittleFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFile(LittleFS, gatewayPath, gateway.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      restart = true;
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip); });
    server.begin();
  }
}

void loop()
{
  if (restart)
  {
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.handle();

  unsigned long currentMail = millis();
  buttonState = digitalRead(button);
  if (buttonState == LOW)
  { // Light is on, set timer to 5 seconds or something before sending alarm..?
    if (pressedPrint == 0)
    {
      digitalWrite(led, HIGH);
      Serial.println("Button pressed!\n");
      pressedPrint++;
      releasedPrint = 0;
      SendEmail("Button pressed! - Sent from ESP board", "BUTTON PRESSED!");
    }
  }
  else
  {
    if (releasedPrint == 0)
    {
      digitalWrite(led, LOW); // Button released
      Serial.println("Button released!\n");
      releasedPrint++;
      pressedPrint = 0;
      SendEmail("Button released! - Sent from ESP board", "BUTTON RELEASED!");
    }
  }
}