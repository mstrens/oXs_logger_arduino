#include "pico/stdlib.h"
#include "stdio.h"  // used by printf
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/util/queue.h"
#include "serial_in.h"
//#include "tools.h"
#include <string.h> // used by memcpy
#include "Arduino.h"
#include "logger_config.h"
#include "sd.h"
//#include "param.h"
#include "hardware/address_mapped.h"
#include "hardware/platform_defs.h"
#include "hardware/uart.h"

#include "hardware/structs/uart.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "pico/assert.h"
#include "pico.h"
#include "ws2812.h"

#include "oXs_logger.h"

#include <charconv>
#include <stdio.h>
#include <system_error>
#include <algorithm>

// note: writing to SD card is done by core1 while preparing the csv data is done on core0
// writing to SD card can block the core1 for max 500 msec
// during this time, core0 must continue to collect data from oXs and convert them to CSV
// collecting data is done by an arduino fifo on UART2 (size is defined in logger_config.h)
// CSV data are buffered in a circular buffer
// before writing a csv rec to the buffer, we check that there is still some continuous free space in the buffer (CSV_MAX_RECORD_LEN)
// when csv data are written in the circular buffer, we use a queue (writeFromNbrQueue) to inform core1 about the index of the first byte and the number of bytes
// when core1 has written the data on SD card, it sent to core0 via a queue (lastWrittenQueue)



// Rx pin = 1, 13, 17,29 (UART0) 
// RX pin  = 5, 9, 21, 25  (UART1)= serial2
// Tx pin = 0, 12 ,16, 28 UART0 = serial1
// TX pin  = 4, 8, 20, 24  (UART1)= serial2


#define RC_CHANNEL_TYPE  41
#define FIRST_CHANNEL_IDX 41

//#define SERIAL_RX_PIN 5 // pin on uart1 to get the data to log
//#define SERIAL_UART_ID uart1
//#define SERIAL_BAUDRATE 115200
//#define SERIAL_QUEUE_LEN 1024*8


//queue_t inQueue ; // queue to get data from uart
queue_t writeFromNbrQueue ; // queue to communicate begin position and number of bytes to write on sd card
queue_t lastWrittenQueue ; // queue to communicate position in circular buffer of last byte written on SD card + number of bytes 


TLM tempTlm[64]; // tempory structure to keep type and data waiting for next 0X7E
uint8_t tempTlmCount = 0;  // number of data to push to next step
int currentTlm[64] = {0} ;  // current telemetry data (updated with last received data's and transmitted in csv format)
uint8_t tlmFormat[64]= LIST_OF_DECIMALS; // number of decimal per fields ;  0..3 number of décimals, 6 = GPS format (we first have to divide by 10 because original value has 7 digits)
bool fieldToAdd[64] ; // flag to say if the field must be part of CSV or not (init is done within setup of config)

uint32_t maxSerialAvailable;    // max number of bytes in Serial2 fifo (to compare with SERIAL_IN_FIFO_LEN)
uint32_t maxBytesInBuf = 0;     // max number of bytes in csvBuff (to compare with CSV_MAX_BUFFER_LEN)
uint maxFromNbrQueueLevel= 0;   // max level reached (to compare with WRITE_FROM_NBR_QUEUE_LEN)
uint maxLastWrittenQueueLevel = 0 ; // keep the max number of entries in the queue (to compare with LAST_WRITTEN_QUEUE_LEN)
uint32_t maxFillCsvUs = 0;      // max time to fill one csv record in buffer and add an entry to the queue (wait if buffer is full)
uint totalNbBytesInCsvFile = 0; // number of bytes written in csv file
uint32_t maxTimeBetween2HandleSerialInMs = 0;

extern uint maxWriteOnSdUs;        // max time to write one rec on sdcard    
extern uint32_t maxSyncOnSdUs;

extern uint8_t ledState; 

uint32_t firstGpsDate; // store the first GPS date (to avoid race issue)
uint32_t firstGpsTime; // idem for time
uint8_t createDateTimeState = 0; // flag to know if the file creation date and time is already registered
                                 // 0 = date/time unknown, 1 date/time known but not yet registered on SD ,2 = registered  

