#include "param.h"
#include "pico/stdlib.h"
#include "logger_config.h"
#include "stdio.h"
#include <string.h>
#include <ctype.h>
#include "hardware/flash.h"
#include <inttypes.h>
#include "stdlib.h"
#include  "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
//#include "tools.h"
#include "hardware/pio.h"  // needed for sbus_out_pwm.h
#include <errno.h>   // used by strtol() to check for errors 
#include "Arduino.h"
#include "sd.h"
// commands could be in following form:
// ADD = xx/yy with xx and yy are interger in range 0/63
// DEL = XX/YY
// CS = 0/29
// MOSI = (SPI1)
// MISO = (SPI1) 
// SCLK = (SPI1)
// LED = 16
// PROTOCOL = O (oXs)
// for RP2040_zero, pin 16 = LED
// When no config is found in memory, a default config is loaded (defined in config.h)


#define CMD_BUFFER_LENTGH 2000
uint8_t cmdBuffer[CMD_BUFFER_LENTGH];
uint16_t cmdBufferPos = 0;

char const * listOfCsvFieldName[64] = LIST_OF_CSV_FIELD_NAME; 


CONFIG config;
uint8_t debugTlm = 'N';
uint8_t debugSbusOut = 'N';

uint8_t pinCount[30] = {0};


bool pinIsduplicated ;
extern bool configIsValid; 
extern bool multicoreIsRunning; 
volatile bool isPrinting = false;
//extern field fields[];  // list of all telemetry fields and parameters used by Sport

//extern queue_t qSendCmdToCore1;


void handleUSBCmd(void){
    uint8_t c;
    while (Serial.available()) {
        c = Serial.read();
        //Serial.println(c,HEX);
        //printf("%X\n", (uint8_t) c);
        
        if (cmdBufferPos >= (CMD_BUFFER_LENTGH -1) ){  // discard previous char when buffer is full
            cmdBufferPos = 0;
        }
        if ( c != '\n') {
            cmdBuffer[cmdBufferPos++] = c ; // save the char
           // printf("%c\n", (uint8_t) c);
        } else {
            //Serial.println("start processing a command");
            cmdBuffer[cmdBufferPos] = 0x00 ; // put the char end of string
            cmdBufferPos = 0;                // reset the position
            processCmd();                    // process the cmd line
        }
    }
}

void printInstructions(){ 
    isPrinting = true; //use to discard incoming data from Sbus... while printing a long text (to avoid having the queue saturated)
    Serial.println(" ");
    Serial.println("Commands can be entered to change the config parameters");
    Serial.println("- To select data to be part of log file, enter ADD = xx / yy");
    Serial.println("- To unselect data, enter DEL = xx / yy");
    Serial.println("      where xx and yy are the index of the fields provided by oXs");   
    Serial.println("- To change the minimum interval between 2 log entries, enter INTV=XXXX");
    Serial.println("       XXXX is in msec and can be 0 (to get all log entries)");
        
    Serial.println("- To change the logging mode, enter MODE=C or MODE=T or MODE=F");
    Serial.println("      C (continuous = log always), T (Triggered = start when MIN & MAX match) , F (Filtered = log when MIN & MAX match)");
    Serial.println("- To change the MIN, enter MINF=XX (for the field index) and enter MINV=YYYYY (for the value)");
    Serial.println("- To change the MAX, enter MAXF=XX (for the field index) and enter MINV=YYYYY (for the value)");
    
    Serial.println("- To change (invert) led color, enter LED=N or LED=I");
    Serial.println("- To change the gpio that get the data from oXs, enter DATA= XX  (with XX = 5, 9, 21, 25)");
    Serial.println("- To change the baudrate, enter BAUD= XXXXXX  (with XXXXXX = the baudrate e.g. 15200)");
    Serial.println("- To change the SPI port being used for sd card, enter SD=0 (for SPI) or SPI=1 (for SPI1) ");
    Serial.println("- To change the SD MOSI gpio, enter MOSI= XX  (with XX = 3, 7, 19, 23)");
    Serial.println("- To change the SD MISO gpio, enter MISO= XX  (with XX = 0, 4, 16, 20)");
    Serial.println("- To change the SD SCLK gpio, enter SCLK= XX  (with XX = 2, 6, 18, 22)");
    Serial.println("- To change the SD CS   gpio, enter CS= XX    (with XX in range 0...29)");
    
    Serial.println(" ");
    Serial.println("   Note: some changes require a manual reset to be applied");
    Serial.println(" ");
    Serial.println("- To get the content of current csv file, enter PF (PF means Print File)");
    Serial.println(" ");
    Serial.println("-To get the current config, just press Enter");
    
    isPrinting = false;
    return;  
}

