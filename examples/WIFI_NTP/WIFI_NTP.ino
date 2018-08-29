// example for WIFI based boards like ESP32 or MKR1000

#include "Arduino.h"
#include "WiFi.h"
#include "WiFiUdp.h"
#include "NTP.h"

char *ssid     = "yourSSID";
char *password = "yourPASSWORD";

WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting ...");
    delay(500);
    }
  Serial.println("Connected");  
  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)
  ntp.begin();
  Serial.println("start NTP");
  }

void loop() {
  ntp.update();
  Serial.println(ntp.formattedTime("%d. %B %Y")); // dd. Mmm yyyy
  Serial.println(ntp.formattedTime("%A %T")); // Www hh:mm:ss
  delay(1000);
  }
