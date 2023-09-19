#line 1 "c:\\Data\\oXs_logger_arduino\\logger_config.h"
#pragma once

#define VERSION "0.0.0"

// Serial parameter reading data from oXs (use Serial2)
#define SERIAL_IN_BAUDRATE 115200 // 230400      // 23000 bytes/sec
#define SERIAL_IN_RX_GPIO 9            // pin used to read the data
#define SERIAL_IN_FIFO_LEN (1024*16)      // Size of uart fifo

// for testing we can generate dummu data using SERIAL1
#define GENERATE_TEST_UART0_TX
#define SERIAL1_TX_PIN 0
    
// Rx pin = 1, 13, 17,29 (UART0) 
// Tx pin = 0, 12 ,16, 28 UART0 = serial1

// RX pin  = 5, 9, 21, 25  (UART1)= serial2
// TX pin  = 4, 8, 20, 24  (UART1)= serial2

// SPI uses pin:
// CS 5 // could be changed

#define CSV_MAX_BUFFER_LEN  50000
#define CSV_MAX_RECORD_LEN 500

//----------------- for SPI
#define SPI_RX   4  // MISO   0 4 16 20
#define SPI_CS   5  // for CS 1 5 17 21
#define SPI_SCK  6  // Clock  2 6 18 22
#define SPI_TX   7  // MOSI   3 7 19 23


#define WRITE_FROM_NBR_QUEUE_LEN 500 // could be calculated based on buffer len and record len
#define LAST_WRITTEN_QUEUE_LEN 500 // idem


#define TEST_INTERVAL_US (1000*1000)

// long and lat are grouped in one csv field named Gps 
#define LIST_OF_CSV_FIELD_NAME { "Gps", " ", "Gps groundspeed (m/s)" , "Gps Heading", "Gps Altitude(m)", "Gps Num Sat" ,\
        "Gps Date", "Gps Time", "Gps PDOP", "Gps home bearing" , "Gps home dist", "Volt 1 (V)",  "Current(A)", "Volt 3(V)" , "Volt 4(V)",\
        "Capacity (Ah)", "Temp 1 " , "Temp 2", "VSpeed (m/s)" , "Baro Altitude (m)" , "Pitch" , "Roll", "Yaw" , "RPM" , \
        "ADS_1_1","ADS_1_2","ADS_1_3","ADS_1_4","ADS_2_1","ADS_2_2","ADS_2_3","ADS_2_4", "Airspeed (m/s)" , "Comp VSpeed (m/s)" ,\
        "Sbus Hold count" , "Sbus Failsafe Count",  "GPS cumul dist (m)" , "Unused 37" ,"Unused 38" ,"Unused 39" , "Unused 40" ,\
        "Rc1", "Rc2","Rc3","Rc4","Rc5","Rc6","Rc7","Rc8",\
        "Rc9", "Rc10","Rc11","Rc12","Rc13","Rc14","Rc15","Rc16",\
        "Unused 57" ,"Unused 58" ,"Unused 59" , "Unused 60", "Unused 61" ,"Unused 62" , "Unused 63" }

// number of decimals per fields ;  0..3 number of décimals, 6 = GPS format (we first have to divide by 10 because original value has 7 digits)
#define LIST_OF_DECIMALS {6, 6, 2, 2, 2, 0,\
                        0, 0, 2, 0, 0, 3, 3, 3, 3,\
                        3, 0, 0, 2, 2, 2, 2, 2, 0,\
                        3, 3, 3, 3, 3, 3, 3, 3, 2, 2,\
                        0, 0, 0, 0, 0, 0, 0,\
                        0, 0, 0, 0, 0, 0, 0, 0,\
                        0, 0, 0, 0, 0, 0, 0, 0,\
                        0, 0, 0, 0, 0, 0, 0} 