void updateFromToConfig(uint8_t fromValue, uint8_t toValue, bool flag){  // the flag is used to update confi.fieldToAdd[]
    // update the table that say which fields have to be included in the csv
    //Serial.print("From = "); Serial.print(fromValue); Serial.print("   To=");Serial.println(toValue); 
    if ((fromValue >= sizeof(config.fieldToAdd)) || (toValue >= sizeof(config.fieldToAdd) ) ) {
        Serial.println("Error : from or to in range of fields to be part of csv exceeds 64");
        return; 
    }
    for (uint8_t i=fromValue ; i <= toValue; i++){
        config.fieldToAdd[i]= flag;
    }
    //Serial.print("fields in csv:");
    //for (uint8_t i = 0 ; i< 64 ; i++){
    //    Serial.print(" ");
    //    if (config.fieldToAdd[i]) {Serial.print("+");} else {Serial.print("-");}
    //}
    //Serial.println( " ");
    //delay(200);
}

char * pkey = NULL;
char * pvalue = NULL;
char * ptoValue = NULL;     

void processCmd(){  // process the command entered via usb; called when a full command has been buffered
    //Serial.println("processing cmd");
    bool updateConfig = false;      // after some cheks, says if we can save the config
    char *ptr;
    uint32_t ui;   // temporary value to get an uint32_t
    int32_t  iTemp; //temporary value to get an int32_t
    pkey = NULL;   // point the first char of the key
    pvalue = NULL; // point the first char of the value (= from in case of a range)  
    ptoValue = NULL;    // point to the first char of the to in case of a range
    //printf("buffer0= %X\n", cmdBuffer[0]);
    if (cmdBuffer[0] == 0x0D){ // when no cmd is entered we print the current config
        printConfig();
        return; 
    }
    if (cmdBuffer[0] == '?'){ // when help is requested we print the instruction
        printInstructions(); 
        return;  
    }
    if (cmdBuffer[0] != 0x0){
        removeSpaces((char *) cmdBuffer);
        char * equalPos = strchr( (char*)cmdBuffer, '=');  // search position of '='
        if (equalPos != NULL){ // there is = so search for value
            *equalPos = 0x0;      // replace '=' with End of string    
            pvalue = equalPos + 1; // point to next position
            //removeTrailingWhiteSpace(pvalue);
        }    
        pkey = (char*)cmdBuffer;  
    }
    upperStr(pkey);
    upperStr(pvalue);
    // pkey point to the key (before '=')
    // pvalue point to the value (it can be a range)
    Serial.println(" ");
    Serial.print("Cmd to execute: ");   
    if (pkey) {Serial.print(" "); Serial.print(pkey);}
    if (pvalue) {Serial.print(" = ");Serial.print(pvalue);}
    Serial.println(" ");
    bool wrongCmd = true;  // used to detect invalid command
    
    // change fields to add in the csv
    if ( strcmp("ADD", pkey) == 0 ) { 
        char * slashPos = strchr( pvalue, '/');  // search position of '/' starting at pvalue
        uint8_t fromValue ;
        uint8_t toValue ;
        if (slashPos != NULL){ // there is a '/' so search for ptoValue
            *slashPos = 0x0;      // replace '=' with End of string    
            ptoValue = slashPos + 1; // point to next position
        }    
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : from part of range of value must be an unsigned integer");
        } else if ( ui > 63 ) {
            Serial.println("Error : from part of range must be less than 64");
        } else {
           fromValue = (uint8_t) ui;
           //Serial.print("from="); Serial.println(fromValue);
           if (slashPos == NULL){
                updateFromToConfig(fromValue ,fromValue, true);
                updateConfig = true;
           } else {
                //Serial.print("buffer= ");
                //for(uint8_t i=0; i<10;i++){
                //    Serial.print(" "); Serial.print(cmdBuffer[i],HEX); 
                //}
                //Serial.println(" ");
                //Serial.println( *ptoValue, HEX);
                ui = strtoul(ptoValue, &ptr, 10);
                if ( *ptr != 0x0){
                    Serial.println("Error : end of range value must be an unsigned integer\n");
                } else if ( ui > 63 ) {
                    Serial.println("Error : end of range must be less than 64\n");
                } else if ( ((uint8_t) ui) < fromValue ) {
                    Serial.println("Error : end of range must be greater or equal to begin of range\n");
                } else {    
                    toValue = (uint8_t) ui;
                    //Serial.print("to="); Serial.println(toValue);
                    //Serial.println("updateFromToConfig will be called");
                    updateFromToConfig(fromValue,toValue, true);
                    updateConfig = true;
                }
           }    
        }
    }
    // change fields to remove from the csv
    if ( strcmp("DEL", pkey) == 0 ) { 
        char * slashPos = strchr( pvalue, '/');  // search position of '/' starting at pvalue
        uint8_t fromValue ;
        uint8_t toValue ;
        if (slashPos != NULL){ // there is a '/' so search for ptoValue
            *slashPos = 0x0;      // replace '=' with End of string    
            ptoValue = slashPos + 1; // point to next position
        }    
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : from part of range of value must be an unsigned integer");
        } else if ( ui > 63 ) {
            Serial.println("Error : from part of range must be less than 64");
        } else {
           fromValue = (uint8_t) ui;
           //Serial.print("from="); Serial.println(fromValue);
           if (slashPos == NULL){
                updateFromToConfig(fromValue ,fromValue, false);
                updateConfig = true;
           } else {
                //Serial.print("buffer= ");
                //for(uint8_t i=0; i<10;i++){
                //    Serial.print(" "); Serial.print(cmdBuffer[i],HEX); 
                //}
                //Serial.println(" ");
                //Serial.println( *ptoValue, HEX);
                ui = strtoul(ptoValue, &ptr, 10);
                if ( *ptr != 0x0){
                    Serial.println("Error : end of range value must be an unsigned integer\n");
                } else if ( ui > 63 ) {
                    Serial.println("Error : end of range must be less than 64\n");
                } else if ( ((uint8_t) ui) < fromValue ) {
                    Serial.println("Error : end of range must be greater or equal to begin of range\n");
                } else {    
                    toValue = (uint8_t) ui;
                    //Serial.print("to="); Serial.println(toValue);
                    //Serial.println("updateFromToConfig will be called");
                    updateFromToConfig(fromValue,toValue, false);
                    updateConfig = true;
                }
           }    
        }
    }
    
    // change SPI used for SD card
    if ( strcmp("SD", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : spi used for SD card must be an unsigned integer\n");
        } else if ( !(ui!=0 or ui!=1)) {   
            Serial.println("Error : spi used for sd card must be 0 (SPI) or 1 (SPI1)\n");
        } else {    
            config.spiForSd = ui;
            Serial.print("Spi for sd card = ");Serial.println(config.spiForSd);
            updateConfig = true;
        }
    }
    
    // change MISO pin
    if ( strcmp("MISO", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : gpio must be an unsigned integer\n");
        } else if ( !(ui!=0 or ui!=4 or ui!=16 or ui!=20 or ui!=8 or ui!=12 or ui!=24 or ui!=28)) {   //0 4 16 20
            Serial.println("Error : MISO gpio must be 0, 4, 16, 20 (for SPI) or 8, 12, 24, 28 (for SPI1)\n");
        } else {    
            config.pinSpiMiso = ui;
            Serial.print("Gpio for MISO = ");Serial.println(config.pinSpiMiso);
            updateConfig = true;
        }
    }
    
    // change MOSI pin
    if ( strcmp("MOSI", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : gpio must be an unsigned integer\n");
        } else if ( !(ui!=3 or ui!=7 or ui!=19 or ui!=23 or ui!=11 or ui!=15 or ui!=27)) { 
            Serial.println("Error : MOSI gpio must be 3, 7, 19, 23 (for SPI) or 11, 15, 27(for SPI1)\n");
        } else {    
            config.pinSpiMosi = ui;
            Serial.print("Gpio for MOSI = ");Serial.println(config.pinSpiMosi);
            updateConfig = true;
        }
    }
    
    // change SCLK pin
    if ( strcmp("SCLK", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : gpio must be an unsigned integer\n");
        } else if ( !(ui!=2 or ui!=6 or ui!=18 or ui!=22 or ui!=10 or ui!=14 or ui!=26)) {   
            Serial.println("Error : SCLK gpio must be 2, 6, 18, 22 (for SPI) or 10, 14, 26 (for SPI1)\n");
        } else {    
            config.pinSpiSclk = ui;
            Serial.print("Gpio for SCLK = ");Serial.println(config.pinSpiSclk);
            updateConfig = true;
        }
    }
    
    // change CS pin
    if ( strcmp("CS", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : GPIO must be an unsigned integer");
        } else if (ui>29) {   
            Serial.println("Error : CS GPIO must be in range 0...29");
        } else {    
            config.pinSpiCs = ui;
            Serial.print("GPIO for CS = ");Serial.println(config.pinSpiCs);
            updateConfig = true;
        }
    }
    
    // change DATA_IN pin (used by Serial2 = UART1 to get data from oXs)
    if ( strcmp("DATA", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : GPIO must be an unsigned integer\n");
        } else if ( !(ui!=2 or ui!=6 or ui!=18 or ui!=22)) {   //2 6 18 22
            Serial.println("Error : DATA in GPIO must be 5, 9, 21 or 25\n"); // 5, 9, 21, 25
        } else {    
            config.pinSerialRx = ui;
            Serial.print("GPIO for DATA = ");Serial.println(config.pinSerialRx);
            updateConfig = true;
        }
    }
    

    // change for SDA pin
    /*
    if ( strcmp("SDA", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            printf("Error : pin must be an unsigned integer\n");
        } else if ( !(ui==2 or ui==6 or ui==10 or ui==14 or ui==18 or ui==22 or ui==26 or ui ==255)) {
            printf("Error : pin must be 2, 6, 10, 14, 18, 22, 26 or 255\n");
        } else {    
            //config.pinSda = ui;
            printf("Pin for SDA (baro) = %u\n" , config.pinSda );
            updateConfig = true;
        }
    }
    // change for SCL pin
    if ( strcmp("SCL", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            printf("Error : pin must be an unsigned integer\n");
        } else if ( !(ui==3 or ui==7 or ui==11 or ui==15 or ui==19 or ui==23 or ui==27 or ui ==255)) {
            printf("Error : pin must be 3, 7, 11, 15, 19, 23, 27 or 255\n");
        } else {    
            //config.pinScl = ui;
            printf("Pin for Scl (baro) = %u\n" , config.pinScl );
            updateConfig = true;
        }
    }
    */    
    // change baudrate
    if ( strcmp("BAUD", pkey) == 0 ) { // if the key is BAUD
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : baudrate must be an unsigned integer");
        } else {
            config.serialBaudrate = ui;
            Serial.print("baud = "); Serial.println(config.serialBaudrate);
            updateConfig = true;
        }
    }
    
    // change protocol
    if ( strcmp("PROTOCOL", pkey) == 0 ) { // 
        if (strcmp("O", pvalue) == 0) {
            config.protocol = 'O';
            updateConfig = true;
        } else if (strcmp("O", pvalue) == 0) { // just to keep a model for another
            config.protocol = 'O';
            updateConfig = true;
        } else  {
            Serial.println("Error : protocol must be O (oXs + CSV)");
        }
    }
    // change led color
    if ( strcmp("LED", pkey) == 0 ) {
        if (strcmp("N", pvalue) == 0) {
            config.ledInverted = 'N';
            updateConfig = true;
        } else if (strcmp("I", pvalue) == 0) {
            config.ledInverted = 'I';
            updateConfig = true;
        } else  {
            Serial.println("Error : LED color must be N (normal) or I(inverted)");
        }
    }
    
    // change INTV (minimum interval between 2 log csv)
    if ( strcmp("INTV", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : minimum onterval must be an unsigned integer");
        } else {
            config.minInterval = ui;
            Serial.print("minimum interval between 2 log = "); Serial.println(config.minInterval);
            updateConfig = true;
        }
    }
    
    
    // change mode (C = continous=0 , T = triggered = 1, F = filtered =2)
    if ( strcmp("MODE", pkey) == 0 ) { // 
        if (strcmp("C", pvalue) == 0) {
            config.mode = '0';
            updateConfig = true;
        } else if (strcmp("T", pvalue) == 0) { 
            config.mode = '1';
            updateConfig = true;
        } else if (strcmp("F", pvalue) == 0) { 
            config.mode = '2';
            updateConfig = true;
        } else  {
            Serial.println("Error : mode must be C (continous), T (triggered) or F (filtered)");
        }
    }
    
    // change MINF
    if ( strcmp("MINF", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : field index must be an unsigned integer\n");
        } else if ( !(ui < 64)) {   //2 6 18 22
            Serial.println("Error : field index must be in range 0...63\n"); 
        } else {    
            config.minField = ui;
            Serial.print("Field index for min value = ");Serial.println(config.minField);
            updateConfig = true;
        }
    }
    
    // change MINV
    if ( strcmp("MINV", pkey) == 0 ) { 
        iTemp = strtol(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : field value must be an integer\n");
        } else {    
            config.minValue = iTemp;
            Serial.print("Min value = ");Serial.println(config.minValue);
            updateConfig = true;
        }
    }
    
    // change MAXF
    if ( strcmp("MAXF", pkey) == 0 ) { 
        ui = strtoul(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : field index must be an unsigned integer\n");
        } else if ( !(ui < 64)) {   //2 6 18 22
            Serial.println("Error : field index must be in range 0...63\n"); 
        } else {    
            config.maxField = ui;
            Serial.print("Field index for max value = ");Serial.println(config.maxField);
            updateConfig = true;
        }
    }
    
    // change MAXV
    if ( strcmp("MAXV", pkey) == 0 ) { 
        iTemp = strtol(pvalue, &ptr, 10);
        if ( *ptr != 0x0){
            Serial.println("Error : field value must be an integer\n");
        } else {    
            config.maxValue = iTemp;
            Serial.print("min value = ");Serial.println(config.maxValue);
            updateConfig = true;
        }
    }
    


    // after this line, put commands that does nor require to update the config; set also wrongCmd = false
    // print content of current csv file
    if ( strcmp("PF", pkey) == 0 ) {
        Serial.println("printing CSV content");
        readCsvFile();
        wrongCmd = false;  
    }
    
    if ( (updateConfig == false) && (wrongCmd == true) ) {
        Serial.println ("Wrong command: command is discarded");
    }

    if (updateConfig) {
        saveConfig();
        Serial.println("config has been saved");  
        //Serial.println("Device will reboot\n");
        //watchdog_enable(1500,false);
        //sleep_ms(1000);
        //watchdog_reboot(0, 0, 100); // this force a reboot!!!!!!!!!!
        //sleep_ms(5000);
        //Serial.println("OXS did not rebooted after 5000 ms");
    }
        
    //printConfig();                                       // print the current config
    Serial.println("\n >> ");
}

