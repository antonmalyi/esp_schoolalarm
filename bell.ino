#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>               // Include NTPClient library
#include <Wire.h>
#include <SPI.h>  // not used here, but needed to prevent a RTClib compile error
#include <RTClib.h>
//#include <EEPROM.h>

#define BELLSC 16
#define RESP_BUFFER_LENGTH 128

RTC_DS3231 RTC;

const char *ssid     = "*";
const char *password = "*";
const char *host = "";
const int port = 80;

byte bells[BELLSC][2] = {{8, 30}, {9, 15}, {9, 25}, {10, 10},
  {10, 20}, {11, 05}, {11, 15}, {12, 00},
  {12, 30}, {13, 15}, {13, 25}, {14, 10},
  {14, 20}, {15, 05}, {0, 0}, {0, 0}
};


WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "time.nist.gov", 3600, 60000);
byte second_;

void setup() {
  pinMode(14, OUTPUT);
  digitalWrite(14, LOW);
  Serial.begin(115200);
  Wire.begin();
  RTC.begin();
  delay(3000);
  Serial.print("Current time: ");
  printTime(RTC.now());
  if (! RTC.isrunning()) {
    updateTime();
  }
  else {
    DateTime now = RTC.now();
    if (now.year() < 2020 ||now.year() > 2022) {
      updateTime();
    }
  }
  updateSchedule();
}

void loop() {
  DateTime now = RTC.now();
  
  printTime(RTC.now());
  if(now.hour()==6 && now.minute()==0) {
    updateTime(); 
    delay(60000);
    return;
  }
  if(now.minute()==42) {
    updateSchedule(); 
    delay(60000);
    return;
  }
  bool timeToRing = false;
  for(byte i = 0; i< BELLSC; i++) {
    if(
      now.hour() == bells[i][0] && 
      now.minute() == bells[i][1] && 
      (bells[i][0] + bells[i][1] != 0)
      ) {
        timeToRing = true;
        break; 
      }
  }
  if(
    timeToRing &&
    now.dayOfWeek() != 0 &&
    now.dayOfWeek() != 7
//    now.dayOfWeek() != 6
    ) {
      digitalWrite(14, HIGH);
      delay(20000);
      digitalWrite(14, LOW);
      delay(50000);
  }
  else {
    delay(10000);
  }

}

void parseSchedule(String s) {
  Serial.println("Parse schedule");
  byte bells_[BELLSC][2], k = 0, dig;
  int i, j = 0;
  const int L = s.length();
  for (byte i = 0; i < BELLSC; i++) {
    bells_[i][0] = 0;
    bells_[i][1] = 0;
  }

  for (i = 0; i < L; i++) {
    if (s.charAt(i) == ',') {
      j++;
      k = 0;
      continue;
    }
    if (s.charAt(i) == ':') {
      k = 1;
      continue;
    }
    dig = s.charAt(i) - '0';
    if (dig >= 0 && dig < 10) {
      bells_[j][k] = 10 * bells_[j][k] + dig;
    }
  }
  if (j > 5) { // AT least 3 classes added
    Serial.println("Adding parsed data");
    for (i = 0; i < BELLSC; i++ ) {
      bells[i][0] = bells_[i][0];
      bells[i][1] = bells_[i][1];
      Serial.print(bells[i][0]);
      Serial.print(':');
      Serial.println(bells[i][1]);
      
    }
  }
}

bool wifiSetup() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to network");
    WiFi.begin(ssid, password);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < 300) {
      delay(50);
      i++;
    }
  }
  Serial.print("Connected: ");
  Serial.println(WiFi.status() == WL_CONNECTED);
  return WiFi.status() == WL_CONNECTED;
}

void updateTime() {
  Serial.println("Update time");
  if (!wifiSetup()) {
    return;
  }
  timeClient.begin();
  timeClient.update();
  unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server
  unix_epoch += 7200;
  if (unix_epoch > 1603584000L && unix_epoch < 1616875200) {
    Serial.println("DST is off");
    unix_epoch += 3600;
  }
  RTC.adjust(DateTime(unix_epoch));
  Serial.print("Current time: ");
  printTime(RTC.now());
}
void updateSchedule() {
  Serial.println("Update schedule");
  if (!wifiSetup()) {
    return;
  }
  WiFiClient client;
  if (!client.connect(host, port)) {
    return;
  }

  client.print(String("GET /bells.txt HTTP/1.1\r\nHost: ") + host + "\r\nConnection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }


  uint8_t * _buffer = new uint8_t[RESP_BUFFER_LENGTH];
  String _responseString = "";
  while (client.available())
  {
    int actualLength = client.read(_buffer, RESP_BUFFER_LENGTH);
    if (actualLength <= 0)
    {
      return;
    }
    _responseString += String((char*)_buffer).substring(0, actualLength);
  }
  delete[] _buffer;
  client.stop();
  Serial.print("Data obtained: ");
  const int ln=_responseString.indexOf("\r\n\r\n");
  if(-1 == ln) {
    _responseString = "";
  }
  else {
    _responseString = _responseString.substring(ln+4);
  }
  Serial.println(_responseString);
  return parseSchedule(_responseString);

}

String printTime(DateTime now) {
   Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
}
