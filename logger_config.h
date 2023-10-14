#pragma once

#define VERSION "0.0.1"

//   -------------   Serial parameter to read data from oXs (use Serial2)
#define SERIAL_IN_BAUDRATE 115200 // 230400      // 23000 bytes/sec
#define SERIAL_IN_RX_GPIO 5            // pin used to read the data from oXs (on UART1) (gpio can be 5, 9, 21, 25)
#define SERIAL_IN_FIFO_LEN (1024*16)      // Size of uart fifo that receives data from oXs


//----------------- for SD card we can use first or second SPI with following GPIO ---------------
// for first SPI (SPI),  MISO must be  0 4 16 20 ; Clock  2 6 18 22 // MOSI   3 7 19 23
// for second SPI (SPI1), MISO must be  8 12 24 28; Clock  10 14 26 // MOSI   11 15 27
// note : the board Challenger with build in sdcard uses SPI1 and gpio Clock 10, Mosi 11 , miso 12 and Cs 9

#define SPI_FOR_SD 0  // must be 0 (first spi = SPI) or 1 (second spi = SPI1)

#define SPI_MISO   4  // MISO   0 4 16 20 (SPI)   or  8 12 24 28 (SPI1) 
#define SPI_SCLK   6  // Clock  2 6 18 22 (SPI)   or  10 14 26   (SPI1) 
#define SPI_MOSI   7  // MOSI   3 7 19 23 (SPI)   or  11 15 27   (SPI1)

#define SPI_CS   2  // can  be a GPIO between 0 and 29

//----------------- oXs fields to include in the CSV log file ----------------------
// list of 64 X true (insert) or false (omit); one for each field in sequence of index (see list of index below)
#define FIELDS_TO_ADD  true , true , true , true ,true , true , true , true , true , true ,\
                       true , true , true , true ,true , true , true , true , true , true ,\
                       true , true , true , true ,true , true , true , false, false, false,\
                       false, true , true , true ,true , true , true , true , true , true ,\
                       true , true , true , true ,true , true , true , false, false, false,\
                       false, false, false, false


//---------------- Start / Stop / inteval of logging ------------------------------- 
#define MIN_INTERVAL_MS 0 // minimum interval between 2 csv records in log file (in milli sec); 0 = no minimum
#define MODE 0            // 0 = always, 1 = use MIN/MAX below to trigger the start of logging , 2= use MIN/MAX as filter to log or not
#define MIN_FIELD 56      // index of the field to be logged (see below) 
#define MIN_VALUE 100     // when MODE = 1(or 2), log starts(is done) only if the value provided by oXs for MIN_FIELD is more than this value
#define MAX_FIELD 56      // index of the field to be logged (see below) 
#define MAX_VALUE 2500    // when MODE = 1(or 2), log starts is done) only if the value provided by oXs for MAX_FIELD is less than this value
// You can e.g. use a RC channel field to decided when logging starts or is done. 
   

// --------------- Buffer to store CSV data waiting write on SD card ---------------
#define CSV_MAX_BUFFER_LEN  50000  // circular buffer used to store csv data before they are store on SD card 
#define CSV_MAX_RECORD_LEN 500  // used to check that there is enough space before writing a csv reccord


// --------------- Queues used to communicate between the 2 cores
#define WRITE_FROM_NBR_QUEUE_LEN 500 // could be calculated based on buffer len and record len
#define LAST_WRITTEN_QUEUE_LEN 500 // idem