void reportStats(uint interval){
    static uint prevReportMs = 0;
    if ( (millis()- prevReportMs) > interval){
        prevReportMs = millis();
        Serial.println(" ");
        Serial.print("max ms between 2 call to main loop= "); Serial.println(maxTimeBetween2HandleSerialInMs);
        Serial.print("max % used of Serial2 fifo= "); Serial.println((maxSerialAvailable * 100) /SERIAL_IN_FIFO_LEN);
        Serial.print("max % of bytes in csvbuf= "); Serial.println((maxBytesInBuf * 100) / CSV_MAX_BUFFER_LEN);
        Serial.print("max us to fill csv rec= ") ; Serial.println(maxFillCsvUs);
        Serial.print("max % used of fromNbr queue= "); Serial.println((maxFromNbrQueueLevel * 100) /WRITE_FROM_NBR_QUEUE_LEN);
        Serial.print("max % used of lastWritten queued= "); Serial.println((maxLastWrittenQueueLevel * 100) /LAST_WRITTEN_QUEUE_LEN);
        Serial.print("max us to write on sd card= "); Serial.println(maxWriteOnSdUs);
        Serial.print("max us to synchro on sd card= "); Serial.println(maxSyncOnSdUs);
        
        Serial.print(" total bytes written in csv file= "); Serial.println(totalNbBytesInCsvFile);

        maxSerialAvailable = 0;    // max number of bytes in Serial2 fifo (to compare with SERIAL_IN_FIFO_LEN)
        maxBytesInBuf = 0;     // max number of bytes in csvBuff (to compare with CSV_MAX_BUFFER_LEN)
        maxFromNbrQueueLevel= 0;   // max level reached (to compare with WRITE_FROM_NBR_QUEUE_LEN)
        maxLastWrittenQueueLevel = 0 ; // keep the max number of entries in the queue (to compare with LAST_WRITTEN_QUEUE_LEN)
        maxFillCsvUs = 0;      // max time to fill one csv record in buffer and add an entry to the queue (wait if buffer is full)
        maxWriteOnSdUs = 0;        // max time to write one rec on sdcard    
        maxSyncOnSdUs = 0;

    }
}


enum IN_STATE{
    IN_WAIT_SYNC = 0,
    IN_RECEIVING_TIME,
    IN_RECEIVING_TYPE,
    IN_RECEIVING_DATA
};

uint8_t toString(int32_t in, uint8_t nDec, char * buf){ // format the int32 with n decimals in a buffer
    // return the number of written bytes (ascii decimal) 
    // this conversion takes about 20 usec (is faster than with float and sprintf)
    std::to_chars_result res; // structure uses to convert bin to char
    uint8_t nBytes=0;
    char * _buf = buf;
    int32_t _in = in;
    
    if (in < 0) {    // for negative number insert '-' and invert the number
        _in = -in;
        *_buf = '-';
        _buf++;
        nBytes=1;
    }
    //if (in !=0) {
    //    Serial.print("Converting: "); Serial.print(in);Serial.print(" "); Serial.println(_in);
    //}
    if (nDec == 0){
        res = std::to_chars(_buf, _buf + 11, _in);
        nBytes += (uint8_t )(res.ptr - _buf) ;
    } else {
        int divider =1;
        if (nDec==1) {divider = 10;
        } else if (nDec==2) {divider = 100; 
        } else if (nDec==3) {divider = 1000;
        } else if (nDec==6) {               // GPS long and lat has 7 dec and should be with only 6 in the csv
            divider = 1000000;
            _in = _in/10;                   // we first divide by 10 to get only 6 dec instead of 7
        }
        div_t divRest = div((int) _in, divider);
        res = std::to_chars(_buf, _buf + 11, divRest.quot); // fill first the quotient = integer part
        if (divRest.rem == 0){   // when there is no remain, no need to add a "."
            nBytes += (uint8_t )(res.ptr - _buf) ;
        } else {
            uint8_t nCharIntPart = (uint8_t )(res.ptr - _buf);
            char * pNewBufPos = res.ptr;
            *pNewBufPos = '.'; // add the point
            pNewBufPos++; //increment the pointer
            char temp[12];
            res = std::to_chars(temp, temp + 11, divRest.rem); // convert remain in a temporary array
            uint8_t nCharInRrem = (uint8_t )(res.ptr - temp);
            uint8_t nZeroToInsert = nDec - nCharInRrem ;
            while (nZeroToInsert != 0){
                *pNewBufPos = '0'; // add the 0
                pNewBufPos++; //increment the pointer
                nZeroToInsert--;
            }
            memcpy(pNewBufPos, temp, nCharInRrem);
            nBytes += nCharIntPart + 1 + nDec;
        }
    }
    //if (in !=0) {
    //    for (uint8_t i=0;i<nBytes;i++){
    //        Serial.print(buf[i], HEX); Serial.print(" ");
    //    }
    //    Serial.println();
    //}    
    return nBytes;
};

