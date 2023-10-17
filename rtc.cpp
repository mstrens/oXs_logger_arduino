#include "Arduino.h"

#include "rtc.h"
#include "RTCx.h"
#include "Wire.h"
#include "logger_config.h"
#include "param.h"

bool rtcInstalled = false; // is true when a RTC is detected 
extern CONFIG config;

// we use WIRE (I2C0)
// rtc object is created in the RTCx lib; namespace is RTCx

void setupRtc(){
    if ((config.pinSda == 255) || (config.pinScl ==  255) ){ // init only if SDA/SCL are defined
        return ;
    }
    Wire.setSDA( config.pinSda) ; 
    Wire.setSCL( config.pinScl) ;
    Wire.begin();
    Serial.println();
    Serial.println("Autoprobing for a RTC...");
    if (rtc.autoprobe()) {
        // Found something, hopefully a clock.
        Serial.print("Autoprobe found ");
        Serial.print(rtc.getDeviceName());
        Serial.print(" at 0x");
        Serial.println(rtc.getAddress(), HEX);
        rtcInstalled = true; 
    }
    else {
        // Nothing found at any of the addresses listed.; rtcInstalled remains false
        Serial.println("No RTCx found, cannot continue");
    }

    // Enable the battery backup. This happens by default on the DS1307
    // but needs to be enabled on the MCP7941x.
    rtc.enableBatteryBackup();

    // rtc.clearPowerFailFlag();

    // Ensure the oscillator is running.
    rtc.startClock();

    if (rtc.getDevice() == RTCx::MCP7941x) {
        Serial.print("Calibration: ");
        Serial.println(rtc.getCalibration(), DEC);
        // rtc.setCalibration(-127);
    }    
}

void setLoggerTime(char * s){
    if ((config.pinSda == 255) || (config.pinScl ==  255) ){ // only if SDA/SCL are defined
        return ;
    }
    if (s == NULL || strlen(s) < 14) { // YY-MM-DDTHH:MM
		Serial.println( "Errorr : date & time must be in format YY-MM-DDTHH:MM");
        return; 
    }
	struct RTCx::tm tm1;
	tm1.tm_year = atoi(s) + 10;
	s += 3;
	tm1.tm_mon = atoi(s) - 1;
	s += 3;
	tm1.tm_mday = atoi(s);
	s += 3;
	tm1.tm_hour = atoi(s);
	s += 3;
	tm1.tm_min = atoi(s);
	tm1.tm_sec = 0;
	rtc.mktime(&tm1);
    rtc.setClock(&tm1, RTCx::TIME);
}

/*
//          to do : store the date&time and set a flag
//void getLoggerTime(){
	if ((config.pinSda == 255) || (config.pinScl ==  255) ){ // only if SDA/SCL are defined
        return ;
    }
    struct RTCx::tm tm;
	rtc.readClock(tm);
    RTCx::printIsotime(Serial, tm).println();
    delay(2000);
    Serial.println("second reading of rtc");
    rtc.readClock(tm);
    RTCx::printIsotime(Serial, tm).println();
}
*/