void addPinToCount(uint8_t pinId){   // used to check that a pin is not used twice
    if ( pinId != 255) {
        if (pinId > 29 ) {
            Serial.print("Error in parameters: one pin number is "); Serial.print(pinId); Serial.println("it must be <30");
            configIsValid = false;
        } else {
            pinCount[pinId]++;
        }
    }
}

void checkConfig(){     // set configIsValid 
    // each pin can be used only once (also in sequencer)
    // if SDA is defined SCL must be defined too, and the opposite
    // if GPS_TX is defined GPS_RX must be defined too and the opposite
    watchdog_update(); //sleep_ms(500);
    //pinIsduplicated = false; 
    for (uint8_t i = 0 ; i<30; i++) pinCount[i] = 0; // reset the counter
    configIsValid = true;
    addPinToCount(config.pinSpiCs);
    addPinToCount(config.pinSpiMiso);
    addPinToCount(config.pinSpiMosi);
    addPinToCount(config.pinSpiSclk);
    addPinToCount(config.pinSerialRx);
    addPinToCount(config.pinLed);
    
    for (uint8_t i = 0 ; i<30; i++) {
        if (pinCount[i] > 1) {
            Serial.print("Error in parameters: pin "); Serial.print(i); Serial.print("is used ");
            Serial.print(pinCount[i]);Serial.println("times");
            configIsValid=false;
            pinIsduplicated= true;
        }          
    }

    if (config.spiForSd == 0 and (config.pinSpiMiso !=0 and config.pinSpiMiso !=4 and config.pinSpiMiso !=16 and config.pinSpiMiso !=20) ){
        Serial.println("Error : when spi for sd = 0 (SPI), MISO gpio must be 0, 4, 16 or 20"); 
        configIsValid=false;
    }
    if (config.spiForSd == 1 and (config.pinSpiMiso !=8 and config.pinSpiMiso !=12 and config.pinSpiMiso !=24 and config.pinSpiMiso !=28) ){
        Serial.println("Error : when spi for sd = 1 (SPI1), MISO gpio must be 8, 12, 24 or 28");
        configIsValid=false; 
    }
    if (config.spiForSd == 0 and (config.pinSpiMosi !=3 and config.pinSpiMosi !=7 and config.pinSpiMosi !=19 and config.pinSpiMosi !=23) ){
        Serial.println("Error : when spi for sd = 0 (SPI), MOSI gpio must be 3, 7, 19 or 23"); 
        configIsValid=false;
    }
    if (config.spiForSd == 1 and (config.pinSpiMosi !=11 and config.pinSpiMosi !=15 and config.pinSpiMosi !=27) ){
        Serial.println("Error : when spi for sd = 1 (SPI1), MOSI gpio must be 11, 15 or 27");
        configIsValid=false; 
    }
    if (config.spiForSd == 0 and (config.pinSpiSclk !=2 and config.pinSpiSclk !=6 and config.pinSpiSclk !=18 and config.pinSpiSclk !=22) ){
        Serial.println("Error : when spi for sd = 0 (SPI), SCLK gpio must be 2, 6, 18 or 22"); 
        configIsValid=false;
    }
    if (config.spiForSd == 1 and (config.pinSpiSclk !=10 and config.pinSpiSclk !=14 and config.pinSpiSclk !=26) ){
        Serial.println("Error : when spi for sd = 1 (SPI1), SCLK gpio must be 10, 14 or 26");
        configIsValid=false; 
    }
  
    if ((config.mode ==1) || (config.mode==2)){
      if ((config.minField ==255 ) && (config.maxField ==255 )) {
        Serial.println("Error in parameters: when mode T or F, at least MINF or MAXF must be defined"); 
        configIsValid=false;    
      }
      if ((config.minField > 63 ) && (config.minField !=255 )) {
        Serial.println("Error in parameters: field index for MIN (MINF) must be < 63 or =255"); 
        configIsValid=false;    
      }  
      if ((config.maxField > 63 ) && (config.maxField !=255 )) {
        Serial.println("Error in parameters: field index for MAX (MAXF) must be < 63 or =255"); 
        configIsValid=false;    
      }  
      
    } 
    

    /*
    if ( (config.pinSda != 255 and config.pinScl==255) or
         (config.pinScl != 255 and config.pinSda==255) ) {
        printf("Error in parameters: SDA and SCL must both be defined or unused\n");
        configIsValid=false;
    }
    */
    if ( configIsValid == false) {
        Serial.println("\nAttention: error in config parameters");
    } else {
        Serial.println("\nConfig parameters are OK");
    }
//    if ( sequencerIsValid == false) {
//        printf("\nAttention: error in sequencer parameters\n");
//    } else {
//        printf("\nSequencer parameters are OK\n");
//    }
    
    Serial.println("Press ? + Enter to get help about the commands");
}