// in main loop:
// read UART, synchronize , remove stuffing, uncompress and store each value (one by one) in a temporary array tempTlm[]
// when next synchronize is detected, save temporary new values (in tempTlm[]] in a cumulative array that represent the current situation currentTlm[]
// Note: in currentTlm, timestamp is saved in the last index = 63 (0X3F)
// once saved in cumulative array (in binary format) , we check if there is space in a long circular csv buffer.
// if not we wait: when csv data are written on the SD card it makes space in the csv buffer.
// When there is enough space for one csv record, we convert immediately the current situation in a csv record saved in the circular long buffer
// when one csv record is added in this buffer, we push the first pos and number of bytes in a queue (could be handled by another core)

// so, if circular buffer is full, we do not process any more the incomong serial data (they are still filled in the serial fifo managed by arduino)


void handleSerialIn(){
    static IN_STATE inState=IN_WAIT_SYNC; 
    static int inCount = 0;
    //static uint32_t lastTestMicros = 0;
    static bool stuffing = false;
    static uint32_t inTime = 0;
    static uint8_t inType = 0;
    static uint8_t inNb0Byte = 0;
    static uint32_t inData = 0; 
    static uint32_t lastSerialInMs = 0;   // use to detect when we do not get valid data anymore
    static uint32_t lastCallToHandleSerialInMs = 0;
    
    uint8_t c;                     // char to read from uart2
    uint32_t serialAvailable = 0;  // number of char available
    uint32_t now = millis();       // timestamp of entering the function
    // keep the max time between 2 call to this function (for debugging/reporting)
    if ( (now - lastCallToHandleSerialInMs) > maxTimeBetween2HandleSerialInMs){
        maxTimeBetween2HandleSerialInMs = now - lastCallToHandleSerialInMs; 
    }
    lastCallToHandleSerialInMs = now; 
    if ((millis() - lastSerialInMs) > 5000){  // check that we got at least some valid data within 5 sec
        Serial.print("No data within "); Serial.println(millis() - lastSerialInMs);
        ledState = STATE_NO_DATA;             // when no data, we will change the led color
    }
    while (Serial2.available() ){
        serialAvailable = Serial2.available();
        if (serialAvailable > maxSerialAvailable) maxSerialAvailable = serialAvailable; // keep trace of the % of filling fifo       
        c= Serial2.read( );
        //Serial.println(c,HEX);
        if (c== 0X7E){
            // when a synchr is received after a previous valid record, then process previous group of data
            if ((tempTlmCount > 0 ) && (inState==IN_RECEIVING_TYPE)){
                
                lastSerialInMs = millis(); // reset the value to check that we got a valid packet
                // update the data in the cumulatieve table (one column for each data)
                currentTlm[0X3F] = inTime;     // update the timestamp at idx 0X3F
                for (uint8_t i=0; i<tempTlmCount; i++ ){  // update current tlm based on incoming data in 
                    uint8_t j = tempTlm[i].type;
                    if (j< 63) { 
                        currentTlm[j] = tempTlm[i].data;
                    }
                    
                }
                // detect when we get for the first time a GPS date and time
                if ( createDateTimeState == 0){
                    if (currentTlm[6] && currentTlm[7]) {
                        firstGpsDate = currentTlm[6]; // store the 2 value (to avoid race issue)
                        firstGpsTime = currentTlm[7];
                        createDateTimeState = 1;    // update the state to KNOWN but not registered
                    }
                }
                // convert current table into a csv record that is filled in a long buffer.
                // when no enogh free space, wait to write in the buffer
                fillCsvBuffer();
            }; 
            inState=IN_RECEIVING_TIME;
            stuffing = false;
            inCount=0;
            inTime=0;
            inType=0;
            inData=0;
            tempTlmCount=0;
            continue;
        }
        // 07E can not exist in the message (outside the sync)
        // Stuffing Output
        //    Byte in frame has value 0x7E is changed into 2 bytes: 0x7D 0x5E
        //    Byte in frame has value 0x7D is changed into 2 bytes: 0x7D 0x5D
        // Stuffing Input:
        //    When byte 0x7D is received, discard this byte, and the next byte is XORed with 0x20;
        if (c== 0X7D) {
            stuffing = true;
            continue;
        }
        if (stuffing) {
            c= (c ^ 020);
            stuffing = false;
        }     
        switch(inState){
            case IN_WAIT_SYNC:
                break;
            case IN_RECEIVING_TIME:
                
                inTime |= (uint32_t) c;
                if (inCount < 3){
                    inCount++;
                    inTime = inTime << 8;
                } else {
                    //Serial.print("in time "); Serial.print(inCount); Serial.print(" "); Serial.println(inTime,HEX);
                    //tempTlm[0X3F].data = inTime; // save time in last
                    //tempTlmCount++;
                    inCount=0;
                    inState=IN_RECEIVING_TYPE; 
                }
                break;
            case IN_RECEIVING_TYPE:
                // to do : remove after debug
                //if (c != 0XCB) {Serial.print("c ="); Serial.print(c,HEX); Serial.println(" ");}
                inType= c & 0X3F ; // reset the first 2 bits (= number of leading zero)
                inNb0Byte = c >> 6; //save number of leading zero
                inState=IN_RECEIVING_DATA;
                //Serial.print("in type "); Serial.print(inType); Serial.print(" 0byte="); Serial.println(inNb0Byte);
                break;
            case IN_RECEIVING_DATA:
                // when type=RC_CHANNEL_TYPE, data contains 16 X 2 bytes (2 bytes for each channel in usec) 
                if (inType == RC_CHANNEL_TYPE){
                    if ( inCount & 0X01) { // when last bit is 0, it is the first byte of RC value
                        inData = ((uint32_t) c << 8);
                    } else {
                        inData |= (uint32_t) c; //It is lower part of RC value
                        // then save the value in temporary structure (waiting for next sync byte)
                        tempTlm[tempTlmCount].type = FIRST_CHANNEL_IDX + (inCount<<2);
                        tempTlm[tempTlmCount].data = inData;
                        tempTlmCount++;  
                    }
                    inCount++;
                    if (inCount >= 32){ // we got all Rc channels data
                        inType=0;
                        inCount=0;
                        inData=0;
                        inState=IN_RECEIVING_TYPE; // expect new data (or sync byte)
                    }    
                } else {  // it is not RC channels data; then data are compressed (zero in MSB bytes are omitted and have to be added again)
                    inData |= (uint32_t) c;
                    inCount++;
                    //Serial.print("in data "); Serial.print(inCount); Serial.print(" "); Serial.println(3-inNb0Byte);
                    if ( inCount <= (3-inNb0Byte)) {
                        inData = inData << 8;
                    } else {
                        tempTlm[tempTlmCount].type = inType;
                        tempTlm[tempTlmCount].data = inData;
                        tempTlmCount++;
                        //Serial.print(inType, HEX);
                        //Serial.print(" ");
                        //Serial.println(inData,HEX);
                        // wait for next data
                        inType=0;
                        inCount=0;
                        inData=0;
                        inState=IN_RECEIVING_TYPE; //expect new data ( or sync byte )
                    }
                }
                break;
        }    
    } // end while    
  }

