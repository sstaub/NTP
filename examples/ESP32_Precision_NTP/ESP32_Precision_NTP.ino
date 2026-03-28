// ESP32 example demonstrating sub-millisecond precision and NTP validation
// This example shows:
// - Sub-millisecond time accuracy on ESP32
// - NTP response validation using isValid()
// - Automatic hardware RTC synchronization
// - Network delay measurement from NTP synchronization

#if !defined(ESP32) && !defined(ESP8266)
  #error "This example is designed for ESP32 or ESP8266 only. Please select an ESP32/ESP8266 board in Tools > Board."
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <sys/time.h>
#include "NTP.h"

char ssid[]     = "yourSSID";
char password[] = "yourPASSWORD";

WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

// Helper function to display ESP32 system RTC time
void displaySystemRTC() {
  struct timeval tv;
  struct tm timeinfo;
  
  // Get current time from ESP32 system RTC
  gettimeofday(&tv, NULL);
  time_t now = tv.tv_sec;
  localtime_r(&now, &timeinfo);
  
  Serial.print("ESP32 System RTC: ");
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d.%06ld UTC",
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec,
                tv.tv_usec);
  Serial.print(" | Epoch: ");
  Serial.println(now);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32 NTP Sub-Millisecond Precision Demo");
  Serial.println("=========================================\n");
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Show ESP32 RTC time BEFORE NTP sync
  Serial.println("\n--- ESP32 System RTC Before NTP Sync ---");
  displaySystemRTC();
  Serial.println("(This is likely wrong - showing epoch plus the time since boot)\n");
  
  // Configure timezone and DST rules (example: Central European Time)
  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // Last Sunday in March 2:00, timezone +120min (+1 GMT + 1h DST)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60);   // Last Sunday in October 3:00, timezone +60min (+1 GMT)
  
  // Start NTP with default server (pool.ntp.org)
  ntp.begin();
  ntp.updateInterval(10000); // 10 seconds, allows user to see fresh NTP round-trip delay calculations on each pass (do not do this in production)
  Serial.println("NTP client started");
  
  // Request NTP update
  Serial.println("\nPerforming initial NTP sync...");
  if (ntp.update()) {
    Serial.println("Initial sync successful!");
    
    // Show the measured network delay
    float delay = ntp.roundTripDelay();
    Serial.print("NTP Round-Trip Delay: ");
    Serial.print(delay, 3);
    Serial.println(" ms (measured at microsecond precision)");
    
    Serial.println("\n--- ESP32 System RTC After NTP Sync ---");
    displaySystemRTC();
    Serial.println("(The library automatically synchronized the ESP32 RTC!)");
  } else {
    Serial.println("Initial sync failed, waiting for next update...");
  }
  
  Serial.println("\n");
}

void loop() {
  // Update will automatically sync every 60 seconds by default
  bool timeAvailable = ntp.update();
  
  if (timeAvailable) {
    // Validate the NTP server's response
    if (!ntp.isValid()) {
      Serial.println("WARNING: NTP server reports invalid time");
      delay(10);
      return;
    }
    // Display full timestamp with millisecond precision
    Serial.print("Time: ");
    Serial.print(ntp.formattedTime("%Y-%m-%d %H:%M:%S"));
    Serial.print(".");
    
    // Show milliseconds with leading zeros
    uint16_t ms = ntp.milliseconds();
    if (ms < 100) Serial.print("0");
    if (ms < 10) Serial.print("0");
    Serial.print(ms);
    
    Serial.print(" ");
    Serial.print(ntp.tzName());
    
    // Show if DST is active
    if (ntp.isDST()) {
      Serial.print(" (DST)");
    }
    
    // Display Unix epoch for reference
    Serial.print(" | Epoch: ");
    Serial.print(ntp.epoch());
    
    // Show day of week
    Serial.print(" | ");
    Serial.println(ntp.formattedTime("%A"));
    
    // Every 10 seconds, show detailed timing information
    static unsigned long lastDetailedOutput = 0;
    if (millis() - lastDetailedOutput >= 10000) {
      lastDetailedOutput = millis();
      Serial.println("\n========================================");
      Serial.println("    Detailed Timing Information");
      Serial.println("========================================");
      
      // Show NTP library time
      Serial.println("\n[NTP Library Time]");
      Serial.print("Date: ");
      Serial.println(ntp.formattedTime("%B %d, %Y"));
      Serial.print("Time with millisecond precision: ");
      Serial.print(ntp.hours());
      Serial.print(":");
      if (ntp.minutes() < 10) Serial.print("0");
      Serial.print(ntp.minutes());
      Serial.print(":");
      if (ntp.seconds() < 10) Serial.print("0");
      Serial.print(ntp.seconds());
      Serial.print(".");
      if (ms < 100) Serial.print("0");
      if (ms < 10) Serial.print("0");
      Serial.println(ms);
      Serial.print("Unix Epoch: ");
      Serial.println(ntp.epoch());
      Serial.println("NTP Server Response: VALID (Leap Indicator and Stratum OK)");
      
      // Show ESP32 System RTC - proves the library synchronized it
      Serial.println("\n[ESP32 Hardware RTC - Auto-Synced by Library]");
      displaySystemRTC();
      
      // Show network delay information
      Serial.println("\n[Network Delay Measurement]");
      float ntpDelay = ntp.roundTripDelay();
      Serial.print("NTP Round-Trip Delay: ");
      Serial.print(ntpDelay, 3);
      Serial.println(" ms");
      
      Serial.println("\nOne-way delay is automatically compensated when calculating");
      Serial.println("the precise timestamp, ensuring sub-millisecond accuracy.");
      Serial.println("Lower values indicate better network conditions to the NTP server.");
      
      Serial.println("\nNote: The ESP32 system RTC is automatically synchronized");
      Serial.println("by the NTP library. Standard C time functions (time(),");
      Serial.println("localtime(), etc.) can be used within your code");
      Serial.println("========================================\n");
    }
  } else {
    Serial.println("Waiting for a valid NTP sync...");
  }
  
  delay(2000);
}