void printConfig(){
    isPrinting = true;
    char version[] =   VERSION ;
    Serial.print("\nVersion = "); Serial.println(version);
    
    Serial.println("Fields in CSV log (+ = included; - = not included)");
    for (uint8_t i=0; i<63; i++){
        Serial.print("  "); 
        if (config.fieldToAdd[i]) Serial.print(" + ") ;else Serial.print(" - ");
         Serial.print(i); Serial.print(" ");
        Serial.println(listOfCsvFieldName[i]); 
    }
    
    Serial.print("Minimum interval betwwen 2 log entries = ");Serial.print(config.minInterval);Serial.println(" ms");
    if ( (config.mode==0) || (config.mode==255) ) Serial.println("Log mode is Continous (log always, MIN/MAX are discarded)");
    if (config.mode==1) Serial.println("Log mode is Triggered (log starts when MIN & MAX match)");
    if (config.mode==2) Serial.println("Log mode is Filtered (log occurs when MIN & MAX match)");
    Serial.print("MIN : to log, value of field index "); Serial.print(config.minField); Serial.print(" must be more than "); Serial.println(config.minValue);  
    Serial.print("MAX : to log, value of field index "); Serial.print(config.maxField); Serial.print(" must be less than "); Serial.println(config.maxValue);  

    Serial.print("SD card use SPI"); Serial.print(config.spiForSd); Serial.println("    (0=SPI,  1=SPI1)");
    Serial.println("GPIO's used by sd card are:");
    Serial.print("    CS   = "); Serial.println(config.pinSpiCs);
    Serial.print("    MOSI = "); Serial.println(config.pinSpiMosi);
    Serial.print("    MISO = "); Serial.println(config.pinSpiMiso);
    Serial.print("    SCLK  = "); Serial.println(config.pinSpiSclk);
    
    Serial.println("UART with oXs uses:");
    Serial.print("    gpio to receive data from oXs = "); Serial.println(config.pinSerialRx);
    Serial.print("    Baudrate = "); Serial.println(config.serialBaudrate);

    if (config.protocol == 'O'){
        Serial.println("Protocol is O (oXs + csv)")  ;
    } else {
        Serial.println("Protocol is unknown")  ;
    }
    
    if (config.ledInverted == 'I'){
        Serial.println("Led color is inverted")  ;
    } else {
        Serial.println("Led color is normal (not inverted)")  ;
    }
    
    watchdog_update(); //sleep_ms(500);
    checkConfig();
    isPrinting = false;
} // end printConfig()