struct repeating_timer timer;

void setupTest(){ //test data are generated on uart0 (= SERIAL1 on arduino)
    Serial1.setTX(SERIAL1_TX_PIN); // 0, 12, 16, 28
    Serial1.begin(SERIAL_IN_BAUDRATE);
    for (uint8_t i=0; i<63 ; i++){
        fieldToAdd[i]= true;
    }
    add_repeating_timer_us(TEST_INTERVAL_US, sendTest_callback, NULL, &timer);
}

#define NBR_TEST_DATA 4
#define TEST_BUFFER_LEN  (10 + (10 * NBR_TEST_DATA))
    
uint32_t testData[NBR_TEST_DATA]= {0};
uint8_t testBuffer[TEST_BUFFER_LEN];
// = {0X7E, 0X11, 0X12, 0X13, 0X14, 
//                    0xC1 , 0X11, 
//                    0X82, 0X21, 0X22, 
//                    0X43, 0X31, 0X32, 0X33, 
//                    0X04, 0X41, 0X42, 0X43, 0X44};
uint32_t count = 0;
uint8_t testCount[256] = {0};
bool endTestCount = false; 
uint8_t testIdx = 0; 
    
void printTestCount(){
    if ((count > 256) && (endTestCount == false)){
        endTestCount = true;
        for (uint i =0; i<256 ; i++){
            Serial.print(" ");Serial.print(testCount[i]);
        }
        Serial.println(" ");
    }
}


