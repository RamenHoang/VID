#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <SocketIoClient.h>
#include <Arduino.h>
#include <stdio.h>
#include <EEPROM.h>

TinyGPSPlus gps;  // The TinyGPS++ object
SoftwareSerial ssGps(4, 5); // The serial connection to the GPS device
SoftwareSerial bt(2, 0); // The serial connection to HC-06

SocketIoClient socketClient; // The socketClient to use socketio

std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure); // Decalre to use SSL

const String host = "https://vid-service.herokuapp.com";
const char* ioHost = "vid-service.herokuapp.com";
const char* ssid = "AnhHoang";
const char* password = "09082018";

#define IMMOBILE 75
#define DB_THRESHOLD 235 // 75 * 3 = 27s

int immobile = 75;

float flagLat = 0.0 , flagLng = 0.0;

String hostId;

unsigned long TIMER;

double DISTANCE = 0.0;

int db = 0;
bool checkThief = false;

void setup()
{
  //  Define baudrate
  Serial.begin(115200);
  ssGps.begin(9600);
  bt.begin(9600);
  EEPROM.begin(512);

  //Connect wifi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }

  client->setInsecure();

  //  String _id = "";
  //
  //  while (bt.available()) {
  //    _id += bt.read();
  //  }
  //
  //  for (int i = 0; i < _id.length(); i++) {
  //    EEPROM.write(i, _id[i]);
  //    EEPROM.commit();
  //  }

  HTTPClient http;

  Serial.print("[HTTPS] begin...\n");
  if (http.begin(*client, host + "/get-info?username=Ramen")) {
    int httpCode = http.GET();
    Serial.print("[HTTPS] GET...\n");

    if (httpCode > 0) {
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("[PAYLOAD] ");
        Serial.println(payload);
        hostId = payload;
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
  }

  http.end();

  delay(500);

  if (http.begin(*client, host + "/post-module-id")) {
    String body = "{\"hostId\":\"";
    body += hostId;
    body += "\", \"moduleId\":\"";
    body += ESP.getChipId();
    body += "\"}";
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(body);
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("[PAYLOAD] ");
        Serial.println(payload);
      }
    }
  }
  http.end();
  delay(500);

  //Connect socketIo
  socketClient.begin(ioHost);

  //  Create timer
  TIMER = millis();
}

void loop()
{
  socketClient.loop();

  if (ssGps.available()) {
    Serial.println("ssGps");
    if (gps.encode(ssGps.read())) {
      if (gps.location.isValid() && gps.speed.isValid()) {
        Serial.println("Have data");
        double _lat, _lng, _speed;
        _lat = gps.location.lat();
        _lng = gps.location.lng();
        _speed = gps.speed.mps();

        if (flagLat == 0.0 && flagLng == 0.0) {
          flagLat = _lat;
          flagLng = _lng;
        }

        if ((int)_speed == 0) {
          db = 0;
          if (immobile > 0) {
            immobile--;
          } else {
            flagLat = _lat;
            flagLng = _lng;
            checkThief = true;
          }
        } else {
          db++;
          if (db == DB_THRESHOLD) {
            immobile = IMMOBILE;
            db = 0;
            checkThief = false;
          }
        }
        
        if (checkThief) {
          DISTANCE = TinyGPSPlus::distanceBetween(
                       flagLat,
                       flagLng,
                       _lat,
                       _lng
                     );
        }

        if (millis() - TIMER >= 3000) {
          String data = "{\"hostId\":\"";
          data += hostId;
          data += "\", \"gps\": [";
          data += String(_lat, 6);
          data += ",";
          data += String(_lng, 6);
          data += ",";
          data += (int) gps.speed.mps();
          data += "],";
          data += "\"distance\": ";
          data += (int) DISTANCE;
          data += ",\"checkThief\": ";
          data += checkThief;
          data += "}";

          Serial.println(data);
          socketClient.emit("Sent-data-to-server", string2char(data));
          
          // Reset
          TIMER = millis();
          DISTANCE = 0;
        }
      }
    }
  }
}

char* string2char(String command) {
  if (command.length() != 0) {
    char *p = const_cast<char*>(command.c_str());
    return p;
  }
}