#define FLASH_CONFIG_OFFSET (256 * 1024)
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_CONFIG_OFFSET);

void __not_in_flash_func(saveConfig)() {
    uint8_t buffer[FLASH_PAGE_SIZE] = {0xff};
    memcpy(&buffer[0], &config, sizeof(config));
    Serial.print("size="); Serial.println(sizeof(config));
    sleep_ms(100);
    // Note that a whole number of sectors must be erased at a time.
    // irq must be disable during flashing
    //watchdog_enable(3000 , true);
    rp2040.idleOtherCore();
    uint32_t irqStatus = save_and_disable_interrupts();
    flash_range_erase(FLASH_CONFIG_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_CONFIG_OFFSET, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(irqStatus);
    rp2040.resumeOtherCore();
    //if (multicoreIsRunning) multicore_lockout_end_blocking();
    printConfig();
}


void upperStr( char *p){
    if (p == NULL ) return;
    while ( *p != 0){
        *p = toupper(*p);
        p++;
    }    
}

char * skipWhiteSpace(char * str)
{
	char *cp = str;
	if (cp)
		while (isspace(*cp))
			++cp;
	return cp;
}

void removeSpaces(char *str)
{
    // To keep track of non-space character count
    int count = 0;
 
    // Traverse the given string. If current character
    // is not space, then place it at index 'count++'
    for (int i = 0; str[i]; i++)
        if ( ! isspace( (int) str[i] ))
            str[count++] = str[i]; // here count is
                                   // incremented
    str[count] = '\0';
}

void removeTrailingWhiteSpace( char * str)
{
	if (str == nullptr)
		return;
	char *cp = str + strlen(str) - 1;
	while (cp >= str && isspace(*cp))
		*cp-- = '\0';
}

void setupConfig(){   // The config is uploaded at power on
    if (*flash_target_contents == CONFIG_VERSION ) {
        memcpy( &config , flash_target_contents, sizeof(config));
    } else {
        config.version = CONFIG_VERSION;
        bool fieldToAddTemp[64] = { FIELDS_TO_ADD };
        for (uint8_t i=0 ; i<64 ; i++) { config.fieldToAdd[i] = fieldToAddTemp[i]; }
        config.spiForSd = SPI_FOR_SD;
        config.pinSpiCs = SPI_CS;
        config.pinSpiMiso = SPI_MISO;
        config.pinSpiMosi = SPI_MOSI;
        config.pinSpiSclk = SPI_SCLK; 
        config.pinSerialRx = SERIAL_IN_RX_GPIO;
        config.pinLed = 16;
        config.protocol = 'O' ; // O = oXs + csv
        config.serialBaudrate = SERIAL_IN_BAUDRATE;
        config.ledInverted = 'N'; // not inverted
    }    
} 