void testBufferStuf(uint8_t c){
    // Stuffing Output
    //    Byte in frame has value 0x7E is changed into 2 bytes: 0x7D 0x5E
    //    Byte in frame has value 0x7D is changed into 2 bytes: 0x7D 0x5D    
    if ( c == 0X7E ){
        testBuffer[testIdx++]= 0X7D;
        testBuffer[testIdx++]= 0X5E;
    } else if ( c == 0X7D ){
        testBuffer[testIdx++]= 0X7D;
        testBuffer[testIdx++]= 0X5D;
    } else {
        testBuffer[testIdx++]= c;
    } 
}

#define UNUSED(x) [&x]{}()              // this is just to avoid a warning about an unused parameter

bool sendTest_callback(struct repeating_timer *t) {
    UNUSED(t);                          // this is just to avoid a warning about an unused parameter
//    printf("Repeat at %lld\n", time_us_64());
//    return true;
//}    
//
//void sendTest(){
    
    #ifdef GENERATE_TEST_UART0_TX
    //static uint32_t lastTestMicros = 0; // not used because we use now a callback with a timer
    //if (millis() < 5000) return true; // wait 5 sec before sending data on serial
    uint32_t usec = micros();
    count++;
    if (count < 256) {
        testCount[count] = count;
    }
        //Serial.print("count="); Serial.println(count);
//    if (( usec - lastTestMicros) > TEST_INTERVAL_US){
        testIdx = 0;
        testBuffer[testIdx++] = 0X7E;                // synchro byte  
        testBufferStuf((usec>> 24) & 0XFF);  // timestamp.
        testBufferStuf((usec>> 16) & 0XFF);
        testBufferStuf((usec>> 8) & 0XFF);
        testBufferStuf((usec) & 0XFF);         
        //Serial.print("i="); Serial.print(0); Serial.print("  data=");Serial.println(testData[0]);
        for (uint8_t i = 0; i<NBR_TEST_DATA; i++) {
            
            testData[i] += i+1;
            int32_t toPack = - ((int32_t)testData[i]); // to test with negative value 
            //int32_t toPack =  ((int32_t)testData[i]); // to test with positive value 
            
            //if (testData[0]<10){
            //    Serial.print(toPack); Serial.print(" ");
            //    Serial.print((toPack>>24) & 0XFF,HEX); Serial.print(" "); Serial.print((toPack>>16) & 0XFF,HEX);Serial.print(" ");
            //    Serial.print((toPack>>8) & 0XFF,HEX); Serial.print(" "); Serial.print((toPack>>0) & 0XFF,HEX);Serial.println(" ");
            //} 
            if ( toPack < 0) {
                testBufferStuf( i );                 // use i as type, 0 zero byte 
                testBufferStuf((toPack>>24) & 0XFF);
                testBufferStuf((toPack>>16) & 0XFF);
                testBufferStuf((toPack>>8) & 0XFF);
                testBufferStuf((toPack>>0) & 0XFF);
            } else if ( (toPack) >= 0X01000000 ) {
                testBufferStuf( i );                 // use i as type, 0 zero byte 
                testBufferStuf((toPack>>24) & 0XFF);
                testBufferStuf((toPack>>16) & 0XFF);
                testBufferStuf((toPack>>8) & 0XFF);
                testBufferStuf((toPack>>0) & 0XFF);         
            } else if ( (toPack) >= 0X010000 ) {
                testBufferStuf(i | (0X40));                 // use i as type, 1 zero byte 
                testBufferStuf((toPack>> 16) & 0XFF);
                testBufferStuf((toPack>> 8) & 0XFF);
                testBufferStuf((toPack>>0) & 0XFF);         
            } else if ( (toPack) >= 0X0100 ) {
                testBufferStuf(i | (0X80));                 // use i as type, 2 zero byte 
                testBufferStuf((toPack>> 8) & 0XFF);
                testBufferStuf((toPack>>0) & 0XFF);         
            } else  {
                testBufferStuf(i | (0XC0));                 // use i as type, 2 zero byte 
                testBufferStuf((toPack >> 0) & 0XFF);         
            }

        }
        //for (uint8_t j = 0 ; j< testIdx; j++){
        //    Serial.print(" ");Serial.print(testBuffer[j],HEX);
        //}
        //Serial.println(" ");    
        Serial1.write(testBuffer , testIdx); // send the buffer just like oXs would do
        //lastTestMicros = micros();
        //toggleRgb();
        //Serial.println(".");
//    }
    #endif
    return true; // repeat the timer
}