// -------------------- parameters for CSV (header and format)
// long and lat are grouped in one csv field named Gps; so second field is just omitted
#define LIST_OF_CSV_FIELD_NAME { "Gps", " ", "Gps groundspeed (m/s)" , "Gps Heading", "Gps Altitude(m)", "Gps Num Sat" ,\
        "Gps Date", "Gps Time", "Gps PDOP", "Gps home bearing" , "Gps home dist", "Volt 1 (V)",  "Current(A)", "Volt 3(V)" , "Volt 4(V)",\
        "Capacity (Ah)", "Temp 1 " , "Temp 2", "VSpeed (m/s)" , "Baro Altitude (m)" , "Pitch" , "Roll", "Yaw" , "RPM" , \
        "ADS_1_1","ADS_1_2","ADS_1_3","ADS_1_4","ADS_2_1","ADS_2_2","ADS_2_3","ADS_2_4", "Airspeed (m/s)" , "Comp VSpeed (m/s)" ,\
        "Sbus Hold count" , "Sbus Failsafe Count",  "GPS cumul dist (m)" , "Unused 37" ,"Unused 38" ,"Unused 39" , "Unused 40" ,\
        "Rc1", "Rc2","Rc3","Rc4","Rc5","Rc6","Rc7","Rc8",\
        "Rc9", "Rc10","Rc11","Rc12","Rc13","Rc14","Rc15","Rc16",\
        "Unused 57" ,"Unused 58" ,"Unused 59" , "Unused 60", "Unused 61" ,"Unused 62" , "Time" }

// number of decimals per fields ;  0..3 number of décimals, 6 = GPS format (we first have to divide by 10 because original value has 7 digits)
#define LIST_OF_DECIMALS {6, 6, 2, 2, 2, 0,\
                        0, 0, 2, 0, 0, 3, 3, 3, 3,\
                        3, 0, 0, 2, 2, 2, 2, 2, 0,\
                        3, 3, 3, 3, 3, 3, 3, 3, 2, 2,\
                        0, 0, 0, 0, 0, 0, 0,\
                        0, 0, 0, 0, 0, 0, 0, 0,\
                        0, 0, 0, 0, 0, 0, 0, 0,\
                        0, 0, 0, 0, 0, 0, 0} 

// ------------- reporting (on USB) to check for overrun and performance issue
#define REPORT_INTERVAL_MS 20000 // interval between 2 reports ; uncomment to avoid any report


// -------------- for testing we can generate dummy data using SERIAL1
//#define GENERATE_TEST_UART0_TX   // uncomment to let UART0 (SERIAL1) generates dummy data
#define SERIAL1_TX_PIN 0  // could be Tx pin = 0, 12 ,16, 28 UART0 = Serial1
#define TEST_INTERVAL_US (1000*1000)  // used when serial data are generate by this RP2040 (for debug only) with Serial1
                             
//--------------- for info, list of index of all fields provided by oXs --------------------
/*
0 Gps latitude
1 GPS longitute
2 Gps groundspeed (m/s)
3 Gps Heading
4 Gps Altitude(m)
5 Gps Num Sat
6 Gps Date
7 Gps Time
8 Gps PDOP
9 Gps home bearing
10 Gps home dist
11 Volt 1 (V)
12 Current(A)
13 Volt 3(V)
14 Volt 4(V)
15 Capacity (Ah)
16 Temp 1 
17 Temp 2
18 VSpeed (m/s)
19 Baro Altitude (m)
20 Pitch
21 Roll
22 Yaw
23 RPM
24 ADS_1_1
25 ADS_1_2
26 ADS_1_3
27 ADS_1_4
28 ADS_2_1
29 ADS_2_2
30 ADS_2_3
31 ADS_2_4
32 Airspeed (m/s)
33 Comp VSpeed (m/s)
34 Sbus Hold count
35 Sbus Failsafe Count
36 GPS cumul dist (m)
37 Unused 37
38 Unused 38
39 Unused 39
40 Unused 40
41 Rc1 = Rc Channel value
42 Rc2
43 Rc3
44 Rc4
45 Rc5
46 Rc6
47 Rc7
48 Rc8
49 Rc9
50 Rc10
51 Rc11
52 Rc12
53 Rc13
54 Rc14
55 Rc15
56 Rc16
57 Unused 57
58 Unused 58
59 Unused 59
60 Unused 60
61 Unused 61
62 Unused 62
*/


// ----------- for info on RP2040
// Rx pin = 1, 13, 17,29 (UART0) = Serial1
// Tx pin = 0, 12 ,16, 28 UART0 = Serial1

// RX pin  = 5, 9, 21, 25  (UART1)= Serial2
// TX pin  = 4, 8, 20, 24  (UART1)= Serial2


