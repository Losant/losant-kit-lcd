/**
   Workshop example for periodically sending temperature data and displaying
   on an LCD.

   Visit https://www.losant.com/kit for full instructions.

   Copyright (c) 2016 Losant IoT. All rights reserved.
   https://www.losant.com
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <Losant.h>


// WiFi credentials.
const char* WIFI_SSID = "my-wifi-ssid";
const char* WIFI_PASS = "my-wifi-pass";

// Losant credentials.
const char* LOSANT_DEVICE_ID = "my-device-id";
const char* LOSANT_ACCESS_KEY = "my-access-key";
const char* LOSANT_ACCESS_SECRET = "my-access-secret";

// Construct an LCD object and pass it the
// I2C address, width (in characters) and
// height (in characters). Depending on the
// Actual device, the IC2 address may change.
LiquidCrystal_I2C lcd(0x3F, 16, 2);

WiFiClientSecure wifiClient;

LosantDevice device(LOSANT_DEVICE_ID);

void setup() {
  Serial.begin(115200);

  // Giving it a little time because the serial monitor doesn't
  // immediately attach. Want the workshop that's running to
  // appear on each upload.
  delay(2000);

  Serial.println();
  Serial.println("Running LCD Kit Firmware.");

  // The begin call takes the width and height. This
  // Should match the number provided to the constructor.
  lcd.begin(16, 2);
  lcd.init();

  // Turn on the backlight.
  lcd.backlight();

  device.onCommand(&handleCommand);
  connect();
}

int timeSinceLastRead = 0;
int tempSum = 0;
int tempCount = 0;

void loop() {

  bool toReconnect = false;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected from WiFi");
    toReconnect = true;
  }

  if (!device.connected()) {
    Serial.println("Disconnected from MQTT");
    Serial.println(device.mqttClient.state());
    toReconnect = true;
  }

  if (toReconnect) {
    connect();
  }

  device.loop();

  tempSum += analogRead(A0);
  tempCount++;

  // Report every 15 seconds.
  if (timeSinceLastRead > 15000) {
    // Take the average reading over the last 15 seconds.
    double raw = (double)tempSum / (double)tempCount;

    // The tmp36 documentation requires the -0.5 offset, but during
    // testing while attached to the MCU, all tmp36 sensors
    // required a -0.52 offset for better accuracy.
    double degreesC = (((raw / 1024.0) * 3.2) - 0.5) * 100.0;
    double degreesF = degreesC * 1.8 + 32;

    reportTemp(degreesC, degreesF);

    timeSinceLastRead = 0;
    tempSum = 0;
    tempCount = 0;
  }

  delay(1000);
  timeSinceLastRead += 1000;
}

void handleCommand(LosantCommand *command) {
  Serial.print("Command received: ");
  Serial.println(command->name);
  JsonObject& payload = *command->payload;

  if (strcmp(command->name, "refresh") == 0) {
    payload.printTo(Serial);
    // Clear the screen.
    lcd.clear();

    // Move the cursor to the start of the first line and print the text.
    lcd.setCursor(0,0);
    lcd.print(payload["L1"].asString());

    // Move the cursor to the start of the second line and print the text.
    lcd.setCursor(0,1);
    lcd.print(payload["L2"].asString());
  }

}

/**
 * Connect to WiFi and Losant
 */
void connect() {

  // Connect to Wifi.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  while (WiFi.status() != WL_CONNECTED) {
    // Check to see if
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println("Failed to connect to WIFI. Please verify credentials: ");
      Serial.println();
      Serial.print("SSID: ");
      Serial.println(WIFI_SSID);
      Serial.print("Password: ");
      Serial.println(WIFI_PASS);
      Serial.println();
      Serial.println("Trying again...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      delay(10000);
    }

    delay(500);
    Serial.println("...");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.print("Authenticating Device...");
  HTTPClient http;
  http.begin("http://api.losant.com/auth/device");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  /* Create JSON payload to sent to Losant

       {
         "deviceId": "575ecf887ae143cd83dc4aa2",
         "key": "this_would_be_the_key",
         "secret": "this_would_be_the_secret"
       }

  */

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = LOSANT_DEVICE_ID;
  root["key"] = LOSANT_ACCESS_KEY;
  root["secret"] = LOSANT_ACCESS_SECRET;
  String buffer;
  root.printTo(buffer);

  int httpCode = http.POST(buffer);

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("This device is authorized!");
    } else {
      Serial.println("Failed to authorize device to Losant.");
      if (httpCode == 400) {
        Serial.println("Validation error: The device ID, access key, or access secret is not in the proper format.");
      } else if (httpCode == 401) {
        Serial.println("Invalid credentials to Losant: Please double-check the device ID, access key, and access secret.");
      } else {
        Serial.println("Unknown response from API");
      }
    }
  } else {
    Serial.println("Failed to connect to Losant API.");

  }

  http.end();

  // Connect to Losant.
  Serial.println();
  Serial.print("Connecting to Losant...");

  device.connectSecure(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET);

  while (!device.connected()) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected!");
  Serial.println();
  Serial.println("This device is now ready for use!");
}

/**
 * Report Temperature to Losant
 * @param degreesC double
 * @param degreesF double
 */
void reportTemp(double degreesC, double degreesF) {
  Serial.println();
  Serial.print("Temperature C: ");
  Serial.println(degreesC);
  Serial.print("Temperature F: ");
  Serial.println(degreesF);
  Serial.println();

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["tempC"] = degreesC;
  root["tempF"] = degreesF;
  device.sendState(root);

  lcd.setCursor(14,1);
  lcd.print(degreesF);
}