// convert a table with 64 int32 data into csv (decimals) 
// we fill the data in a circular buffer of m bytes
// we keep a pointer where to wrire the next byte
// we use a queue to communicate to the other cpu the first and number of positions to write on the sdcard (= one record)
// when sdcard has written the bytes, it sent back via another queue the last position and number of bytes being written
// the first cpu, uses this to check if his buffer is full or not  
// we apply a filter on each data to decide if it has to be included or not in the csv

// handling of circular buffer:
// we write in the buffer one csv record and a time; we assume the record can't exceed some length (e.g. 640 bytes).
// before starting writing a record we check that there is still enough space in the buffer.
// if head is > tail we also check that there is enough space up to the end of the buffer (not circular) else we go back to begin
// if tail > head, we check that there is enough space before tail

// when we write a record, we fill the time, then each "expected" data (with a "," before) and finaly a EOL

// to store data to the sd card,
// as long there are entry in the queue, we read the queue to get firstByteIdx and number of bytes
// we write all those data to the sdcard
// we send in another queue the lastByteIdx (+1) and the number of bytes being written

// in the main loop, we also read the incoming queue in order to update the readIdx and number of bytes in the buffer

char csvBuf[CSV_MAX_BUFFER_LEN];
uint32_t writeIdx = 0 ; // current idx to write the next byte in csvBuf
uint32_t readIdx = 0 ;  // current idx where Sd function should read
uint32_t bytesInBuf = 0; // number of bytes in the buffer
#define CSV_MAX_IN_BUFFER (CSV_MAX_BUFFER_LEN - CSV_MAX_RECORD_LEN)

