// todo : write on SD per block of 512 bytes if possible
//        avoid asking to press a key to start core0
//        add RTC data to file creation
//        add a command to update RTC date/time
//        check that SPI pin are allowed for spi (done)
//        add another protocol (raw saving)
//        check stat about timing and % of use of buffers
//        add time between 2 mainloop (done)
//        implement led overflow
//        ? find a way to close a file
//        remove msg "Reset writeIDx" (done)
//        test if it is possible to use any gpio for cs (done)
//        test with more data generated by oXs
//        test with rc channels
//        test write file creation date/time when we get an initial GPS date and time (use timestamp()
//        Log only when some field is less than a value and more than another
//        Continue or not to log once it started
//        Set a min delay between 2 log


#include "logger_config.h"

//--------------for led + serialIn
#include "ws2812.h"
#include "serial_in.h"
#include "sd.h"
#include "param.h"
#include "oXs_logger.h"

extern CONFIG config;
bool configIsValid = true;
bool configIsValidPrev = true;
bool multicoreIsRunning = true;

bool blinking = true ;
uint8_t ledState = STATE_STARTING;
uint8_t prevLedState = STATE_STARTING;
uint32_t lastBlinkMillis;


//------------------------------------------------------
//void testDummyData(){
  //Serial.println("Press a key to stop testing with dummy value");
  //while (!Serial.available()) {
//    handleSerialIn();
//    sendTest();
  //} 
  //Serial.println("End of test with dummy value");
  
//}
void waitKeyPressed(){
    Serial.println(" ");
    Serial.println(F("Type any character to begin +++++++++++"));
    while (!Serial.available()) {
        yield();
    }
    while (Serial.available()) {
        Serial.read();
    }
}
//------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);  // uSB port used to debug
    // Wait for USB Serial
    while (!Serial) {
        yield();
    }
    delay(100);

    setupLed();
    setRgbColorOn(0,0,10); // start with blue color
  
    
    if (watchdog_caused_reboot()) {
        Serial.println("Rebooted by Watchdog!");
    } else {
        Serial.println("Clean boot");
        //sleep_ms(1000); // wait that GPS is initialized
    }
    setupConfig(); // retrieve the config parameters (crsf baudrate, voltage scale & offset, type of gps, failsafe settings)  
    checkConfig();
    waitKeyPressed();
    rp2040.fifo.push(1); // send a command to other core to allow SD to set up first 

    //setRgbColorOn(0,0,10);  // switch to blue during the setup of different sensors/pio/uart
    
    //Serial.println("Waiting end of creating csv file by other core");
    //uint32_t endOfSetupCore1;  // setup will return 0 if OK, 
    if ( rp2040.fifo.pop() != 0){  // wait end of set up core and then in case of error set ledState
        ledState = STATE_NO_SD;
        handleLedState();
    }    
    

    Serial2.setRX(config.pinSerialRx);
    Serial2.setFIFOSize(SERIAL_IN_FIFO_LEN);
    Serial2.begin(config.serialBaudrate);
    
    //Serial.println("End of setup on core0");
    setupQueues();
    #ifdef GENERATE_TEST_UART0_TX
    setupTest();   // set up uart0 to generate uart data and start generating the data using a timer callback
    #endif
    //watchdog_enable(3500, 0); // require an update once every 3500 msec
}
//------------------------------------------------------------------------------
void loop() {
    handleUSBCmd();
    if ( ledState != STATE_NO_SD) {
        handleSerialIn();  // get data from UART, stored them, create csv record in a buffer and push the references to a queue
        //printTestCount();    // just used to debug sending dummy data on Serial2 to simulate oXs
        //watchdog_update();
    }
    handleLedState();
    #ifdef REPORT_INTERVAL_MS
        reportStats(REPORT_INTERVAL_MS);     // report some stats at some interval (msec)
    #endif        
}


void setup1(){
    rp2040.fifo.pop(); // block until it get a value from main setup
    //Serial.println("Setup on core1 starting");
    // start sdfat and create a csv file; returned value = 0 if OK; else value is >0
    rp2040.fifo.push(setupSdCard()); // allow core 0 to continue setup()
    //Serial.println("End of setup of sd card on core1");
}

void loop1(){
    writeOnSdcard();  // get the parameter about the position of the csv record, store it on sdcar and says it is don
}

void handleLedState(){ 
    static uint8_t ledBlinkCount = 0; // used in order to get a minimum number of blink
    static uint ledBlinkIntervalMs = 300; 
    if (( ledState != prevLedState) && (ledBlinkCount == 0)){
        //printf(" %d\n ",ledState);
        prevLedState = ledState;
        setColorState();
        ledBlinkCount = 10;
        // blink fast when state says no SD
        if (ledState == STATE_NO_SD) {ledBlinkIntervalMs = 200;} else {ledBlinkIntervalMs = 500;}
    } else if ( blinking && (( millis() - lastBlinkMillis) > ledBlinkIntervalMs ) ){
        toggleRgb();
        lastBlinkMillis = millis();
        if (ledBlinkCount>0) ledBlinkCount--;
    }
}    

void setColorState(){    // set the colors based on the RF link
    // Binking red fast = no way to open the file
    // Binking red slow = no way to write in the file
    // Blinking green = OK (recording)
    // Blinking blue = getting no data to record
    // Blinking yellow = overflow 
    lastBlinkMillis = millis(); // reset the timestamp for blinking
    switch (ledState) {
        case STATE_OK:
            setRgbColorOn(0, 10, 0); //green
            break;
        case STATE_NO_SD:
            setRgbColorOn(10, 0, 0); //red
            break;
        case STATE_WRITE_ERROR:
            setRgbColorOn(10, 0, 0); //red
            break;
        case STATE_OVERFLOW:
            setRgbColorOn(10, 5, 0); //yellow
            break;
        case STATE_NO_DATA:
            setRgbColorOn(0, 0, 10); //blue
            break;
        default:
            setRgbColorOn(10, 0, 0); //red
            break;     
    }
}


/*
  printUnusedStack();
  // Read any Serial data.
  clearSerialInput();

  if (ERROR_LED_PIN >= 0) {
    digitalWrite(ERROR_LED_PIN, LOW);
  }
  Serial.println();
  Serial.println(F("type: "));
  Serial.println(F("b - open existing bin file"));
  Serial.println(F("c - convert file to csv"));
  Serial.println(F("l - list files"));
  Serial.println(F("p - print data to Serial"));
  Serial.println(F("r - record data"));
  Serial.println(F("t - test without logging"));
  Serial.println(F("d - test with dummy data"));
  
  while (!Serial.available()) {
    yield();
  }
  char c = tolower(Serial.read());
  Serial.println();

  if (c == 'b') {
    openBinFile();
  } else if (c == 'c') {
    if (createCsvFile()) {
      binaryToCsv();
    }
  } else if (c == 'l') {
    Serial.println(F("ls:"));
    sd.ls(&Serial, LS_DATE | LS_SIZE);
  } else if (c == 'p') {
    printData();
  } else if (c == 'r') {
    createBinFile();
    logData();
  } else if (c == 't') {
    testSensor();
  } else if (c == 'd') {
    testDummyData(); 
  } else {
    Serial.println(F("Invalid entry"));
  }
}
*/