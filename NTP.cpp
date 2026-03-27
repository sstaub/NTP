/**
 * NTP library for Arduino framework
 * The MIT License (MIT)
 * (c) 2026 sstaub
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NTP.h"

NTP::NTP(UDP& udp) {
	this->udp = &udp;
	}

NTP::~NTP() {
	stop();
	}

void NTP::begin(const char* server) {
	const char* selectedServer = server ? server : "pool.ntp.org";
	strncpy(this->server, selectedServer, sizeof(this->server) - 1);
	this->server[sizeof(this->server) - 1] = '\0';
	useServerIP = false;
	init(); 
	}

void NTP::begin(IPAddress serverIP) {
	this->serverIP = serverIP;
	useServerIP = true;
	init();
	}

void NTP::init() {
	memset(ntpRequest, 0, NTP_PACKET_SIZE);
  ntpRequest[0] = 0b00100011; // LI=0, Version=4, Mode=3 (client)
  ntpRequest[1] = 0;          // Stratum, or type of clock
  ntpRequest[2] = 6;          // Polling Interval
  ntpRequest[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  // Reference Identifier (bytes 12-15) left as zero for client requests
	udp->begin(NTP_PORT);
	bool syncOk = ntpUpdate();
	if (syncOk && dstZone && dstRuleConfigured && stdRuleConfigured) {
		currentTime();
		beginDST();
		}
	}

void NTP::stop() {
	udp->stop();
	}

bool NTP::update() {
	if (!everSynced) {
		bool syncSuccess = ntpUpdate();
		hasValidSync = syncSuccess;
		return syncSuccess;
		}
	if (millis() - lastUpdate >= interval) {
		bool syncSuccess = ntpUpdate();
		hasValidSync = syncSuccess;
		return syncSuccess;
		}
	return hasValidSync;
	}

bool NTP::ntpUpdate() {
	if (useServerIP) udp->beginPacket(serverIP, NTP_PORT);
	else udp->beginPacket(server, NTP_PORT);
	
	// Capture send time for network delay compensation
	#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_SAMD)
	uint32_t sendTime = micros();
	#else
	uint32_t sendTime = millis();
	#endif
	
	udp->write(ntpRequest, NTP_PACKET_SIZE);
	udp->endPacket();
	
	uint32_t startTime = millis();
	int size = 0;
	while (size < NTP_PACKET_SIZE || size > NTP_PACKET_MAX_SIZE) {
		size = udp->parsePacket();
		if (millis() - startTime > 1000) return false; // 1 second timeout
		if (size > 0 && (size < NTP_PACKET_SIZE || size > NTP_PACKET_MAX_SIZE)) return false; // Invalid packet size
		if (size == 0) delay(1); // Yield to avoid watchdog issues on ESP platforms
		}
	
	// Capture receive time for network delay compensation
	#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_SAMD)
	uint32_t receiveTime = micros();
	#else
	uint32_t receiveTime = millis();
	#endif
	
	lastUpdate = millis();
	udp->read(ntpQuery, NTP_PACKET_SIZE);

	// Stratum 0 is a "Kiss-of-Death" packet — server is telling us to back off.
	// Double the configured update interval (up to a maximum of 300000 ms).
	if (ntpQuery[1] == 0) {
		if (interval < 300000) interval = min(interval * 2, (uint32_t)300000);
		hasValidSync = false;
		return false;
		}

	// Read timestamp and fractional seconds from NTP response
	uint32_t fraction = (uint32_t)ntpQuery[44] << 24 | (uint32_t)ntpQuery[45] << 16 | 
	                    (uint32_t)ntpQuery[46] << 8 | (uint32_t)ntpQuery[47];
	
	#ifdef __AVR__
 		unsigned long highWord = word(ntpQuery[40], ntpQuery[41]);
		unsigned long lowWord = word(ntpQuery[42], ntpQuery[43]);
		uint32_t timestamp = highWord << 16 | lowWord;
		if (timestamp != 0) {
			// Compensate for network delay
			compensateNetworkDelay(sendTime, receiveTime, timestamp, fraction);
 			ntpTime = timestamp;
 			utcTime = ntpTime - NTP_OFFSET;
			hasValidSync = true;
			everSynced = true;
			}
		else {
			hasValidSync = false;
			return false;
			}
 	#else
 		uint32_t timestamp = (uint32_t)ntpQuery[40] << 24 | (uint32_t)ntpQuery[41] << 16 | (uint32_t)ntpQuery[42] << 8 | (uint32_t)ntpQuery[43];
		if (timestamp != 0) {
			// Compensate for network delay
			compensateNetworkDelay(sendTime, receiveTime, timestamp, fraction);
 			ntpTime = timestamp;
 			utcTime = ntpTime - SEVENTYYEARS;
			hasValidSync = true;
			everSynced = true;
			// Sync ESP32/ESP8266 system RTC if enabled
			#if defined(ESP32) || defined(ESP8266)
			if (syncSystemRTC) {
				struct timeval tv;
				tv.tv_sec = utcTime;
				// Convert NTP fraction to microseconds: fraction * 1,000,000 / 2^32
				tv.tv_usec = ((uint64_t)fraction * 1000000ULL) >> 32;
				settimeofday(&tv, NULL);
				}
			#endif
			}
		else {
			hasValidSync = false;
			return false;
			}
 	#endif
	if (dstZone && dstRuleConfigured && stdRuleConfigured) {
		timezoneOffset = dstEnd.tzOffset * SECS_PER_MINUTES;
		dstOffset = (dstStart.tzOffset - dstEnd.tzOffset) * SECS_PER_MINUTES;
		}
	return true;
	}

void NTP::compensateNetworkDelay(uint32_t sendTime, uint32_t receiveTime, uint32_t& timestamp, uint32_t& fraction) {
	// Calculate round-trip delay
	#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_SAMD)
	// sendTime and receiveTime are in microseconds
	uint32_t roundTripUs = receiveTime - sendTime;
	lastRoundTripDelay = roundTripUs;  // Store for retrieval
	uint32_t oneWayUs = roundTripUs / 2;  // Assume symmetric network delay
	
	// Add one-way delay to the timestamp
	// Convert microseconds to NTP fraction (2^32 / 1,000,000)
	uint64_t delayFraction = ((uint64_t)oneWayUs * 4294967296ULL) / 1000000ULL;
	uint64_t adjustedFraction = (uint64_t)fraction + delayFraction;
	
	// Handle fraction overflow into seconds
	if (adjustedFraction >= 4294967296ULL) {
		timestamp++;
		adjustedFraction -= 4294967296ULL;
	}
	fraction = (uint32_t)adjustedFraction;
	
	#else
	// sendTime and receiveTime are in milliseconds  
	uint32_t roundTripMs = receiveTime - sendTime;
	lastRoundTripDelay = roundTripMs;  // Store for retrieval
	uint32_t oneWayMs = roundTripMs / 2;  // Assume symmetric network delay
	
	// Convert NTP fraction to milliseconds first
	uint16_t fractionMs = (uint16_t)(fraction / 4294967UL);
	
	// Add one-way delay in milliseconds
	uint32_t totalMs = fractionMs + oneWayMs;
	
	// Handle overflow into seconds (32-bit)
	while (totalMs >= 1000) {
		timestamp++;
		totalMs -= 1000;
	}
	
	// Convert back to NTP fraction: ms * 2^32 / 1000
	fraction = totalMs * 4294967UL;
	#endif
	
	// Update ntpMilliseconds from compensated fraction
	ntpMilliseconds = (uint16_t)(fraction / 4294967UL);
	}

void NTP::updateInterval(uint32_t interval) {
	this->interval = interval;
	}

#if defined(ESP32) || defined(ESP8266)
void NTP::syncRTC(bool enable) {
	syncSystemRTC = enable;
	}
#endif

void NTP::ruleDST(const char* tzName, int8_t week, int8_t wday, int8_t month, int8_t hour, int tzOffset) {
	// Validate input parameters
	if (week < 0 || week > 4) return;
	if (wday < 0 || wday > 6) return;
	if (month < 0 || month > 11) return;
	if (hour < 0 || hour > 23) return;
	
	const char* selectedName = tzName ? tzName : "";
	strncpy(dstStart.tzName, selectedName, sizeof(dstStart.tzName) - 1);
	dstStart.tzName[sizeof(dstStart.tzName) - 1] = '\0';
	dstStart.week = week;
	dstStart.wday = wday;
	dstStart.month = month;
	dstStart.hour = hour;
	dstStart.tzOffset = tzOffset;
	dstRuleConfigured = true;
	}

const char* NTP::ruleDST() {
	if(dstZone && dstRuleConfigured) {
		const char* timeStr = ctime(&dstTime);
		if (!timeStr) return "Invalid DST time";
		strncpy(ruleString, timeStr, sizeof(ruleString) - 1);
		ruleString[sizeof(ruleString) - 1] = '\0';
		size_t len = strlen(ruleString);
		if (len > 0 && ruleString[len - 1] == '\n') ruleString[len - 1] = '\0';
		return ruleString;
		}
	else return RULE_DST_MESSAGE;
	}

void NTP::ruleSTD(const char* tzName, int8_t week, int8_t wday, int8_t month, int8_t hour, int tzOffset) {
	// Validate input parameters
	if (week < 0 || week > 4) return;
	if (wday < 0 || wday > 6) return;
	if (month < 0 || month > 11) return;
	if (hour < 0 || hour > 23) return;
	
	const char* selectedName = tzName ? tzName : "";
	strncpy(dstEnd.tzName, selectedName, sizeof(dstEnd.tzName) - 1);
	dstEnd.tzName[sizeof(dstEnd.tzName) - 1] = '\0';
	dstEnd.week = week;
	dstEnd.wday = wday;
	dstEnd.month = month;
	dstEnd.hour = hour;
	dstEnd.tzOffset = tzOffset;
	stdRuleConfigured = true;
	}
		
const char* NTP::ruleSTD() {
	if(dstZone && stdRuleConfigured) {
		const char* timeStr = ctime(&stdTime);
		if (!timeStr) return "Invalid STD time";
		strncpy(ruleString, timeStr, sizeof(ruleString) - 1);
		ruleString[sizeof(ruleString) - 1] = '\0';
		size_t len = strlen(ruleString);
		if (len > 0 && ruleString[len - 1] == '\n') ruleString[len - 1] = '\0';
		return ruleString;
		}
	else return RULE_STD_MESSAGE;
	}

const char* NTP::tzName() {
	if (dstZone && dstRuleConfigured && stdRuleConfigured && hasValidSync) {
		if (summerTime()) return dstStart.tzName;
		else return dstEnd.tzName;
		}
	return GMT_MESSAGE;
	}

void NTP::timeZone(int8_t tzHours, int8_t tzMinutes) {
	timezoneOffset = (int32_t)tzHours * 3600;
	if (tzHours < 0) {
		timezoneOffset -= tzMinutes * 60;
		}
	else {
		timezoneOffset += tzMinutes * 60;
		}
	}

void NTP::isDST(bool dstZone) {
	this->dstZone = dstZone;
	}

bool NTP::isDST() {
	if (dstZone && dstRuleConfigured && stdRuleConfigured && hasValidSync) return summerTime();
	return false;
	}

time_t NTP::epoch() {
	currentTime();
	return utcCurrent; 
	}

void NTP::currentTime() {
	utcCurrent = utcTime + ((millis() - lastUpdate) / 1000); 
	if (dstZone && dstRuleConfigured && stdRuleConfigured && hasValidSync) {
		// Bootstrap DST transition times on the first successful call after a
		// failed init() – yearDST==0 means beginDST() has never run.
		// Use a UTC-based gmtime to get the year, then compute the transitions
		// before evaluating summerTime(), so utcDST/utcSTD are valid.
		if (yearDST == 0) {
			current = gmtime(&utcCurrent);
			if (current) beginDST();
			}
		if (summerTime()) {
			local = utcCurrent + dstOffset + timezoneOffset;
			current = gmtime(&local);
			if (!current) return;  // Invalid time
			}
		else {
			local = utcCurrent + timezoneOffset;
			current = gmtime(&local);
			if (!current) return;  // Invalid time
			}
		if ((current->tm_year + 1900) > yearDST) beginDST();
		}
	else {
		local = utcCurrent + timezoneOffset;
		current = gmtime(&local);
		if (!current) return;  // Invalid time
		}
	}

int16_t NTP::year() {
	currentTime();
	if (!current) return 1970;  // Default fallback
	return current->tm_year + 1900;
	}

int8_t NTP::month() {
	currentTime();
	if (!current) return 1;  // Default fallback
	return current->tm_mon + 1;
	}

int8_t NTP::day() {
	currentTime();
	if (!current) return 1;  // Default fallback
	return current->tm_mday;
	}

int8_t NTP::weekDay() {
	currentTime();
	if (!current) return 0;  // Default fallback
	return current->tm_wday;
	}

int8_t NTP::hours() {
	currentTime();
	if (!current) return 0;  // Default fallback
	return current->tm_hour;
	}

int8_t NTP::minutes() {
	currentTime();
	if (!current) return 0;  // Default fallback
	return current->tm_min;
	}

int8_t NTP::seconds() {
	currentTime();
	if (!current) return 0;  // Default fallback
	return current->tm_sec;
	}

uint16_t NTP::milliseconds() {
	// Get full elapsed time, then reduce to just the ms component
	uint32_t elapsed = millis() - lastUpdate;
	uint32_t elapsedMs = elapsed % 1000;  // Only the ms portion of elapsed time
	
	// Add to NTP milliseconds (max: 999 + 999 = 1998)
	uint16_t total = ntpMilliseconds + elapsedMs;
	return (total >= 1000) ? (total - 1000) : total;
	}

const char* NTP::formattedTime(const char *format) {
	currentTime();
	memset(timeString, 0, sizeof(timeString));
	if (!current) {
		strncpy(timeString, "Invalid time", sizeof(timeString) - 1);
		return timeString;
		}
	strftime(timeString, sizeof(timeString), format, current);
	return timeString;
	}

void NTP::beginDST() {
	if (!current) return;  // Invalid time
	dstTime = calcDateDST(dstStart, current->tm_year + 1900);
	utcDST = dstTime - (dstEnd.tzOffset * SECS_PER_MINUTES);
	stdTime = calcDateDST(dstEnd, current->tm_year + 1900);
	utcSTD = stdTime - (dstStart.tzOffset * SECS_PER_MINUTES);
	yearDST = current->tm_year + 1900;
	}

time_t NTP::calcDateDST(struct ruleDST rule, int year) {
	uint8_t month = rule.month;
	uint8_t week = rule.week;
	if (week == 0) {
		if (++month > 11) {
			month = 0;
			year++;
			}
		week = 1;
		}

	struct tm tm = {};
	tm.tm_hour = rule.hour;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	tm.tm_mday = 1;
	tm.tm_mon = month;
	tm.tm_year = year - 1900;
	time_t t = mktime(&tm);

	t += ((rule.wday - tm.tm_wday + 7) % 7 + (week - 1) * 7 ) * SECS_PER_DAY;
	if (rule.week == 0) t -= 7 * SECS_PER_DAY;
	return t;
	}

bool NTP::summerTime() {
	if (utcDST < utcSTD) {
		return (utcCurrent > utcDST) && (utcCurrent <= utcSTD);
		}
	return (utcCurrent > utcDST) || (utcCurrent <= utcSTD);
	}

uint32_t NTP::ntp() {
	return ntpTime;
	}

uint32_t NTP::utc() {
	return utcTime;
	}

bool NTP::isValid() {
	// No valid data if we haven't synced yet
	if (!hasValidSync) return false;
	
	// Check Leap Indicator (top 2 bits of byte 0)
	uint8_t leapIndicator = ntpQuery[0] >> 6;
	if (leapIndicator == 3) return false;  // Clock unsynchronized (alarm condition)
	
	// Check Stratum (byte 1)
	uint8_t stratum = ntpQuery[1];
	if (stratum == 0) return false;   // Kiss-o'-Death or unspecified
	if (stratum >= 16) return false;  // Unsynchronized or reserved value
	
	return true;
	}

float NTP::roundTripDelay() {
	#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_SAMD)
	// Convert microseconds to milliseconds
	return lastRoundTripDelay / 1000.0;
	#else
	// Already in milliseconds
	return (float)lastRoundTripDelay;
	#endif
	}