void fillCsvBuffer(){    // wait for enough free space, then write a csv record in the buffer and inform other core; 
    //Serial.print(" fillCsvBuffer: bytesInBuf="); Serial.println(bytesInBuf);
    uint32_t usec = micros();
    static bool warningCsvFull = true; // used to send only once the message that CSV is full
    static bool warningNoSpaceBeforeReadIdx = true;
    while (true){
        if (bytesInBuf ){
            updateBytesInBuf() ; // check if Sd card has writen some bytes in order to make space
        }
        if (bytesInBuf < CSV_MAX_IN_BUFFER){ // buffer is not full
            if ( writeIdx >= readIdx) {
                if (( CSV_MAX_BUFFER_LEN -writeIdx ) > CSV_MAX_RECORD_LEN){ // There is enough space forward
                    //Serial.println("Write forward ")  ;      
                    writeRec();
                    warningCsvFull = true; 
                    warningNoSpaceBeforeReadIdx = true;
                    break;
                } else {                                                   // reset write pos
                    writeIdx=0;
                    Serial.println("Reset writeIdx")  ;      
                    continue;                                              // restart the checks
                }
            } else {   // readIdx is greater than witeIdx
                if (( readIdx - writeIdx ) > CSV_MAX_RECORD_LEN){ // There is enough space before readIdx
                    //Serial.println("Write before readIdx ")  ;      
                    writeRec();  // build and write the csv record
                    warningCsvFull = true; 
                    warningNoSpaceBeforeReadIdx = true;
                    break;
                } else {                                          // no enough space
                    if (warningNoSpaceBeforeReadIdx){
                        //Serial.println("No enough space before readIdx to write a csv record");
                        warningNoSpaceBeforeReadIdx = false;
                    }    
                }
            }
        } else {
            if (warningCsvFull){
                Serial.println("csv buffer is full");
                warningCsvFull = false;
            }    
        }
    } // end While
    usec = micros()- usec;
    if ( usec > maxFillCsvUs)maxFillCsvUs = usec;
    if (bytesInBuf > maxBytesInBuf) maxBytesInBuf = bytesInBuf; 
}

uint8_t write2Digits(uint8_t val , char * buf){
    char* _buf = buf;
    if (val >=100){   // when value is 3 digits, replace by 00
        *_buf= '0';
        _buf++;
        *_buf = '0';
    } else {
        if (val < 10) {  // when less than 10, insert a 0 first
            *_buf = '0';
            _buf++;
        }
        std::to_chars(_buf, _buf + 11, (int) val);
    }
    return 2;
}

void writeRec(){    // convert currentTlm[] (with filtering) in a csv format written in a buffer
            
    uint32_t beginIdx = writeIdx; // save the beginning of record with current position
    // write first the time stamp
    writeIdx += toString(currentTlm[0X3F], 0 , &csvBuf[writeIdx]); // format the timestamp with n decimals in buffer at position writeIdx
    // write all other requested fields (special handling for GPS long/lat, GPS date, GPS time) 
    for (uint8_t i = 0; i<63; i++){ // for each field except the timestamp (stored at index 63)
        if ((fieldToAdd[i]) && (i!=1)) {        // if the field has to be included and i!= 1 (because GPS long and lat are merged in one field)    
            csvBuf[writeIdx++] = ',';  // put comma
            switch (i){
                case 6:  // GPS date (must be in format 20YY:MM:DD)
                    if ( currentTlm[i] != 0 ) { // do not fill when data = 0 to avoid 2000:00:00
                        writeIdx += write2Digits(20 , &csvBuf[writeIdx]); // add century
                        writeIdx += write2Digits((uint8_t) (currentTlm[i]>>24) , &csvBuf[writeIdx]); // add century
                        csvBuf[writeIdx++] = ':';  // put :
                        writeIdx += write2Digits((uint8_t) (currentTlm[i]>>16) , &csvBuf[writeIdx]); // add month
                        csvBuf[writeIdx++] = ':';  // put :
                        writeIdx += write2Digits((uint8_t) (currentTlm[i]>>8) , &csvBuf[writeIdx]); // add date
                    }
                    break;
                case 7:  // GPS time (must be in format HH:MM:SS)
                    writeIdx += write2Digits((uint8_t) (currentTlm[i]>>24) , &csvBuf[writeIdx]); // add HH
                    csvBuf[writeIdx++] = ':';  // put :
                    writeIdx += write2Digits((uint8_t) (currentTlm[i]>>16) , &csvBuf[writeIdx]); // add MM
                    csvBuf[writeIdx++] = ':';  // put :
                    writeIdx += write2Digits((uint8_t) (currentTlm[i]>>8) , &csvBuf[writeIdx]); // add SS
                    break;
                case 0: // for long & lat, we use a format "XX.xxxxxx YY.yyyyyy"
                    writeIdx += toString(currentTlm[i], tlmFormat[i] , &csvBuf[writeIdx]); // fill the buffer
                    csvBuf[writeIdx++] = ' ';  // put space
                    writeIdx += toString(currentTlm[i+1], tlmFormat[i+1] , &csvBuf[writeIdx]); // fill the buffer
                    break;    
                default:
                    writeIdx += toString(currentTlm[i], tlmFormat[i] , &csvBuf[writeIdx]); // fill the buffer taking care of number of decimals
                    break;
            }
            
            
        }
    }
    
    // add CR+LF
    csvBuf[writeIdx++] = 0X0D;
    csvBuf[writeIdx++] = 0X0A;
    
    // add the from and nbr bytes to the queue
    writeQueue_t writeQueueEntry;
    writeQueueEntry.idx = beginIdx;     
    writeQueueEntry.len = (uint16_t) (writeIdx - beginIdx) ;
    bytesInBuf += writeQueueEntry.len;
    uint fromNbrQueueLevel = queue_get_level(&writeFromNbrQueue);
    if (fromNbrQueueLevel > maxFromNbrQueueLevel) maxFromNbrQueueLevel = fromNbrQueueLevel;
    if (!queue_try_add ( &writeFromNbrQueue , &writeQueueEntry)){
        Serial.println("Error add in writeFromNbrQueue"); 
    }
    //Serial.print("a csv has been written from ");
    //Serial.print(beginIdx);
    //Serial.print(" nbrBytesWritten=");
    //Serial.print(writeQueueEntry.len);
    //Serial.print(" readIdx=");
    //Serial.println(readIdx);
    totalNbBytesInCsvFile += writeQueueEntry.len ;
}

