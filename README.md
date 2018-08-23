# NTP
The **NTP** library allows you to receive time information from the Internet. It also have support for
different timezones and daylight saving time (DST).
This NTP library uses the functions of the time.h standard library.

## Example
Example for WIFI boards like ESP32 or MKR1000, prints formatted time and date strings to console.

```cpp
#include "Arduino.h"
#include "WiFi.h"
#include "WiFiUdp.h"
#include "NTP.h"

const char *ssid     = "yourSSID"; // your network SSID
const char *password = "yourPASSWORD"; // your network PW

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
```

# Documentation

## NTP / ~NTP
onstructor / destructor for a NTP object

## void begin()
starts the underlaying UDP client

## void stop()
stops the underlaying UDP client

## void update()
this must called in the main loop

## void ntpServer(const char* server)
set an other NTP server, default NTP server is "pool.ntp.org"

## void updateInterval(uint32\_t interval)
sets the update interval for connecting the NTP server in ms, default is 60000ms (60s)

## void ruleDST(const char* tzName, int8\_t week, int8\_t wday, int8\_t month, int8\_t hour, int tzOffset)
sets the rules for the daylight save time settings

- tzname is the name of the timezone, e.g. "CEST" (central europe summer time)
- week Last, First, Second, Third, Fourth (0 - 4)
- wday Sun, Mon, Tue, Wed, Thu, Fri, Sat (0 - 7)
- month Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec (0 -11)
- hour the local hour when rule changes
- tzOffset timezone offset in minutes

## char* ruleDST()
gives the DST time back, formatted as an ctime string

## void ruleSTD(const char* tzName, int8\_t week, int8\_t wday, int8\_t month, int8\_t hour, int tzOffset)
sets the rules for the standard time settings

- tzname is the name of the timezone, e.g. "CET" (central europe time)
- week Last, First, Second, Third, Fourth (0 - 4)
- wday Sun, Mon, Tue, Wed, Thu, Fri, Sat (0 - 7)
- month Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec (0 -11)
- hour the local hour when rule changes
- tzOffset timezone offset in minutes

## char* ruleSTD()
gives the STD time back, formatted as an ctime string

## char* tzName()
gives you the name of the current timezone, based on your rule settings

## void timeZone(int8\_t tzHours, int8\_t tzMinutes = 0)
only use this function when you don't made the rules setting,
you have to the set isDST(false)

## void isDST(bool dstZone)
use in conjunction with timeZone, when there is no DST!

## bool isDST()
gives the DST status back, true if summertime

## time_t epoch()
get the Unix epoch timestamp

## int16\_t year(), int8\_t month(), int8\_t day(), int8\_t weekDay(), int8\_t hours(), int8\_t minutes(), int8\_t seconds()
get the datas from the tm structure of the "time.h" library

## char* formattedTime(const char *format)
gives back a string, formated with strftime function of standard time library
```
| symbol | explanation
/* General */
| % | writes literal %. The full conversion specification must be %%.
| n | writes newline character
| t | writes horizontal tab character
/* Year */
| Y | writes year as a decimal number, e.g. 2017
| y | writes last 2 digits of year as a decimal number (range [00,99])
| C | writes first 2 digits of year as a decimal number (range [00,99])
| G | writes ISO 8601 week-based year, i.e. the year that contains the specified week. 
	  In IS0 8601 weeks begin with Monday and the first week of the year must satisfy the following requirements:
	  - Includes January 4 
	  - Includes first Thursday of the year
| g | writes last 2 digits of ISO 8601 week-based year, i.e. the year that contains the specified week (range [00,99]).
	  In IS0 8601 weeks begin with Monday and the first week of the year must satisfy the following requirements:
	  - Includes January 4
	  - Includes first Thursday of the year
/* Month */
| b | writes abbreviated month name, e.g. Oct (locale dependent)
| h | synonym of b
| B | writes full month name, e.g. October (locale dependent)
| m | writes month as a decimal number (range [01,12])
/* Week */
| U | writes week of the year as a decimal number (Sunday is the first day of the week) (range [00,53])
| W | writes week of the year as a decimal number (Monday is the first day of the week) (range [00,53])
| V | writes ISO 8601 week of the year (range [01,53]).
	  In IS0 8601 weeks begin with Monday and the first week of the year must satisfy the following requirements:
	  - Includes January 4
	  - Includes first Thursday of the year
/* Day of the year/month */
| j | writes day of the year as a decimal number (range [001,366])
| d | writes day of the month as a decimal number (range [01,31])
| e | writes day of the month as a decimal number (range [1,31]).
	  Single digit is preceded by a space.
/* Day of the week */
| a | writes abbreviated weekday name, e.g. Fri (locale dependent)
| A | writes full weekday name, e.g. Friday (locale dependent)
| w | writes weekday as a decimal number, where Sunday is 0 (range [0-6])
| u | writes weekday as a decimal number, where Monday is 1 (ISO 8601 format) (range [1-7])
/* Hour, minute, second */
| H | writes hour as a decimal number, 24 hour clock (range [00-23])
| I | writes hour as a decimal number, 12 hour clock (range [01,12])
| M | writes minute as a decimal number (range [00,59])
| S | writes second as a decimal number (range [00,60])
/* Other */
| c | writes standard date and time string, e.g. Sun Oct 17 04:41:13 2010 (locale dependent)	
| x | writes localized date representation (locale dependent)
| X | writes localized time representation (locale dependent)
| D | equivalent to "%m/%d/%y"
| F | equivalent to "%Y-%m-%d" (the ISO 8601 date format)
| r | writes localized 12-hour clock time (locale dependent)
| R | equivalent to "%H:%M"
| T | equivalent to "%H:%M:%S" (the ISO 8601 time format)
| p | writes localized a.m. or p.m. (locale dependent)
```

## void offset(int16\_t days, int8\_t hours, int8\_t minutes, int8\_t seconds)
you can give a manually time offset for e.g. debug purposes




