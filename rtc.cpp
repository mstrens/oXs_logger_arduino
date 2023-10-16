#include "Arduino.h"

#include "rtc.h"
#include "RTCx.h"
#include "Wire.h"

bool rtcInstalled = false; // is true when a RTC is detected 

// we use WIRE0 ()
//sda may be 0, 4 8 12 16 20 24
#define GPIO_SDA 0
  //scl may be 1, 5, 9, 13, 17, 21, 23

#define GPIO_SCL 1

void setupRtc(){
    #if defined(GPIO_SDA) && defined(GPIO_SCL) // init only if SDA/SCL are defined
    Wire.setSDA( GPIO_SDA) ; 
    Wire.setSCL( GPIO_SCL) ;
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
    #endif
}
//void setTime(uint8_t * timeBuffer){
//    #if defined(GPIO_SDA) && defined(GPIO_SCL) // init only if SDA/SCL are defined
//    #endif
//}
void getLoggerTime(){
	#if defined(GPIO_SDA) && defined(GPIO_SCL) // init only if SDA/SCL are defined
    struct RTCx::tm tm;
	rtc.readClock(tm);
    RTCx::printIsotime(Serial, tm).println();
    delay(2000);
    Serial.println("second reading of rtc");
    rtc.readClock(tm);
    RTCx::printIsotime(Serial, tm).println();
    

    #endif
}