void updateBytesInBuf(){
    writeQueue_t writtenBytes;
    uint lastWrittenQueueLevel = queue_get_level(&lastWrittenQueue);
    if (lastWrittenQueueLevel > maxLastWrittenQueueLevel) maxLastWrittenQueueLevel = lastWrittenQueueLevel;
    while (! queue_is_empty(&lastWrittenQueue)){
        if (queue_try_remove(&lastWrittenQueue, &writtenBytes)) {
            //Serial.print("last written bytes at idx "); Serial.println(writtenBytes.idx);
            //Serial.print("last written bytes"); Serial.println(writtenBytes.len);
            readIdx = writtenBytes.idx ;
            bytesInBuf -=  writtenBytes.len;
        } else {
            Serial.println("Error trying to remove from lastWrittenqueue" );
        }
    }
}

void setupQueues(){
    queue_init(&writeFromNbrQueue , sizeof(writeQueue_t) ,WRITE_FROM_NBR_QUEUE_LEN) ;// queue for sbus uart with 256 elements of 1
    queue_init(&lastWrittenQueue , sizeof(writeQueue_t) ,LAST_WRITTEN_QUEUE_LEN) ;// queue for sbus uart with 256 elements of 1 

}

void writeOnSdcard(){
    // get the parameters from the queue writeFromNbrQueue
    writeQueue_t toWrite;
    writeQueue_t writtenBytes; 
    while (! queue_is_empty(&writeFromNbrQueue)){
        if (queue_try_remove(&writeFromNbrQueue, &toWrite)) {
            //Serial.print("In SD: Write from Pos :"); Serial.print((uint32_t) toWrite.idx );
            //Serial.print("   Nb bytes:"); Serial.println(toWrite.len );
            //Serial.println(" "); 
            //for (uint i=0; i<toWrite.len ; i++){
            //    Serial.print(csvBuf[i],HEX);Serial.print(" ");  
            //}
            //Serial.println(" ");
            logOnSD(toWrite.idx , toWrite.len);
            // inform core 0 that data has been written
            writtenBytes.idx = toWrite.idx + toWrite.len;
            writtenBytes.len = toWrite.len;
            if (! queue_try_add(&lastWrittenQueue, &writtenBytes)){
                Serial.println("error trying to write to lastWrittenQueue");
            }
        }	
    }// end while
}